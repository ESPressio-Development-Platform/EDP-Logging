idf_component_register(SRCS "main.cpp" INCLUDE_DIRS "." REQUIRES nvs_flash esp_hw_support joltwallet__littlefs)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
