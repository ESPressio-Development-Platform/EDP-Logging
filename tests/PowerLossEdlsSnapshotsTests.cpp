#include <logging/EdlgFrameWriter.hpp>
#include <ESPressio_Platform_Portable_ByteOperations.hpp>

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>
#include <logging/PowerLossEdlsSnapshots.hpp>
#include <logging/PowerLossEdlsJournal.hpp>
#include <logging/EdlgFrame.hpp>

namespace {
    namespace L=ESPressio::Logging;
    namespace P=ESPressio::Persistence;
    namespace F=ESPressio::Persistence::Composition;
    // Invalid provider configurations are compiled by the negative-test
    // driver and must be rejected by the production template itself.
#if defined(EDP_TEST_WEAK_RETENTION)
    constexpr auto TestedRetention=P::RetentionLevel::Restart;
    constexpr auto TestedAtomicity=P::InterruptionAtomicity::Restart;
#elif defined(EDP_TEST_WEAK_ATOMICITY)
    constexpr auto TestedRetention=P::RetentionLevel::PowerLoss;
    constexpr auto TestedAtomicity=P::InterruptionAtomicity::Restart;
#else
    constexpr auto TestedRetention=P::RetentionLevel::PowerLoss;
    constexpr auto TestedAtomicity=P::InterruptionAtomicity::PowerLoss;
#endif
    using Offer=F::Offer<P::KeyValueStorage,
        F::PropertyValue<P::KeyValueAccessMode,P::AccessMode::ReadWrite>,
        F::PropertyValue<P::KeyValueRetention,TestedRetention>,
        F::PropertyValue<P::KeyCaseSensitivity,P::TextCaseSensitivity::CaseSensitive>,
        F::PropertyValue<P::KeyValueMediaRemovability,P::MediaRemovability::Fixed>,
        F::PropertyValue<P::MaximumKeyBytes,std::size_t{15U}>,
        F::PropertyValue<P::MaximumKeyValueSize,P::StorageSize{512U}>,
        F::PropertyValue<P::KeyEnumerationSupport,P::Support::Unsupported>,
        F::PropertyValue<P::ReadValueAtSupport,P::Support::Unsupported>,
        F::PropertyValue<P::ClearAllSupport,P::Support::Unsupported>,
        F::PropertyValue<P::KeyValueCapacityReportingSupport,P::Support::Unsupported>,
        F::PropertyValue<P::KeyValueInvocationConcurrency,P::InvocationConcurrency::CallerSerialized>,
        F::PropertyValue<P::KeyValueFailurePreservation,P::FailurePreservation::MayModify>,
        F::PropertyValue<P::KeyValueInterruptionAtomicity,TestedAtomicity>
    >;
    struct FakeStorage final : F::Provider<P::Domain,F::Offers<Offer>> {
        std::array<std::array<std::byte,512U>,8U> bytes{};
        std::array<std::size_t,8U> sizes{};
        bool failStore{false};
        bool failRead{false};
        bool tornValue{false};
        /// Models a provider accepting a write while returning different valid bytes.
        bool alterStoredIncarnation{false};
        std::size_t stores{0U};
        [[nodiscard]] bool IsKeyValueStorageReady() const noexcept {return true;}
        [[nodiscard]] std::size_t Which(P::KeyView key) const noexcept {
            return static_cast<std::size_t>(key.Data()[0]-'a');
        }
        [[nodiscard]] P::KeyValueSizeResult GetValueSize(P::KeyView key) const noexcept {
            const auto i=Which(key);
            if(sizes[i]==0U)return {P::KeyValueSizeStatus::NotFound,{}};
            return {P::KeyValueSizeStatus::Succeeded,P::StorageSize{sizes[i]}};
        }
        [[nodiscard]] P::KeyValueReadResult ReadValue(
            P::KeyView key,P::DestinationBufferView out
        ) const noexcept {
            if(failRead)return {P::KeyValueReadStatus::IoFailure,0U,0U,{}};
            const auto i=Which(key);
            if(sizes[i]==0U)return {P::KeyValueReadStatus::NotFound,0U,0U,{}};
            const auto count=sizes[i]<out.Capacity?sizes[i]:out.Capacity;
            for(std::size_t j=0U;j<count;++j)
                static_cast<std::byte*>(out.Address)[j]=bytes[i][j];
            return {P::KeyValueReadStatus::Succeeded,
                static_cast<std::uint8_t>(P::ReadFact::AvailableDataSizeIsKnown),
                count,P::StorageSize{sizes[i]}};
        }
        [[nodiscard]] P::KeyValueStoreStatus StoreValue(
            P::KeyView key,P::SourceBufferView source
        ) noexcept {
            ++stores;
            const auto i=Which(key);
            if(source.Size>512U)return P::KeyValueStoreStatus::ValueTooLarge;
            if(failStore) {
                if(tornValue) {
                    sizes[i]=10U;
                    for(std::size_t j=0U;j<10U;++j)
                        bytes[i][j]=static_cast<const std::byte*>(source.Address)[j];
                }
                return P::KeyValueStoreStatus::IoFailure;
            }
            for(std::size_t j=0U;j<source.Size;++j)
                bytes[i][j]=static_cast<const std::byte*>(source.Address)[j];
            sizes[i]=source.Size;
            if (alterStoredIncarnation) {
                L::EdlsHeader header{};
                assert(L::ReadEdlsHeader(
                    std::span<const std::byte>(bytes[i]).first(L::EdlsHeaderBytes),
                    header
                ) == L::EdlsHeaderStatus::Succeeded);
                ++header.RuntimeIncarnation;
                assert(L::WriteEdlsHeader(
                    header,
                    bytes[i]
                ) == L::EdlsHeaderStatus::Succeeded);
            }
            return P::KeyValueStoreStatus::Succeeded;
        }
        [[nodiscard]] P::KeyValueRemoveStatus RemoveKey(P::KeyView key) noexcept {
            sizes[Which(key)]=0U;
            return P::KeyValueRemoveStatus::Succeeded;
        }
    };

