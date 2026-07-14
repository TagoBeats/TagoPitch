cmake_minimum_required(VERSION 3.24)
project(TagoPitch VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(TAGOPITCH_DEV_UI "Load the plugin UI from the Vite dev server (http://localhost:5173) instead of the bundled build" OFF)

add_subdirectory(third_party/JUCE)

juce_add_plugin(TagoPitch
    COMPANY_NAME "TagoBeats"
    BUNDLE_ID com.tagobeats.tagopitch
    PLUGIN_MANUFACTURER_CODE Tago
    PLUGIN_CODE TgPt
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "TagoPitch"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_WEB_BROWSER TRUE
    NEEDS_WEBVIEW2 TRUE
    MICROPHONE_PERMISSION_ENABLED TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(TagoPitch PRIVATE
    plugin/PluginProcessor.cpp
    plugin/PluginEditor.cpp)

# signalsmith-stretch engine, vendored 1:1 from the tagodsp prototype for engine parity.
# Same FFT backend as the Python build: Accelerate on Apple.
# SYSTEM keeps the strict warning flags quiet for vendored code we do not touch.
target_include_directories(TagoPitch SYSTEM PRIVATE third_party)
if(APPLE)
    target_compile_definitions(TagoPitch PRIVATE SIGNALSMITH_USE_ACCELERATE ACCELERATE_NEW_LAPACK)
    target_link_libraries(TagoPitch PRIVATE "-framework Accelerate")
endif()
if(WIN32)
    # NEEDS_WEBVIEW2 only links the WebView2 loader lib; this config macro is what
    # actually enables the WebView2 backend (and the resource provider) in juce_gui_extra.
    target_compile_definitions(TagoPitch PRIVATE JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1)
endif()

# The bundled UI: ui/webui.zip is produced by scripts/build-ui.sh from the Vite build.
if(NOT TAGOPITCH_DEV_UI AND NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ui/webui.zip")
    message(FATAL_ERROR "ui/webui.zip missing. Run scripts/build-ui.sh first or configure with -DTAGOPITCH_DEV_UI=ON.")
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ui/webui.zip")
    juce_add_binary_data(TagoPitchAssets SOURCES ui/webui.zip)
else()
    juce_add_binary_data(TagoPitchAssets SOURCES cmake/empty_webui.zip)
endif()

target_compile_definitions(TagoPitch PRIVATE
    JUCE_WEB_BROWSER=1
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    TAGOPITCH_DEV_UI=$<BOOL:${TAGOPITCH_DEV_UI}>)

target_link_libraries(TagoPitch
    PRIVATE
        TagoPitchAssets
        juce::juce_audio_utils
        juce::juce_gui_extra
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Offline render CLI for the delta comparison against the Python prototype.
# Dev tool only, opt in with -DTAGOPITCH_BUILD_TOOLS=ON.
option(TAGOPITCH_BUILD_TOOLS "Build the offline render CLI" OFF)
if(TAGOPITCH_BUILD_TOOLS)
    juce_add_console_app(TagoPitchRender PRODUCT_NAME "TagoPitchRender")
    target_sources(TagoPitchRender PRIVATE tools/render_cli.cpp)
    target_include_directories(TagoPitchRender SYSTEM PRIVATE third_party)
    target_compile_definitions(TagoPitchRender PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        SIGNALSMITH_USE_ACCELERATE
        ACCELERATE_NEW_LAPACK)
    target_link_libraries(TagoPitchRender PRIVATE
        juce::juce_audio_formats
        juce::juce_dsp
        "-framework Accelerate"
        PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
endif()
