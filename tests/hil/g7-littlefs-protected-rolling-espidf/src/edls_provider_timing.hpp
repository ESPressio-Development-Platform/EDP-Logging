#pragma once
// Test-only nonallocating instrumentation of ACTUAL EDP-Persistence
// providers. Does not change EDLS or its durability / ACK ordering.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <esp_timer.h>
#include <logging/EdlsPowerLossAcknowledgement.hpp>

namespace {
enum class ProfileOp : unsigned {
    FileSize,FileRead,FileReplace,FileRemove,
    NvsSize,NvsRead,NvsStore,NvsRemove,Count
};
static constexpr std::size_t ProfileOpCount =
    static_cast<std::size_t>(ProfileOp::Count);
static constexpr std::array<const char*,ProfileOpCount> ProfileNames{{
    "file_size","file_read","file_replace","file_remove",
    "nvs_size","nvs_read","nvs_store","nvs_remove"}};

struct ProfileStat {
    std::uint64_t Count{0U};
    std::uint64_t TimeUs{0U};
    std::uint64_t MaxUs{0U};
};
struct ProfileSample {
    std::uint64_t Count{0U};
    std::uint64_t TotalUs{0U};
};
struct ProfileRecorder {
    std::array<ProfileStat,ProfileOpCount> Ops{};
    std::array<std::array<ProfileSample,ProfileOpCount>,256U> Samples{};
    std::uint32_t Generation{0U};
    std::uint32_t ErrorCount{0U};
    bool Enabled{false};
    void Reset() noexcept {
        Ops={};
        Samples={};
        Generation=0U;
        ErrorCount=0U;
        Enabled=false;
    }
    void Record(ProfileOp op,std::int64_t elapsedUs) noexcept {
        if(!Enabled)return;
        if(elapsedUs<0) {++ErrorCount;return;}
        auto& item=Ops[static_cast<std::size_t>(op)];
        ++item.Count;
        item.TimeUs+=static_cast<std::uint64_t>(elapsedUs);
        if(static_cast<std::uint64_t>(elapsedUs)>item.MaxUs)
            item.MaxUs=static_cast<std::uint64_t>(elapsedUs);
    }
    void FinishGeneration(std::uint32_t generation) noexcept {
        Generation=generation;
        for(std::size_t i=0U;i<ProfileOpCount;++i) {
            Samples[generation-1U][i]={
                Ops[i].Count,Ops[i].TimeUs};
        }
    }
};
static ProfileRecorder ProfileLog{};

class ProfileDuration {
    ProfileOp type_;
    std::int64_t started_;
public:
    explicit ProfileDuration(ProfileOp type) noexcept :
        type_(type), started_(esp_timer_get_time()) {}
    ~ProfileDuration() {
        ProfileLog.Record(type_,esp_timer_get_time()-started_);
    }
    ProfileDuration(const ProfileDuration&)=delete;
    ProfileDuration& operator=(const ProfileDuration&)=delete;
};

template<class TProvider>
struct ProfileFileStorage {
    using ProviderDeclarationTag=typename TProvider::ProviderDeclarationTag;
    using CompositionDomain=typename TProvider::CompositionDomain;
    using CompositionOffers=typename TProvider::CompositionOffers;
    TProvider& Actual;
    explicit ProfileFileStorage(TProvider& source) noexcept : Actual(source) {}
    [[nodiscard]] bool IsFileStorageReady() const noexcept {
        return Actual.IsFileStorageReady();
    }
    [[nodiscard]] P::FileSizeResult GetFileSize(
        P::FilePathView path) const noexcept {
        ProfileDuration duration(ProfileOp::FileSize);
        return Actual.GetFileSize(path);
    }
    [[nodiscard]] P::FileReadResult ReadFileAt(
        P::FilePathView path,P::StorageOffset offset,
        P::DestinationBufferView target) const noexcept {
        ProfileDuration duration(ProfileOp::FileRead);
        return Actual.ReadFileAt(path,offset,target);
    }
    [[nodiscard]] P::FileReplaceStatus ReplaceFile(
        P::FilePathView path,P::SourceBufferView source) noexcept {
        ProfileDuration duration(ProfileOp::FileReplace);
        return Actual.ReplaceFile(path,source);
    }
    [[nodiscard]] P::FileRemoveStatus RemoveFile(
        P::FilePathView path) noexcept {
        ProfileDuration duration(ProfileOp::FileRemove);
        return Actual.RemoveFile(path);
    }
};
template<class TProvider>
struct ProfileKeyValue {
    using ProviderDeclarationTag=typename TProvider::ProviderDeclarationTag;
    using CompositionDomain=typename TProvider::CompositionDomain;
    using CompositionOffers=typename TProvider::CompositionOffers;
    TProvider& Actual;
    explicit ProfileKeyValue(TProvider& source) noexcept : Actual(source) {}
    [[nodiscard]] bool IsKeyValueStorageReady() const noexcept {
        return Actual.IsKeyValueStorageReady();
    }
    [[nodiscard]] P::KeyValueSizeResult GetValueSize(
        P::KeyView key) const noexcept {
        ProfileDuration duration(ProfileOp::NvsSize);
        return Actual.GetValueSize(key);
    }
    [[nodiscard]] P::KeyValueReadResult ReadValue(
        P::KeyView key,P::DestinationBufferView target) const noexcept {
        ProfileDuration duration(ProfileOp::NvsRead);
        return Actual.ReadValue(key,target);
    }
    [[nodiscard]] P::KeyValueStoreStatus StoreValue(
        P::KeyView key,P::SourceBufferView source) noexcept {
        ProfileDuration duration(ProfileOp::NvsStore);
        return Actual.StoreValue(key,source);
    }
    [[nodiscard]] P::KeyValueRemoveStatus RemoveKey(
        P::KeyView key) noexcept {
        ProfileDuration duration(ProfileOp::NvsRemove);
        return Actual.RemoveKey(key);
    }
    // NVS advertises ClearAllSupport::Supported; retain the complete
    // mandatory provider capability contract even though this HIL never calls it.
    [[nodiscard]] P::KeyValueClearStatus ClearAllKeys() noexcept {
        return Actual.ClearAllKeys();
    }
};
} // native HIL only
