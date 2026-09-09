add_library(shadegenerator-utils INTERFACE)

target_include_directories(shadegenerator-utils INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(shadegenerator-utils INTERFACE
    shadegenerator-warnings
)