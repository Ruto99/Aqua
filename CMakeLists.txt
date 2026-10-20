# Aqua — RutoEngine のゲームの UI を作るツール（RutoEngine とは別のアプリ）
#
# RutoEngine の SDK（ビルド済みのエンジン）をリンクして作る。エンジンの場所は次の順で決まる:
#   -DRUTO_ENGINE_DIR=<dir> / 環境変数 RUTO_ENGINE_DIR / このフォルダの隣の ../RutoEngine
#
#   build.bat [debug|release] [build|install|run|clean]
#     install … release でビルドして bin/Aqua.exe に置く（RutoEditor・RutoLauncher はここの Aqua.exe を見つけて起動する）
cmake_minimum_required(VERSION 3.24)
project(Aqua VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(NOT RUTO_ENGINE_DIR AND DEFINED ENV{RUTO_ENGINE_DIR})
    set(RUTO_ENGINE_DIR "$ENV{RUTO_ENGINE_DIR}")
endif()
if(NOT RUTO_ENGINE_DIR)
    get_filename_component(RUTO_ENGINE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../RutoEngine" ABSOLUTE)
endif()
set(RUTO_ENGINE_DIR "${RUTO_ENGINE_DIR}" CACHE PATH "RutoEngine のフォルダ（SDK を使う）")
# ビルド構成に合った SDK（SDK/Debug・SDK/Release）を find_package する（ゲームプロジェクトと同じ仕組み）
include("${RUTO_ENGINE_DIR}/cmake/RutoProject.cmake")

file(GLOB AQUA_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/Source/*.h)
add_executable(Aqua ${AQUA_SOURCES})
target_link_libraries(Aqua PRIVATE Ruto::Engine Ruto::EditorUI $<$<PLATFORM_ID:Windows>:shell32>)
target_compile_definitions(Aqua PRIVATE AQUA_VERSION="${PROJECT_VERSION}")
ruto_set_exe_icon(Aqua "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Aqua.ico") # RutoGame.cmake（SDK）の関数

if(MSVC)
    target_compile_options(Aqua PRIVATE /W4 /permissive-)
    target_link_options(Aqua PRIVATE /ENTRY:mainCRTStartup)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set_target_properties(Aqua PROPERTIES WIN32_EXECUTABLE TRUE)
    endif()
endif()

# ウィンドウのアイコン（実行ファイルの隣の Aqua.png を読む）
add_custom_command(TARGET Aqua POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Aqua.png $<TARGET_FILE_DIR:Aqua>/Aqua.png)

# build.bat install → <このフォルダ>/bin
install(TARGETS Aqua RUNTIME DESTINATION .)
install(FILES Resources/Aqua.png DESTINATION .)
