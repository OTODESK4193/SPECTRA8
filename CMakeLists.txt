cmake_minimum_required(VERSION 3.22)

# ============================================================================
#  SPECTRA8  -  Formant-Separation Voice Modeling Synthesizer (OTODESK)
#  JUCE 8.0 系 / VST3 + Standalone
#  8-voice polyphonic LPC synthesis engine / Character (Lo-Fi <-> Hi-Fi)
#  最適化: AVX2 SIMD, /fp:fast
# ============================================================================
project(SPECTRA8 VERSION 0.1.0 LANGUAGES C CXX)

# ----------------------------------------------------------------------------
#  C++20
# ----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ----------------------------------------------------------------------------
#  JUCE の場所
# ----------------------------------------------------------------------------
set(JUCE_PATH "C:/JUCE" CACHE PATH "Path to the JUCE checkout")
add_subdirectory(${JUCE_PATH} ${CMAKE_BINARY_DIR}/JUCE)

# ----------------------------------------------------------------------------
#  プラグイン定義
# ----------------------------------------------------------------------------
juce_add_plugin(SPECTRA8
    COMPANY_NAME             "OTODESK"
    PLUGIN_MANUFACTURER_CODE "Otdk"
    PLUGIN_CODE              "Sp8v"
    FORMATS                  VST3 Standalone
    PRODUCT_NAME             "SPECTRA8"
    PLUGIN_DESCRIPTION       "Formant-Separation Voice Modeling Synthesizer"
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         TRUE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_AUTO_MANIFEST       FALSE
    COPY_PLUGIN_AFTER_BUILD  TRUE
    VERSION                  "0.1.0"
)

# ----------------------------------------------------------------------------
#  ソース (Sourceフォルダ経由)
# ----------------------------------------------------------------------------
target_sources(SPECTRA8 PRIVATE
    # --- プラグインコア ---
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp

    # --- DSP モジュール ---
    Source/DSP/SimdUtils.h
    Source/DSP/VoiceState.h
    Source/DSP/MidiQueue.h
    Source/DSP/PitchDetector.h
    Source/DSP/PitchDetector.cpp
    Source/DSP/LPCAnalyzer.h
    Source/DSP/LPCAnalyzer.cpp
    Source/DSP/TrueEnvelope.h
    Source/DSP/TrueEnvelope.cpp
    Source/DSP/BarkFilterBank.h
    Source/DSP/BarkFilterBank.cpp
    Source/DSP/TELPCIntegrator.h
    Source/DSP/TELPCIntegrator.cpp
    Source/DSP/LPCtoLSP.h
    Source/DSP/LPCtoLSP.cpp
    Source/DSP/LSPtoLPC.h
    Source/DSP/LSPtoLPC.cpp
    Source/DSP/FormantShifter.h
    Source/DSP/FormantShifter.cpp
    Source/DSP/MultiRateMapper.h
    Source/DSP/MultiRateMapper.cpp
    Source/DSP/OscillatorBank.h
    Source/DSP/OscillatorBank.cpp
    Source/DSP/NoiseGenerator.h
    Source/DSP/CharacterProcessor.h
    Source/DSP/VoiceManager.h
    Source/DSP/VoiceManager.cpp

    # --- GUI モジュール ---
    Source/GUI/ColorPalette.h
    Source/GUI/ArcDial.h
    Source/GUI/ArcDial.cpp
    Source/GUI/MainPanel.h
    Source/GUI/MainPanel.cpp
    Source/GUI/ValueKnob.h
    Source/GUI/GlowToggle.h
)

# ----------------------------------------------------------------------------
#  インクルードパス
# ----------------------------------------------------------------------------
target_include_directories(SPECTRA8 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/DSP
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/GUI
)

# サードパーティは SYSTEM 扱い (/external:I) にして警告対象から外す
target_include_directories(SPECTRA8 SYSTEM PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/third_party
)

# ----------------------------------------------------------------------------
#  JUCE モジュール
# ----------------------------------------------------------------------------
target_link_libraries(SPECTRA8
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ----------------------------------------------------------------------------
#  コンパイル定義
# ----------------------------------------------------------------------------
target_compile_definitions(SPECTRA8 PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    SPECTRA8_VERSION="0.1.0"
)

# ----------------------------------------------------------------------------
#  JUCE ヘッダー生成
# ----------------------------------------------------------------------------
juce_generate_juce_header(SPECTRA8)

# ----------------------------------------------------------------------------
#  MSVC コンパイルオプション (AVX2 SIMD 有効)
# ----------------------------------------------------------------------------
if(MSVC)
    target_compile_options(SPECTRA8 PRIVATE
        /arch:AVX2
        /fp:fast
        /utf-8
        /W3
        /wd4819
        /external:W0          # SYSTEMインクルードの警告を抑制
        $<$<CONFIG:Release>:/O2>
    )
endif()
