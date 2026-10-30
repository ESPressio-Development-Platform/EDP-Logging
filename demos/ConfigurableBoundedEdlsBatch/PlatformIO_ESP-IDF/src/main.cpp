#include "../../Arduino_IDE/src/BatchDemo.hpp"

/// Runs this finite Bootstrap application on the ESP-IDF main task.
extern "C" void app_main() {
    const auto result = ESPressio::Demos::ConfigurableBoundedEdlsBatch::Run();
    if (result != ESPressio::Demos::ConfigurableBoundedEdlsBatch::Result::Completed) {
        std::printf(
            "EDP_BATCH_DEMO|FAIL|result=%u\n",
            static_cast<unsigned>(result)
        );
    }
}
