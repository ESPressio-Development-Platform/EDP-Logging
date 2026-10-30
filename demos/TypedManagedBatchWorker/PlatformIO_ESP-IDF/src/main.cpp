#include "../../Arduino_IDE/src/WorkerDemo.hpp"

/// Runs one existing managed Logging coordinator and finite Bootstrap producer.
extern "C" void app_main() {
    ESPressio::Demos::TypedManagedBatchWorker::Run();
}
