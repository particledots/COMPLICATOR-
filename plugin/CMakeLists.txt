# The COMPLICATOR plugin wrapper: what Logic sees (AU / VST3), the controls and the panel.
# The sound itself lives in ../engine.
#
# The identifiers below were chosen once, for the first build, and must never change
# (Logic finds saved projects' plugins by them).

juce_add_plugin(Complicator
    COMPANY_NAME                "particledots"
    BUNDLE_ID                   com.particledots.complicator
    PLUGIN_MANUFACTURER_CODE    Pdot
    PLUGIN_CODE                 Cmpl
    FORMATS                     AU VST3
    PRODUCT_NAME                "COMPLICATOR"
    PLUGIN_NAME                 "COMPLICATOR"
    DESCRIPTION                 "Complex oscillator instrument (inspired by the Instruo Cs-L; not affiliated)"
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Instrument Synth
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice
)

target_sources(Complicator PRIVATE
    Source/Parameters.h
    Source/Parameters.cpp
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp
    Source/Controls.h
    Source/Controls.cpp
    Source/Style.h
)

target_compile_definitions(Complicator PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(Complicator
    PRIVATE
        complicator_engine
        juce::juce_audio_processors
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
