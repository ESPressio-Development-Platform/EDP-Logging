#pragma once

#include <cstddef>

#include <ESPressio_Clock.hpp>
#include <ESPressio_Threading.hpp>

namespace ESPressio::Logging::Detail {

    /// Stateless view of one Bootstrap-provisioned EDP-Threading wait/wake slot.
    ///
    /// Synchronization storage and canonical monotonic waiting remain entirely
    /// owned by the supplied EDP-Threading provider. Logging retains only the
    /// caller's exclusive slot identity; no second Signal, waiter pool, or
    /// independent clock mechanism is introduced here.
    template<class TWaitWakeProvider>
    struct ThreadingWaitSlot final {
        TWaitWakeProvider* WaitWake;
        std::size_t Slot;

        [[nodiscard]] ESPressio::Threading::BoundedWaitResult WaitUntil(
            ESPressio::Clock::MonotonicTimestamp deadline
        ) noexcept {
            switch (WaitWake->WaitUntil(Slot, deadline)) {
                case ESPressio::Threading::BoundedWaitWakeResult::Woken:
                    return ESPressio::Threading::BoundedWaitResult::Woken;

                case ESPressio::Threading::BoundedWaitWakeResult::TimedOut:
                    return ESPressio::Threading::BoundedWaitResult::TimedOut;

                default:
                    return ESPressio::Threading::BoundedWaitResult::ProviderFailure;
            }
        }
    };

} // namespace ESPressio::Logging::Detail
