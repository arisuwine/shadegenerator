include(FetchContent)

FetchContent_Declare(
    ValveFileVDF
    GIT_REPOSITORY https://github.com/TinyTinni/ValveFileVDF.git
    GIT_TAG b45809f3e8447308f1e08cb63bc2dc8564318339
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(ValveFileVDF)

add_library(shadegenerator-game STATIC
    game.cpp
)

target_include_directories(shadegenerator-game PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(shadegenerator-game
    PRIVATE
        ValveFileVDF::ValveFileVDF
        shadegenerator-game-config
        shadegenerator-utils
        shadegenerator-warnings
)
