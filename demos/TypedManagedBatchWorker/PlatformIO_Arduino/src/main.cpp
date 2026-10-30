#include "../../Arduino_IDE/src/WorkerDemo.hpp"
#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>

/// Executes the finite typed worker composition from ordinary Arduino Bootstrap.
void setup() {
    ESPressio::Demos::TypedManagedBatchWorker::Run();
}

/// Run retains ownership until confirmed shutdown and then remains idle.
void loop() {
    delay(1000U);
}
