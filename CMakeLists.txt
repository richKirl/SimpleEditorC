cmake_minimum_required(VERSION 3.28)

project(SimpleEditorC LANGUAGES C)

set(CMAKE_C_COMPILER "/usr/lib/llvm/20/bin/clang")#set compiler
set(CMAKE_CXX_COMPILER "/usr/lib/llvm/20/bin/clang++")#set compiler
add_definitions(-DSHM)
add_executable(SimpleEditorC main.c)

find_package(SDL3 REQUIRED)
find_package(SDL3_ttf REQUIRED)
include(GNUInstallDirs)
install(TARGETS SimpleEditorC
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
target_link_libraries(SimpleEditorC
    PRIVATE
    SDL3
    SDL3_ttf
    m
)
# Создание исполняемого файла
# add_executable(SimpleEditorC ${SOURCES})
target_compile_options(SimpleEditorC PRIVATE
    -O3
    -ansi
    -msse4.2
    -mavx2
    -ffast-math
#    -Wall
#    -Wextra
)
# target_link_libraries(SimpleEditorC PRIVATE SDL3::SDL3-shared)
# target_link_libraries(SimpleEditorC PRIVATE SDL3::SDL3_ttf::SDL3_ttf-shared)
