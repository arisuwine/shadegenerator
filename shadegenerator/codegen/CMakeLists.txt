add_library(shadegenerator-codegen STATIC
    cpp_emitter.cpp
    ida_emitter.cpp
    generator.cpp
    typeformatter.cpp
)

target_compile_definitions(shadegenerator-codegen PUBLIC
    NOMINMAX
)

target_include_directories(shadegenerator-codegen PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(shadegenerator-codegen
    PRIVATE
        shadegenerator-sdk
        shadegenerator-tools
        shadegenerator-warnings
)
