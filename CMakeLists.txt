cmake_minimum_required(VERSION 3.22)

# ──────────────────────────────────────────────────────────────────────────────
# FREEQUENCY
#
# A next-generation hybrid DAW (FL Studio pattern workflow + Cubase linear
# audio/MIDI editing) built with modern C++20 and the JUCE 8 framework.
#
# This top-level script pulls in JUCE via FetchContent so the project is fully
# self-contained: a fresh `cmake -B build` will download the exact pinned JUCE
# version and build the standalone "FREEQUENCY" application.
# ──────────────────────────────────────────────────────────────────────────────

project(FREEQUENCY VERSION 0.1.0 LANGUAGES C CXX)

# We standardise on C++20 across every target (including JUCE's own modules).
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Produce compile_commands.json for IDE / clangd / Cursor tooling.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default to a Release build when the user does not specify one. A DAW is a
# real-time application: an unoptimised debug build will glitch under load.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ──────────────────────────────────────────────────────────────────────────────
# Dependencies: JUCE 8 via FetchContent
# ──────────────────────────────────────────────────────────────────────────────
include(FetchContent)

# Allow callers to override the JUCE tag from the command line if desired:
#   cmake -B build -DFREEQUENCY_JUCE_TAG=8.0.13
set(FREEQUENCY_JUCE_TAG "8.0.13" CACHE STRING "JUCE git tag to fetch")

message(STATUS "FREEQUENCY: fetching JUCE ${FREEQUENCY_JUCE_TAG}")

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        ${FREEQUENCY_JUCE_TAG}
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)
FetchContent_MakeAvailable(JUCE)

# ──────────────────────────────────────────────────────────────────────────────
# The application target lives in src/
# ──────────────────────────────────────────────────────────────────────────────
add_subdirectory(src)
