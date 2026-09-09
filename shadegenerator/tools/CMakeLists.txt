add_library(shadegenerator-tools STATIC
    collector/collector.cpp
    collector/detail.cpp
    collector/type.cpp
    collector/class.cpp
    collector/atomic.cpp
    bitfield.cpp
    dependencies.cpp
    sort.cpp
)

target_include_directories(shadegenerator-tools PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(shadegenerator-tools
    PUBLIC shadegenerator-sdk
    PRIVATE shadegenerator-warnings
)

target_compile_definitions(shadegenerator-tools PUBLIC
    NOMINMAX
)
