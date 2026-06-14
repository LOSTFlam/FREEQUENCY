# ──────────────────────────────────────────────────────────────────────────────
# OmniDAW standalone application target
# ──────────────────────────────────────────────────────────────────────────────

# juce_add_gui_app creates a GUI application target. Even though Phase 1 ships no
# real UI, a DAW is fundamentally a windowed application, so we use the GUI app
# wrapper from the start to avoid restructuring later.
juce_add_gui_app(OmniDAW
    PRODUCT_NAME    "OmniDAW"
    COMPANY_NAME    "OmniDAW"
    VERSION         ${PROJECT_VERSION}
    # A console is invaluable while bringing up the audio engine.
    NEEDS_CURL                FALSE
    NEEDS_WEB_BROWSER         FALSE)

# Generates a JuceHeader-free build: we include individual module headers
# explicitly. This keeps compile times down and dependencies obvious.
juce_generate_juce_header(OmniDAW)

target_sources(OmniDAW PRIVATE
    Main.cpp

    # ── Engine ───────────────────────────────────────────────────────────────
    Engine/AudioEngine.cpp
    Engine/TrackProcessor.cpp

    # ── Models ───────────────────────────────────────────────────────────────
    Models/Clip.cpp
    Models/Track.cpp
    Models/AudioTrack.cpp
    Models/MidiTrack.cpp
    Models/Timeline.cpp
    Models/Mixer.cpp
    Models/Project.cpp

    # ── UI (intentionally minimal in Phase 1) ─────────────────────────────────
    UI/MainComponent.cpp)

# Let every source file include headers relative to src/ (e.g. "Models/Track.h").
target_include_directories(OmniDAW PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_features(OmniDAW PRIVATE cxx_std_20)

# ── Preprocessor configuration ───────────────────────────────────────────────
target_compile_definitions(OmniDAW PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:OmniDAW,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:OmniDAW,JUCE_VERSION>"
    # Plugin hosting toggles. VST3 hosting is wired up in Phase 2 but we enable
    # the format here so the module compiles with the host code paths present.
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_AU=1
    JUCE_PLUGINHOST_LADSPA=1)

# ── JUCE module dependencies ─────────────────────────────────────────────────
target_link_libraries(OmniDAW PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    PRIVATE
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
