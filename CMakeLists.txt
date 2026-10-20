cmake_minimum_required(VERSION 3.22)

project(Vibes VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(BEADS_BUILD_PLUGIN "Build the JUCE plugin" ON)
option(BEADS_BUILD_TESTS "Build the headless engine test harness" ON)
option(BEADS_COPY_AFTER_BUILD "Copy the plugin into the system plugin folder after building" OFF)
set(JUCE_DIR "" CACHE PATH "Path to a JUCE checkout (downloaded automatically when empty)")

# ------------------------------------------------------------------------------
# Mutable Instruments Beads DSP (vendored, MIT) -- no JUCE dependency
# ------------------------------------------------------------------------------
set(MUTABLE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/mutable)

add_library(mutable_beads_dsp STATIC
  ${MUTABLE_DIR}/beads/dsp/processor.cc
  ${MUTABLE_DIR}/beads/dsp/trigger_generator.cc
  ${MUTABLE_DIR}/beads/dsp/delay_controller.cc
  ${MUTABLE_DIR}/beads/dsp/delay_clock.cc
  ${MUTABLE_DIR}/beads/dsp/mu_law.cc
  ${MUTABLE_DIR}/beads/dsp/synth.cc
  ${MUTABLE_DIR}/beads/resources.cc
  ${MUTABLE_DIR}/stmlib/dsp/atan.cc
  ${MUTABLE_DIR}/stmlib/dsp/units.cc
  ${MUTABLE_DIR}/stmlib/utils/random.cc)

target_include_directories(mutable_beads_dsp PUBLIC ${MUTABLE_DIR})
# stmlib's TEST switch selects the desktop (non-ARM, no linker-section) code paths.
target_compile_definitions(mutable_beads_dsp PUBLIC TEST)
if(WIN32)
  target_compile_definitions(mutable_beads_dsp PUBLIC _USE_MATH_DEFINES NOMINMAX)   # M_PI etc. on Windows
endif()
if(MSVC AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  message(FATAL_ERROR
    "The Beads DSP uses variable-length arrays, which MSVC does not support. "
    "Use the ClangCL toolset:  cmake -G \"Visual Studio 17 2022\" -T ClangCL -A x64 ..")
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(mutable_beads_dsp PRIVATE -w)   # vendored code, keep the log quiet
endif()
if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  target_compile_options(mutable_beads_dsp PRIVATE -w)
endif()

# ------------------------------------------------------------------------------
# Headless test harness
# ------------------------------------------------------------------------------
if(BEADS_BUILD_TESTS)
  add_executable(beads_test_harness
    tools/test_harness.cpp
    Source/BeadsEngine.cpp)
  target_link_libraries(beads_test_harness PRIVATE mutable_beads_dsp)
  enable_testing()
  add_test(NAME engine_sanity COMMAND beads_test_harness ${CMAKE_CURRENT_BINARY_DIR})
endif()

# ------------------------------------------------------------------------------
# JUCE plugin
# ------------------------------------------------------------------------------
if(BEADS_BUILD_PLUGIN)
  if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} juce EXCLUDE_FROM_ALL)
  else()
    include(FetchContent)
    FetchContent_Declare(JUCE
      GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
      GIT_TAG 8.0.12
      GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
  endif()

  set(PLUGIN_FORMATS VST3)
  if(APPLE)
    list(APPEND PLUGIN_FORMATS AU)
  endif()

  # Fonts (SIL OFL) and the logo are compiled into the plugin so it looks identical everywhere.
  juce_add_binary_data(VibesAssets SOURCES
    Assets/Barlow-Medium.ttf
    Assets/Barlow-SemiBold.ttf
    Assets/Barlow-Bold.ttf
    Assets/BarlowCondensed-Bold.ttf
    Assets/IBMPlexMono-Medium.ttf
    Assets/vibes_logo.png)

  juce_add_plugin(Vibes
    COMPANY_NAME "Vibes"
    PLUGIN_MANUFACTURER_CODE Vibe
    PLUGIN_CODE Vib1
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "Vibes"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${BEADS_COPY_AFTER_BUILD}
    VST3_CATEGORIES Fx
    AU_MAIN_TYPE kAudioUnitType_MusicEffect)

  target_sources(Vibes PRIVATE
    Source/BeadsEngine.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

  target_compile_definitions(Vibes PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

  if(UNIX AND NOT APPLE)
    # Keep the Linux build free of optional X11 extension dependencies.
    target_compile_definitions(Vibes PUBLIC
      JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_USE_XSHM=0)
  endif()

  target_link_libraries(Vibes
    PRIVATE
      VibesAssets
      mutable_beads_dsp
      juce::juce_audio_processors
    PUBLIC
      juce::juce_recommended_config_flags)
endif()

# ------------------------------------------------------------------------------
# Optional: instantiate the real plugin class, exercise it, and snapshot the UI.
#   cmake -DBEADS_BUILD_PLUGIN_TEST=ON ... ; xvfb-run -a ./beads_plugin_test out.png
# ------------------------------------------------------------------------------
option(BEADS_BUILD_PLUGIN_TEST "Build the plugin-level test (needs a display, e.g. xvfb)" OFF)
if(BEADS_BUILD_PLUGIN AND BEADS_BUILD_PLUGIN_TEST)
  add_executable(beads_plugin_test tools/plugin_test.cpp)
  target_include_directories(beads_plugin_test PRIVATE Source)
  target_link_libraries(beads_plugin_test PRIVATE
    Vibes VibesAssets mutable_beads_dsp juce::juce_audio_processors juce::juce_gui_basics
    juce::juce_recommended_config_flags)
endif()
