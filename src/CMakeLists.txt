# ──────────────────────────────────────────────────────────────────────────────
# FREEQUENCY — modular build
#
# The codebase is compiled as four static libraries with a strictly downward
# dependency graph, then linked into the FREEQUENCY application:
#
#     FREEQUENCY (app) ─▶ FreequencyUI ─▶ FreequencyEngine ─▶ FreequencyDSP
#                                              │                    │
#                                              └────────────────────┴─▶ FreequencyCore
#
# JUCE module *sources* are compiled exactly once — into the app target. The
# static libraries receive only JUCE's headers + compile definitions (via the
# `freequency_common` interface), so there are no duplicate JUCE symbols, while
# the Model/Engine/DSP code stays free of JUCE module ownership.
# ──────────────────────────────────────────────────────────────────────────────

set(FREEQUENCY_JUCE_MODULES
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_basics
    juce::juce_gui_extra)

# Definitions that MUST match between the libraries and the app (they affect JUCE
# header-inlined code, so a mismatch would be an ODR violation).
set(FREEQUENCY_DEFS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_AU=1
    JUCE_PLUGINHOST_LADSPA=1)

# ── Shared interface: C++20, our include root, JUCE headers/defs, warning flags ──
add_library(freequency_common INTERFACE)
target_compile_features(freequency_common INTERFACE cxx_std_20)
target_include_directories(freequency_common INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(freequency_common INTERFACE ${FREEQUENCY_DEFS})
target_link_libraries(freequency_common INTERFACE
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

# Pull each JUCE module's include dirs + availability definitions WITHOUT linking
# (so the module sources are not compiled into our libraries).
foreach(mod ${FREEQUENCY_JUCE_MODULES})
    target_include_directories(freequency_common SYSTEM INTERFACE
        $<TARGET_PROPERTY:${mod},INTERFACE_INCLUDE_DIRECTORIES>)
    target_compile_definitions(freequency_common INTERFACE
        $<TARGET_PROPERTY:${mod},INTERFACE_COMPILE_DEFINITIONS>)
endforeach()

option(FREEQUENCY_ENABLE_LTO "Enable link-time optimisation" ON)
if(FREEQUENCY_ENABLE_LTO)
    target_link_libraries(freequency_common INTERFACE juce::juce_recommended_lto_flags)
endif()

# Helper to declare one of our static libraries consistently.
function(freequency_add_library name)
    add_library(${name} STATIC ${ARGN})
    target_link_libraries(${name} PUBLIC freequency_common)
    set_target_properties(${name} PROPERTIES POSITION_INDEPENDENT_CODE ON)
endfunction()

# ── FreequencyCore — the data model "brain" (no UI, no audio devices) ────────────
freequency_add_library(FreequencyCore
    Models/Clip.cpp
    Models/Track.cpp
    Models/AudioTrack.cpp
    Models/MidiTrack.cpp
    Models/Pattern.cpp
    Models/Timeline.cpp
    Models/Mixer.cpp
    Models/Project.cpp
    Models/ProjectSerializer.cpp)

# ── FreequencyDSP — self-contained signal processors (no transport/model) ────────
freequency_add_library(FreequencyDSP
    DSP/SynthProcessor.cpp
    DSP/LimiterProcessor.cpp
    DSP/BuiltinEffects.cpp)

# ── FreequencyEngine — real-time audio graph, transport, sequencing ─────────────
freequency_add_library(FreequencyEngine
    Engine/AudioEngine.cpp
    Engine/TrackProcessor.cpp
    Engine/MidiSourceProcessor.cpp
    Engine/AudioClipProcessor.cpp
    Engine/MetronomeProcessor.cpp
    Engine/PluginManager.cpp)
target_link_libraries(FreequencyEngine PUBLIC FreequencyCore FreequencyDSP)

# ── FreequencyUI — views (observe Core/Engine, never the reverse) ────────────────
freequency_add_library(FreequencyUI
    UI/FreequencyLookAndFeel.cpp
    UI/TransportBar.cpp
    UI/TrackHeaderComponent.cpp
    UI/TrackLaneComponent.cpp
    UI/TimelineRuler.cpp
    UI/ArrangeView.cpp
    UI/ChannelStrip.cpp
    UI/MixerView.cpp
    UI/MainComponent.cpp)
target_link_libraries(FreequencyUI PUBLIC FreequencyCore FreequencyEngine)

# ── FREEQUENCY — the application target ──────────────────────────────────────────
juce_add_gui_app(FREEQUENCY
    PRODUCT_NAME    "FREEQUENCY"
    COMPANY_NAME    "FREEQUENCY"
    VERSION         ${PROJECT_VERSION}
    BUNDLE_ID       "com.freequency.app"
    ICON_BIG        "${CMAKE_SOURCE_DIR}/assets/icon.png"
    DOCUMENT_EXTENSIONS       freq
    NEEDS_CURL                FALSE
    NEEDS_WEB_BROWSER         FALSE)

juce_generate_juce_header(FREEQUENCY)

target_sources(FREEQUENCY PRIVATE Main.cpp)
target_include_directories(FREEQUENCY PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(FREEQUENCY PRIVATE
    ${FREEQUENCY_DEFS}
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:FREEQUENCY,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:FREEQUENCY,JUCE_VERSION>")

# Our libraries (headers/defs only) + the JUCE modules compiled once, here.
target_link_libraries(FREEQUENCY PRIVATE
    FreequencyUI
    FreequencyEngine
    FreequencyDSP
    FreequencyCore
    ${FREEQUENCY_JUCE_MODULES}
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
    $<$<BOOL:${FREEQUENCY_ENABLE_LTO}>:juce::juce_recommended_lto_flags>)
