// Compile-fail regression: zero or nine sinks are not valid G10 profiles.
#include <logging/RecordProduction.hpp>

#ifndef EDP_TEST_SINK_COUNT
#error EDP_TEST_SINK_COUNT must be supplied by the test driver
#endif

ESPressio::Logging::RecordFanoutOutcome<EDP_TEST_SINK_COUNT> InvalidOutcome{};
