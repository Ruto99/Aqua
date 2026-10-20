cmake_minimum_required(VERSION 3.24)
project(AquaSamples LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# RutoEngine の場所（ランチャーが生成。環境変数 RUTO_ENGINE_DIR があればそちらを優先）
include(ProjectSettings/RutoEngine.cmake)
if(DEFINED ENV{RUTO_ENGINE_DIR})
    set(RUTO_ENGINE_DIR "$ENV{RUTO_ENGINE_DIR}")
endif()
include("${RUTO_ENGINE_DIR}/cmake/RutoProject.cmake")

# Source フォルダ内の .cpp / .h はすべて自動でビルドに含まれる
file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS Source/*.cpp Source/*.h)
ruto_add_game(AquaSamples SOURCES ${GAME_SOURCES})
