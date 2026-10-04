cmake_minimum_required(VERSION 3.20)
project(ThumbForgeShellExtension LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-DUNICODE -D_UNICODE -D_CRT_SECURE_NO_WARNINGS)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/pdfium/include
)

link_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/pdfium/lib
)

set(COMMON_SOURCES
    src/PdfRenderer.cpp
    src/VideoRenderer.cpp
    src/AudioRenderer.cpp
    src/ApkRenderer.cpp
    src/CodeRenderer.cpp
    src/ShellRegistry.cpp
)

# 1. ThumbForgeProvider.dll (COM in-proc server)
add_library(ThumbForgeProvider SHARED
    ${COMMON_SOURCES}
    src/ThumbnailProvider.cpp
    src/ClassFactory.cpp
    src/DllMain.cpp
    src/ThumbForgeProvider.def
)

target_link_libraries(ThumbForgeProvider PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/pdfium/lib/pdfium.dll.lib"
    gdiplus
    shlwapi
    shell32
    ole32
    user32
    gdi32
    advapi32
    Cabinet
)

# 2. ThumbForgeCli.exe (companion CLI tool)
add_executable(ThumbForgeCli
    ${COMMON_SOURCES}
    src/CliMain.cpp
)

target_link_libraries(ThumbForgeCli PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/pdfium/lib/pdfium.dll.lib"
    gdiplus
    shlwapi
    shell32
    ole32
    user32
    gdi32
    advapi32
    Cabinet
)


# Copy pdfium.dll next to output binaries
add_custom_command(TARGET ThumbForgeProvider POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/pdfium/bin/pdfium.dll"
    "$<TARGET_FILE_DIR:ThumbForgeProvider>/pdfium.dll"
)

add_custom_command(TARGET ThumbForgeCli POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/pdfium/bin/pdfium.dll"
    "$<TARGET_FILE_DIR:ThumbForgeCli>/pdfium.dll"
)
