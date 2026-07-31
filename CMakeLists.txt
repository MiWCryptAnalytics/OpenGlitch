cmake_minimum_required(VERSION 3.22)

project(OpenGlitch
    VERSION 0.1.0
    DESCRIPTION "Open-source port of dblue Glitch 1.3 (Pure Data + hvcc + JUCE)"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(FetchContent)

# ---------------------------------------------------------------------------
# JUCE 8 (plugin wrapper + GUI framework)
# ---------------------------------------------------------------------------
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.15
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# ---------------------------------------------------------------------------
# hvcc — the Heavy Pd-to-C compiler (Wasted-Audio maintained fork).
# The repo is fetched as plain source (it is a Python package, not a CMake
# project) and installed into a virtualenv inside the build tree.
# ---------------------------------------------------------------------------
FetchContent_Declare(hvcc
    GIT_REPOSITORY https://github.com/Wasted-Audio/hvcc.git
    GIT_TAG        v0.16.2
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(hvcc)

find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)

set(HVCC_VENV "${CMAKE_BINARY_DIR}/hvcc-venv")
if(WIN32)
    set(HVCC_BIN "${HVCC_VENV}/Scripts")
else()
    set(HVCC_BIN "${HVCC_VENV}/bin")
endif()
set(HVCC_EXECUTABLE "${HVCC_BIN}/hvcc")

if(NOT EXISTS "${HVCC_EXECUTABLE}")
    message(STATUS "Creating hvcc virtualenv: ${HVCC_VENV}")
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" -m venv "${HVCC_VENV}"
        RESULT_VARIABLE HVCC_VENV_RESULT
        OUTPUT_VARIABLE HVCC_VENV_OUTPUT
        ERROR_VARIABLE  HVCC_VENV_OUTPUT)
    if(NOT HVCC_VENV_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to create hvcc virtualenv:\n${HVCC_VENV_OUTPUT}")
    endif()

    message(STATUS "Installing hvcc from ${hvcc_SOURCE_DIR}")
    execute_process(
        COMMAND "${HVCC_BIN}/python" -m pip install --quiet "${hvcc_SOURCE_DIR}"
        RESULT_VARIABLE HVCC_PIP_RESULT
        OUTPUT_VARIABLE HVCC_PIP_OUTPUT
        ERROR_VARIABLE  HVCC_PIP_OUTPUT)
    if(NOT HVCC_PIP_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to pip-install hvcc:\n${HVCC_PIP_OUTPUT}")
    endif()
endif()

# ---------------------------------------------------------------------------
# Compile the Pd patch to C/C++ with hvcc (configure time, so the generated
# file list can be globbed). Editing the patch re-runs CMake automatically
# via CMAKE_CONFIGURE_DEPENDS.
# ---------------------------------------------------------------------------
set(HEAVY_PATCH   "${CMAKE_CURRENT_SOURCE_DIR}/pd/main.pd")
set(HEAVY_OUT_DIR "${CMAKE_BINARY_DIR}/heavy")

message(STATUS "Running hvcc on ${HEAVY_PATCH}")
execute_process(
    COMMAND "${HVCC_EXECUTABLE}" "${HEAVY_PATCH}" -n OpenGlitch -g c -o "${HEAVY_OUT_DIR}"
    RESULT_VARIABLE HVCC_RESULT
    OUTPUT_VARIABLE HVCC_OUTPUT
    ERROR_VARIABLE  HVCC_ERROR)
if(NOT HVCC_RESULT EQUAL 0)
    message(FATAL_ERROR "hvcc failed on ${HEAVY_PATCH}:\n${HVCC_OUTPUT}\n${HVCC_ERROR}")
endif()
if(HVCC_OUTPUT MATCHES "[Ww]arning|[Ee]rror")
    message(WARNING "hvcc reported issues:\n${HVCC_OUTPUT}")
endif()

file(GLOB HEAVY_PATCH_FILES "${CMAKE_CURRENT_SOURCE_DIR}/pd/*.pd")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${HEAVY_PATCH_FILES})

file(GLOB HEAVY_SOURCES CONFIGURE_DEPENDS
    "${HEAVY_OUT_DIR}/c/*.c"
    "${HEAVY_OUT_DIR}/c/*.cpp")
if(NOT HEAVY_SOURCES)
    message(FATAL_ERROR "hvcc produced no sources in ${HEAVY_OUT_DIR}/c")
endif()

add_library(OpenGlitchDSP STATIC ${HEAVY_SOURCES})
target_include_directories(OpenGlitchDSP PUBLIC "${HEAVY_OUT_DIR}/c")

# hvcc's generated code vectorises with SSE4.1 when available (falls back to
# scalar otherwise). The flag is PUBLIC so every TU that includes HvUtils.h
# agrees on HV_N_SIMD.
if((CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|i.86") AND NOT MSVC)
    target_compile_options(OpenGlitchDSP PUBLIC -msse4.1)
endif()

# ---------------------------------------------------------------------------
# The plugin itself
# ---------------------------------------------------------------------------
juce_add_plugin(OpenGlitch
    PRODUCT_NAME "OpenGlitch"
    COMPANY_NAME "OpenGlitch Project"
    BUNDLE_ID com.openglitch.plugin
    PLUGIN_MANUFACTURER_CODE OpGl
    PLUGIN_CODE Oglt
    FORMATS VST3 Standalone
    VST3_CATEGORIES Fx
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    # Auto-installs to the platform plugin folder (~/.vst3 on Linux) so a
    # rebuild is immediately visible to hosts.
    COPY_PLUGIN_AFTER_BUILD TRUE)

target_sources(OpenGlitch PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp)

target_compile_definitions(OpenGlitch PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(OpenGlitch
    PRIVATE
        OpenGlitchDSP
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Developer tools: headless UI screenshot renderer (also usable from CI)
# ---------------------------------------------------------------------------
option(OPENGLITCH_BUILD_TOOLS "Build developer tools (UI screenshot renderer)" OFF)
if(OPENGLITCH_BUILD_TOOLS)
    juce_add_console_app(OpenGlitchScreenshot PRODUCT_NAME "OpenGlitchScreenshot")
    target_sources(OpenGlitchScreenshot PRIVATE
        tools/Screenshot.cpp
        src/PluginProcessor.cpp
        src/PluginEditor.cpp)
    target_include_directories(OpenGlitchScreenshot PRIVATE src)
    target_compile_definitions(OpenGlitchScreenshot PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)
    target_link_libraries(OpenGlitchScreenshot PRIVATE
        OpenGlitchDSP
        juce::juce_audio_utils
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
endif()
