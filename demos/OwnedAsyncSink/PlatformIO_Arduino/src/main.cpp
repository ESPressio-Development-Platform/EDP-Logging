#include <Arduino.h>

#include "../../Arduino_IDE/src/AsyncDemo.hpp"

/// Runs the example once from the ordinary Arduino Bootstrap task.
void setup() {
    Serial.begin(115200U);
    const auto result = ESPressio::Demos::OwnedAsyncSink::Run();
    Serial.printf(
        "EDP_LOGGING_ASYNC_DEMO|RESULT|%u\n",
        static_cast<unsigned>(result)
    );
}

/// Leaves the completed example idle while preserving any uncertain static owners.
void loop() {
    delay(1000U);
}