    void Run() {
        constexpr auto a=P::KeyView::Validate("a");
        constexpr auto b=P::KeyView::Validate("b");
        static_assert(a.IsValuePresent && b.IsValuePresent);
        FakeStorage storage;
        using Archive=L::PowerLossEdlsSnapshots<2U,256U,64U,FakeStorage>;
        Archive alias(storage,{{a.Value,a.Value}});
        assert(alias.Inspect().Status==L::DurableEdlsStatus::Ambiguous);
        Archive archive(storage,{{a.Value,b.Value}});
        auto state=archive.Inspect();
        assert(state.Status==L::DurableEdlsStatus::Empty);
        std::array<std::byte,16U> id{};
        id.fill(std::byte{0xAD});
        std::array<std::byte,64U> encoded{};
        constexpr std::array<std::byte,3U> payload{
            std::byte{0xA1},std::byte{0x01},std::byte{0x02}
        };
        std::size_t len{};
        assert(L::WriteEdlgFrame(L::EdlgCodec::Cbor,payload,encoded,len,64U,
        ESPressio::Platform::Portable::Memory::ByteOperationsProvider{})
            ==L::EdlgFrameWriteResult::Succeeded);
        const auto frame=std::span<const std::byte>(encoded.data(),len);
        auto outcome=archive.Commit(id,1U,frame);
        assert(outcome.Status==L::DurableEdlsStatus::Committed &&
            outcome.Generation==1U && outcome.Slot==0U);
        state=archive.Inspect();
        assert(state.Status==L::DurableEdlsStatus::Clean &&
            state.Generation==1U && state.NewestSlot==0U);
        outcome=archive.Commit(id,2U,frame);
        assert(outcome.Status==L::DurableEdlsStatus::Committed &&
            outcome.Generation==2U && outcome.Slot==1U);
        outcome=archive.Commit(id,3U,frame);
        assert(outcome.Status==L::DurableEdlsStatus::Committed &&
            outcome.Generation==3U && outcome.Slot==0U);
        std::array<std::byte,128U> recovered{};
        std::size_t used=1000U;
        state=archive.RecoverNewest(recovered,used);
        assert(state.Status==L::DurableEdlsStatus::Clean &&
            state.Generation==3U && used==len);
        for(std::size_t i=0U;i<len;++i)assert(recovered[i]==encoded[i]);
        auto wrong=id;
        wrong[0]=std::byte{0x10};
        const auto oldCalls=storage.stores;
        outcome=archive.Commit(wrong,4U,frame);
        assert(outcome.Status==L::DurableEdlsStatus::Ambiguous &&
            storage.stores==oldCalls);
        encoded[17U]^=std::byte{0x40};
        outcome=archive.Commit(id,4U,std::span<const std::byte>(encoded.data(),len));
        assert(outcome.Status==L::DurableEdlsStatus::InvalidRecord);
        encoded[17U]^=std::byte{0x40};

        // A valid but different stored header must fail exact commit readback.
        {
            FakeStorage differingStorage{};
            Archive differingArchive(differingStorage, {{a.Value, b.Value}});
            differingStorage.alterStoredIncarnation = true;
            const auto mismatch = differingArchive.Commit(
                id,
                1U,
                frame
            );
            assert(mismatch.Native == P::KeyValueStoreStatus::Succeeded);
            assert(mismatch.Status == L::DurableEdlsStatus::ReadbackUnconfirmed);
        }

        // A failed in-progress write may damage the older inactive key.
        // The protected newest generation remains independently recoverable.
        storage.failStore=true;
        storage.tornValue=true;
        outcome=archive.Commit(id,4U,frame);
        assert(outcome.Status==L::DurableEdlsStatus::StorageRejected);
        state=archive.RecoverNewest(recovered,used);
        assert(state.Status==L::DurableEdlsStatus::Degraded &&
            state.Generation==3U && used==len);
        outcome=archive.Commit(id,5U,frame);
        assert(outcome.Status==L::DurableEdlsStatus::Degraded);
        // Fail closed until a dedicated owner reviews the damaged key.
        storage.failStore=false;
        storage.failRead=true;
        state=archive.Inspect();
        assert(state.Status==L::DurableEdlsStatus::ProviderFailure);
        storage.failRead=false;
    }
    /// Tests a strict N+1-key journal with two protected historical records
    /// and a sacrificial slot across both successful and torn appends.
    void RunJournal() {
        constexpr auto A=P::KeyView::Validate("a");
        constexpr auto B=P::KeyView::Validate("b");
        constexpr auto C=P::KeyView::Validate("c");
        static_assert(A.IsValuePresent && B.IsValuePresent && C.IsValuePresent);
        using Journal=L::PowerLossEdlsJournal<3U,256U,64U,FakeStorage>;
        FakeStorage provider{};
        Journal journal(provider,{{A.Value,B.Value,C.Value}});
        auto current=journal.Inspect();
        assert(current.Status==L::DurableEdlsStatus::Empty);
        assert(current.RetainedCount==0U);
        std::array<std::byte,16U> identity{};
        identity.fill(std::byte{0xB2U});
        std::array<std::byte,64U> framed{};
        std::array<std::byte,100U> destination{};
        for(std::uint32_t generation=1U;generation<=5U;++generation) {
            const std::array<std::byte,3U> payload{
                std::byte{0x83U},
                std::byte{0x02U},
                std::byte{static_cast<std::uint8_t>(generation)}
            };
            std::size_t used{};
            assert(L::WriteEdlgFrame(
                L::EdlgCodec::Cbor,
                payload,
                framed,
                used,
                64U,
                ESPressio::Platform::Portable::Memory::ByteOperationsProvider{}
            )==L::EdlgFrameWriteResult::Succeeded);
            const auto committed=journal.Append(
                identity,
                generation,
                std::span<const std::byte>(framed.data(),used)
            );
            assert(committed.Status==L::DurableEdlsStatus::Committed);
            assert(committed.Generation==generation);
            current=journal.Inspect();
            assert(current.Status==L::DurableEdlsStatus::Clean);
            assert(current.Newest==generation);
            const auto count=(generation<2U)
                ? static_cast<std::size_t>(generation)
                : 2U;
            assert(current.RetainedCount==count);
            assert(current.OldestRetained==generation-count+1U);
            // Every still-retained generation remains separately replayable,
            // including across replacement of the sacrificial oldest slot.
            for(std::uint64_t gen=current.OldestRetained;
                gen<=current.Newest;++gen) {
                const auto recovered=journal.RecoverRetained(
                    gen,
                    destination
                );
                assert(recovered.Status==L::DurableEdlsStatus::Clean);
                L::EdlgFrameView frame{};
                assert(L::ReadEdlgFrame(
                    std::span<const std::byte>(
                        destination.data(),
                        recovered.FrameBytes
                    ),
                    frame,
                    64U
                )==L::EdlgFrameReadResult::Succeeded);
                assert(frame.Payload[2U]==std::byte{
                    static_cast<std::uint8_t>(gen)
                });
            }
        }
        assert(current.OldestRetained==4U&&current.Newest==5U);
        const auto retired=journal.RecoverRetained(3U,destination);
        assert(retired.Status==L::DurableEdlsStatus::InvalidRecord);
        assert(retired.FrameBytes==0U);

        const auto beforeFailedWrite=provider.stores;
        provider.failStore=true;
        provider.tornValue=true;
        const auto rejected=journal.Append(
            identity,
            6U,
            std::span<const std::byte>(framed.data(),23U)
        );
        assert(rejected.Status==L::DurableEdlsStatus::StorageRejected);
        assert(provider.stores==beforeFailedWrite+1U);
        current=journal.Inspect();
        assert(current.Status==L::DurableEdlsStatus::Degraded);
        assert(current.OldestRetained==4U&&current.Newest==5U);
        assert(current.RetainedCount==2U);
        // A damaged sacrificial slot must NOT make its two protected records
        // unrecoverable, but it must preclude any further archive mutation.
        for(std::uint64_t gen=4U;gen<=5U;++gen) {
            const auto retained=journal.RecoverRetained(gen,destination);
            assert(retained.Status==L::DurableEdlsStatus::Clean);
            assert(retained.FrameBytes==23U);
        }
        provider.failStore=false;
        const auto storesAtFailure=provider.stores;
        const auto blocked=journal.Append(
            identity,
            6U,
            std::span<const std::byte>(framed.data(),23U)
        );
        assert(blocked.Status==L::DurableEdlsStatus::Degraded);
        assert(provider.stores==storesAtFailure);

        // The same canonical EDLS format also detects duplicate generations
        // in non-newest keys; checking only newest would miss this corruption.
        FakeStorage collisionProvider{};
        Journal collision(collisionProvider,{{A.Value,B.Value,C.Value}});
        for(std::uint32_t g=1U;g<=3U;++g) {
            const auto ok=collision.Append(
                identity,
                g,
                std::span<const std::byte>(framed.data(),23U)
            );
            assert(ok.Status==L::DurableEdlsStatus::Committed);
        }
        L::EdlsHeader duplicate{};
        duplicate.Slot=0U;
        duplicate.Generation=2U;
        duplicate.DeviceIdentifier=identity;
        duplicate.RuntimeIncarnation=1U;
        assert(L::WriteEdlsHeader(
            duplicate,
            std::span<std::byte>(
                collisionProvider.bytes[0U].data(),
                L::EdlsHeaderBytes
            )
        )==L::EdlsHeaderStatus::Succeeded);
        assert(collision.Inspect().Status==L::DurableEdlsStatus::Ambiguous);

        Journal aliases(collisionProvider,{{A.Value,A.Value,C.Value}});
        assert(aliases.Inspect().Status==L::DurableEdlsStatus::Ambiguous);

        // Maximum current NVS reference capacity: eight physical slots
        // protect seven simultaneously replayable, independent records.
        constexpr auto D=P::KeyView::Validate("d");
        constexpr auto E=P::KeyView::Validate("e");
        constexpr auto FKey=P::KeyView::Validate("f");
        constexpr auto G=P::KeyView::Validate("g");
        constexpr auto H=P::KeyView::Validate("h");
        static_assert(D.IsValuePresent && E.IsValuePresent &&
            FKey.IsValuePresent && G.IsValuePresent && H.IsValuePresent);
        FakeStorage largeProvider{};
        using Larger=L::PowerLossEdlsJournal<8U,256U,64U,FakeStorage>;
        Larger larger(largeProvider,{{
            A.Value,B.Value,C.Value,D.Value,E.Value,FKey.Value,
            G.Value,H.Value
        }});
        for(std::uint32_t gen=1U;gen<=18U;++gen) {
            const std::array<std::byte,3U> payload{
                std::byte{0x83U},std::byte{0x01U},
                std::byte{static_cast<std::uint8_t>(gen)}
            };
            std::size_t len{};
            assert(L::WriteEdlgFrame(
                L::EdlgCodec::Cbor,payload,framed,len,64U,
                ESPressio::Platform::Portable::Memory::ByteOperationsProvider{}
            )==L::EdlgFrameWriteResult::Succeeded);
            assert(larger.Append(
                identity,gen,
                std::span<const std::byte>(framed.data(),len)
            ).Status==L::DurableEdlsStatus::Committed);
            const auto scanned=larger.Inspect();
            assert(scanned.Status==L::DurableEdlsStatus::Clean);
            const auto n=gen<7U ? static_cast<std::size_t>(gen) : 7U;
            assert(scanned.RetainedCount==n);
            assert(scanned.Newest==gen);
            assert(scanned.OldestRetained==gen-n+1U);
        }
        const auto retained=larger.Inspect();
        assert(retained.OldestRetained==12U&&retained.Newest==18U);
        for(std::uint64_t gen=12U;gen<=18U;++gen) {
            const auto replay=larger.RecoverRetained(gen,destination);
            assert(replay.Status==L::DurableEdlsStatus::Clean);
            L::EdlgFrameView decoded{};
            assert(L::ReadEdlgFrame(
                std::span<const std::byte>(destination.data(),replay.FrameBytes),
                decoded,64U
            )==L::EdlgFrameReadResult::Succeeded);
            assert(decoded.Payload[2U]==std::byte{
                static_cast<std::uint8_t>(gen)
            });
        }
        assert(larger.RecoverRetained(11U,destination).Status==
            L::DurableEdlsStatus::InvalidRecord);
    }
}
int main(){Run();RunJournal();}
