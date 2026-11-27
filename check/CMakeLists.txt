# ComplicatorCheck: a console program that runs the plugin layer (parameters, programs / presets, saved state, quality
# switching) without a host. It compiles the plugin's own source files, so what it checks is what Logic gets.
# Not part of the plugin; the build workflow runs it on every push.

juce_add_console_app(ComplicatorCheck PRODUCT_NAME "ComplicatorCheck")

target_sources(ComplicatorCheck PRIVATE
    Check.cpp
    ../plugin/Source/Parameters.cpp
    ../plugin/Source/PluginProcessor.cpp
    ../plugin/Source/PluginEditor.cpp
    ../plugin/Source/Controls.cpp
)

target_include_directories(ComplicatorCheck PRIVATE ../plugin/Source)

target_compile_definitions(ComplicatorCheck PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_ALSA=0
    JUCE_JACK=0
    JUCE_MODAL_LOOPS_PERMITTED=0
    COMPLICATOR_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden"
    JucePlugin_Name="COMPLICATOR"
    JucePlugin_VersionString="${PROJECT_VERSION}"
)

target_link_libraries(ComplicatorCheck
    PRIVATE
        complicator_engine
        ComplicatorFonts
        juce::juce_audio_processors
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
