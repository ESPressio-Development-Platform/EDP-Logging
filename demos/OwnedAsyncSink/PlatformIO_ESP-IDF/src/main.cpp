#include "../../Arduino_IDE/src/AsyncDemo.hpp"

#include <cstdio>

/// Runs one application-owned copying Task sink and reports its final lifecycle result.
extern "C" void app_main() {
    const auto result = ESPressio::Demos::OwnedAsyncSink::Run();
    std::printf(
        "EDP_LOGGING_ASYNC_DEMO|RESULT|%u\n",
        static_cast<unsigned>(result)
    );
}
