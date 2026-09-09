add_library(shadegenerator-sdk INTERFACE)
add_library(shadegenerator::sdk ALIAS shadegenerator-sdk)

get_filename_component(shadegenerator_include_root "${CMAKE_CURRENT_SOURCE_DIR}" DIRECTORY)

target_include_directories(shadegenerator-sdk INTERFACE
    "${shadegenerator_include_root}"
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
