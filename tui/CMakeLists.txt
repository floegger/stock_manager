cmake_minimum_required(VERSION 3.16)
project(Aktienkurse LANGUAGES CXX)

set(CMAKE_CXX_STANDARD          17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)

# ── Shared compile options (applied to every target) ─────────────────
add_library(common_opts INTERFACE)
target_compile_options(common_opts INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang>:
        -Wall -Wextra -Wpedantic -O2 -mavx2>
    $<$<CXX_COMPILER_ID:MSVC>:
        /W4 /O2 /arch:AVX2>
)

# ── Shared object library (Stock + HashTable + plot) ─────────────────
# Building once and linking into all three binaries avoids redundant
# recompilation and keeps the CMakeLists clean.
add_library(aktienkurse_lib STATIC
    Stock.cpp
    HashTable.cpp
    plot.cpp
)
target_link_libraries(aktienkurse_lib PUBLIC common_opts)

# ── 1. cli  –  original line-based menu ──────────────────────────────
add_executable(cli main.cpp)
target_link_libraries(cli PRIVATE aktienkurse_lib)

# ── 2. tui  –  ASCII box-drawing TUI (no extra dependencies) ─────────
add_executable(tui tui_ascii.cpp)
target_link_libraries(tui PRIVATE aktienkurse_lib)

# ── 3. ncurses_tui  –  full-screen ncurses TUI ───────────────────────
find_package(Curses QUIET)
if(CURSES_FOUND)
    add_executable(ncurses_tui tui_ncurses.cpp)
    target_include_directories(ncurses_tui PRIVATE ${CURSES_INCLUDE_DIRS})
    target_link_libraries(ncurses_tui PRIVATE aktienkurse_lib
                                              ${CURSES_LIBRARIES})
    message(STATUS "ncurses found – ncurses_tui target enabled")
else()
    message(STATUS "ncurses not found – ncurses_tui target skipped")
    message(STATUS "  Install: sudo apt install libncurses-dev")
    message(STATUS "          or: brew install ncurses")
endif()

# ── Unit tests (optional, requires doctest.h) ────────────────────────
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/doctest.h)
    add_executable(aktienkurse_tests tests.cpp)
    target_link_libraries(aktienkurse_tests PRIVATE aktienkurse_lib)
    target_compile_options(aktienkurse_tests PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>: -Wall -Wextra -O1 -mavx2>
        $<$<CXX_COMPILER_ID:MSVC>:       /W4 /O1 /arch:AVX2>
    )
    enable_testing()
    add_test(NAME unit_tests COMMAND aktienkurse_tests)
endif()

# ── Quick-start ──────────────────────────────────────────────────────
# mkdir build && cd build
# cmake ..                        # configure
# cmake --build .                 # build all available targets
# ./cli                           # line-based CLI
# ./tui                           # ASCII-frame TUI
# ./ncurses_tui                   # full-screen TUI (if ncurses found)
# ctest                           # run unit tests