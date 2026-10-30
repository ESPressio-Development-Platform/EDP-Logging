#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include "src/BatchDemo.hpp"

/// Executes the finite example after Arduino's ordinary platform startup.
void setup() {
    const auto result = ESPressio::Demos::ConfigurableBoundedEdlsBatch::Run();
    if (result != ESPressio::Demos::ConfigurableBoundedEdlsBatch::Result::Completed) {
        std::printf(
            "EDP_BATCH_DEMO|FAIL|result=%u\n",
            static_cast<unsigned>(result)
        );
    }
}

/// No pending records remain after the example's explicit final service.
void loop() {
    delay(1000U);
}
