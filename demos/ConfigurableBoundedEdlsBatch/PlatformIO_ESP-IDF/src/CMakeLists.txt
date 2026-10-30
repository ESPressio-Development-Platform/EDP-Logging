idf_component_register(SRCS "main.cpp" INCLUDE_DIRS "." REQUIRES nvs_flash esp_hw_support joltwallet__littlefs)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
target_include_directories(${COMPONENT_LIB} PRIVATE
    "../../../../src"
    "../../../../../EDP-Persistence/src"
    "../../../../../EDP-Persistence-ESP-IDF/src"
    "../../../../../EDP-Memory/src"
    "../../../../../EDP-Platform/src"
    "../../../../../EDP-Platform-Portable/src"
    "../../../../../EDP-System/src"
    "../../../../../EDP-BoundedTopology/src"
    "../../../../../EDP-BoundedTypes/src"
    "../../../../../EDP-Serialisation/src"
    "../../../../../EDP-Localisation/src"
    "../../../../../EDP-Clock/src"
    "../../../../../EDP-Platform-ESP-IDF/src"
)
