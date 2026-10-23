#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>
#include <logging/PowerLossEdlsSnapshots.hpp>
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
        std::array<std::array<std::byte,512U>,2U> bytes{};
        std::array<std::size_t,2U> sizes{};
        bool failStore{false};
        bool failRead{false};
        bool tornValue{false};
        std::size_t stores{0U};
        [[nodiscard]] bool IsKeyValueStorageReady() const noexcept {return true;}
        [[nodiscard]] std::size_t Which(P::KeyView key) const noexcept {
            return key.Data()[0]=='a' ? 0U : 1U;
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
        assert(L::WriteEdlgFrame(L::EdlgCodec::Cbor,payload,encoded,len,64U)
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
}
int main(){Run();}
