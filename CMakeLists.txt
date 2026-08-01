cmake_minimum_required(VERSION 3.22)

project(OpenGlitch
    VERSION 1.0.0
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
    set(HVCC_EXECUTABLE "${HVCC_BIN}/hvcc.exe")
    set(HVCC_PYTHON "${HVCC_BIN}/python.exe")
else()
    set(HVCC_BIN "${HVCC_VENV}/bin")
    set(HVCC_EXECUTABLE "${HVCC_BIN}/hvcc")
    set(HVCC_PYTHON "${HVCC_BIN}/python")
endif()

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
        COMMAND "${HVCC_PYTHON}" -m pip install --quiet "${hvcc_SOURCE_DIR}"
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
# Embedded assets: the UI fonts (OFL-licensed, licenses alongside the files)
# ---------------------------------------------------------------------------
juce_add_binary_data(OpenGlitchAssets
    SOURCES
        assets/fonts/RubikGlitch-Regular.ttf
        assets/fonts/SpaceMono-Regular.ttf
        assets/fonts/SpaceMono-Bold.ttf)

# ---------------------------------------------------------------------------
# The plugin itself. Formats per platform: VST3 + Standalone everywhere,
# AU on macOS, LV2 on Linux.
# ---------------------------------------------------------------------------
set(OPENGLITCH_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND OPENGLITCH_FORMATS AU)
elseif(UNIX)
    list(APPEND OPENGLITCH_FORMATS LV2)
endif()

# CI turns this off: runners have no use for plugins in the user folders.
option(OPENGLITCH_INSTALL_PLUGIN
    "Copy built plugins into the user's plugin folders after each build" ON)

juce_add_plugin(OpenGlitch
    PRODUCT_NAME "OpenGlitch"
    COMPANY_NAME "OpenGlitch Project"
    BUNDLE_ID com.openglitch.plugin
    PLUGIN_MANUFACTURER_CODE OpGl
    PLUGIN_CODE Oglt
    FORMATS ${OPENGLITCH_FORMATS}
    LV2URI "https://github.com/MiWCryptAnalytics/OpenGlitch"
    VST3_CATEGORIES Fx
    IS_SYNTH FALSE
    # MIDI notes 36..51 switch pattern banks 1..16 live
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    # Auto-installs to the platform plugin folder (~/.vst3 on Linux) so a
    # rebuild is immediately visible to hosts.
    COPY_PLUGIN_AFTER_BUILD ${OPENGLITCH_INSTALL_PLUGIN})

target_sources(OpenGlitch PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp)

target_compile_definitions(OpenGlitch PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    OPENGLITCH_VERSION="${PROJECT_VERSION}")

target_link_libraries(OpenGlitch
    PRIVATE
        OpenGlitchDSP
        OpenGlitchAssets
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Release packaging: stage the built formats and zip them under a versioned
# name — the same name CI publishes on tags.
#   cmake --build build --target openglitch_package
# ---------------------------------------------------------------------------
if(APPLE)
    set(OPENGLITCH_PACKAGE_OS "macOS")
else()
    set(OPENGLITCH_PACKAGE_OS "${CMAKE_SYSTEM_NAME}")
endif()
if(APPLE AND CMAKE_OSX_ARCHITECTURES MATCHES ";")
    set(OPENGLITCH_PACKAGE_ARCH "universal")
else()
    set(OPENGLITCH_PACKAGE_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
endif()
set(OPENGLITCH_PACKAGE_NAME
    "OpenGlitch-v${PROJECT_VERSION}-${OPENGLITCH_PACKAGE_OS}-${OPENGLITCH_PACKAGE_ARCH}")
set(OPENGLITCH_PACKAGE_STAGE "${CMAKE_BINARY_DIR}/package/${OPENGLITCH_PACKAGE_NAME}")
set(OPENGLITCH_ARTEFACTS "${CMAKE_BINARY_DIR}/OpenGlitch_artefacts/$<CONFIG>")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/package")

set(OPENGLITCH_PACKAGE_COMMANDS
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${OPENGLITCH_PACKAGE_STAGE}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${OPENGLITCH_PACKAGE_STAGE}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${OPENGLITCH_ARTEFACTS}/VST3/OpenGlitch.vst3"
            "${OPENGLITCH_PACKAGE_STAGE}/OpenGlitch.vst3"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${OPENGLITCH_ARTEFACTS}/Standalone"
            "${OPENGLITCH_PACKAGE_STAGE}")
set(OPENGLITCH_PACKAGE_DEPENDS OpenGlitch_VST3 OpenGlitch_Standalone)
if(APPLE)
    list(APPEND OPENGLITCH_PACKAGE_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${OPENGLITCH_ARTEFACTS}/AU/OpenGlitch.component"
                "${OPENGLITCH_PACKAGE_STAGE}/OpenGlitch.component")
    list(APPEND OPENGLITCH_PACKAGE_DEPENDS OpenGlitch_AU)
elseif(UNIX)
    list(APPEND OPENGLITCH_PACKAGE_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${OPENGLITCH_ARTEFACTS}/LV2/OpenGlitch.lv2"
                "${OPENGLITCH_PACKAGE_STAGE}/OpenGlitch.lv2")
    list(APPEND OPENGLITCH_PACKAGE_DEPENDS OpenGlitch_LV2)
endif()

add_custom_target(openglitch_package
    ${OPENGLITCH_PACKAGE_COMMANDS}
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE" "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
            "${OPENGLITCH_PACKAGE_STAGE}"
    COMMAND ${CMAKE_COMMAND} -E tar cf
            "${CMAKE_BINARY_DIR}/${OPENGLITCH_PACKAGE_NAME}.zip" --format=zip
            "${OPENGLITCH_PACKAGE_NAME}"
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/package"
    DEPENDS ${OPENGLITCH_PACKAGE_DEPENDS}
    COMMENT "Packaging ${OPENGLITCH_PACKAGE_NAME}.zip")

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
        JUCE_USE_CURL=0
        OPENGLITCH_VERSION="${PROJECT_VERSION}")
    target_link_libraries(OpenGlitchScreenshot PRIVATE
        OpenGlitchDSP
        OpenGlitchAssets
        juce::juce_audio_utils
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

    # Listening harness: renders the demo pattern and each effect solo to
    # WAVs (build/renders/) for manual A/B against the original Glitch 1.3.
    juce_add_console_app(OpenGlitchRender PRODUCT_NAME "OpenGlitchRender")
    target_sources(OpenGlitchRender PRIVATE tools/RenderDemos.cpp)
    target_include_directories(OpenGlitchRender PRIVATE tests)
    target_compile_definitions(OpenGlitchRender PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)
    target_link_libraries(OpenGlitchRender PRIVATE
        OpenGlitchDSP
        juce::juce_audio_utils
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
endif()

# ---------------------------------------------------------------------------
# Test suite (Catch2 + CTest). Every TEST_CASE becomes its own CTest entry
# running in a separate process with a timeout, so a hang fails loudly.
#   cmake -B build -DOPENGLITCH_BUILD_TESTS=ON && ctest --test-dir build
# ---------------------------------------------------------------------------
option(OPENGLITCH_BUILD_TESTS "Build the Catch2/CTest test suite" OFF)
if(OPENGLITCH_BUILD_TESTS)
    include(CTest)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.15.3
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(Catch2)

    juce_add_console_app(OpenGlitchTests PRODUCT_NAME "OpenGlitchTests")
    target_sources(OpenGlitchTests PRIVATE
        tests/DspEngineTests.cpp
        tests/LfoEngineTests.cpp
        tests/HostSimTests.cpp
        tests/WrapperTests.cpp
        src/PluginProcessor.cpp
        src/PluginEditor.cpp)
    target_include_directories(OpenGlitchTests PRIVATE src tests)
    target_compile_definitions(OpenGlitchTests PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        OPENGLITCH_VERSION="${PROJECT_VERSION}"
        JUCE_PLUGINHOST_VST3=1 # wrapper tests host the built .vst3 like a DAW
        OPENGLITCH_VST3_DIR="${CMAKE_BINARY_DIR}/OpenGlitch_artefacts/$<CONFIG>/VST3/OpenGlitch.vst3"
        JUCE_MODAL_LOOPS_PERMITTED=1) # tests pump the message loop to flush async updates
    add_dependencies(OpenGlitchTests OpenGlitch_VST3)
    target_link_libraries(OpenGlitchTests PRIVATE
        OpenGlitchDSP
        OpenGlitchAssets
        Catch2::Catch2WithMain
        juce::juce_audio_utils
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

    list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
    include(Catch)
    catch_discover_tests(OpenGlitchTests PROPERTIES TIMEOUT 60)
endif()
