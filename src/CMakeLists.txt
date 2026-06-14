# ──────────────────────────────────────────────────────────────────────────────
# FREEQUENCY standalone application target
# ──────────────────────────────────────────────────────────────────────────────

# juce_add_gui_app creates a GUI application target. Even though Phase 1 ships no
# real UI, a DAW is fundamentally a windowed application, so we use the GUI app
# wrapper from the start to avoid restructuring later.
juce_add_gui_app(FREEQUENCY
    PRODUCT_NAME    "FREEQUENCY"
    COMPANY_NAME    "FREEQUENCY"
    VERSION         ${PROJECT_VERSION}
    BUNDLE_ID       "com.freequency.app"
    ICON_BIG        "${CMAKE_SOURCE_DIR}/assets/icon.png"
    # Associate the custom project document type (.freq) with the app. On macOS
    # this writes the CFBundleDocumentTypes into Info.plist; the Linux/Windows
    # association is handled by the installer/launcher scripts.
    DOCUMENT_EXTENSIONS       freq
    # A console is invaluable while bringing up the audio engine.
    NEEDS_CURL                FALSE
    NEEDS_WEB_BROWSER         FALSE)

# Generates a JuceHeader-free build: we include individual module headers
# explicitly. This keeps compile times down and dependencies obvious.
juce_generate_juce_header(FREEQUENCY)

target_sources(FREEQUENCY PRIVATE
    Main.cpp

    # ── Engine ───────────────────────────────────────────────────────────────
    Engine/AudioEngine.cpp
    Engine/TrackProcessor.cpp
    Engine/MidiSourceProcessor.cpp
    Engine/AudioClipProcessor.cpp
    Engine/SynthProcessor.cpp
    Engine/MetronomeProcessor.cpp
    Engine/LimiterProcessor.cpp
    Engine/PluginManager.cpp

    # ── Models ───────────────────────────────────────────────────────────────
    Models/Clip.cpp
    Models/Track.cpp
    Models/AudioTrack.cpp
    Models/MidiTrack.cpp
    Models/Timeline.cpp
    Models/Mixer.cpp
    Models/Project.cpp
    Models/ProjectSerializer.cpp

    # ── UI ─────────────────────────────────────────────────────────────────────
    UI/FreequencyLookAndFeel.cpp
    UI/TransportBar.cpp
    UI/TrackHeaderComponent.cpp
    UI/TrackLaneComponent.cpp
    UI/TimelineRuler.cpp
    UI/ArrangeView.cpp
    UI/ChannelStrip.cpp
    UI/MixerView.cpp
    UI/MainComponent.cpp)

# Let every source file include headers relative to src/ (e.g. "Models/Track.h").
target_include_directories(FREEQUENCY PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_features(FREEQUENCY PRIVATE cxx_std_20)

# ── Preprocessor configuration ───────────────────────────────────────────────
target_compile_definitions(FREEQUENCY PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:FREEQUENCY,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:FREEQUENCY,JUCE_VERSION>"
    # Plugin hosting toggles. VST3 hosting is wired up in Phase 2 but we enable
    # the format here so the module compiles with the host code paths present.
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_AU=1
    JUCE_PLUGINHOST_LADSPA=1)

# ── JUCE module dependencies ─────────────────────────────────────────────────
target_link_libraries(FREEQUENCY PRIVATE
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
    juce::juce_recommended_warning_flags)

# Link-time optimisation is great for release performance but slow to link, which
# hurts iteration. Default ON; configure with -DFREEQUENCY_ENABLE_LTO=OFF to disable.
option(FREEQUENCY_ENABLE_LTO "Enable link-time optimisation" ON)
if(FREEQUENCY_ENABLE_LTO)
    target_link_libraries(FREEQUENCY PRIVATE juce::juce_recommended_lto_flags)
endif()
