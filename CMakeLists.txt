cmake_minimum_required(VERSION 3.22)
project(Valvane VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Fetch JUCE 8.x via FetchContent
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.1
    GIT_SHALLOW    ON
)
FetchContent_MakeAvailable(JUCE)

# ---------------------------------------------------------------------------
# Plugin target definition
# ---------------------------------------------------------------------------
# Formats: VST3, AU (macOS-only, JUCE skips automatically elsewhere), Standalone
# AAX is intentionally omitted (requires licensed Avid SDK). To add AAX later,
# append "AAX" to the FORMATS list and link the AAX SDK via AAX_SDK_PATH.
juce_add_plugin(Valvane
    PRODUCT_NAME              "Valvane"
    COMPANY_NAME              "AJaudio"
    COMPANY_WEBSITE           "https://ajaudio.com"
    COMPANY_EMAIL             "support@ajaudio.com"
    BUNDLE_ID                 "com.ajaudio.valvane"
    PLUGIN_MANUFACTURER_CODE  Ajau
    PLUGIN_CODE               Vlvn
    FORMATS                   VST3 AU Standalone
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD   FALSE
    VST3_CATEGORIES           "Fx" "Dynamics"
    AU_MAIN_TYPE              "kAudioUnitType_Effect"
    PLUGIN_AU_EXPORT_PREFIX   ValvaneAU
)

juce_generate_juce_header(Valvane)

# ---------------------------------------------------------------------------
# Source files
# ---------------------------------------------------------------------------
target_sources(Valvane PRIVATE
    # Plugin core
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/PresetManager.cpp

    # DSP engines
    Source/DSP/EnvelopeDetector.cpp
    Source/DSP/GainComputer.cpp
    Source/DSP/SaturationStage.cpp
    Source/DSP/OptoEngine.cpp
    Source/DSP/FETEngine.cpp
    Source/DSP/VariMuEngine.cpp
    Source/DSP/VCAEngine.cpp

    # UI components
    Source/UI/CustomLookAndFeel.cpp
    Source/UI/VUMeterComponent.cpp
)

# ---------------------------------------------------------------------------
# Compile definitions
# ---------------------------------------------------------------------------
target_compile_definitions(Valvane PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

# ---------------------------------------------------------------------------
# Link JUCE modules
# ---------------------------------------------------------------------------
target_link_libraries(Valvane
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
