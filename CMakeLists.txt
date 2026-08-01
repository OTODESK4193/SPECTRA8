cmake_minimum_required(VERSION 3.22)

# ============================================================================
#  SPECTRA8  -  Hybrid Formant Vocoder (OTODESK)
#  JUCE 8.0 系 / VST3 + Standalone
#  開発計画書 第2版準拠: C++スカラー設計（AVX2廃止）/ Ableton Live最適化
# ============================================================================
project(SPECTRA8 VERSION 1.0.0 LANGUAGES C CXX)

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
    PLUGIN_DESCRIPTION       "Hybrid Formant Vocoder"
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         TRUE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_AUTO_MANIFEST       FALSE
    COPY_PLUGIN_AFTER_BUILD  TRUE
    VERSION                  "1.0.0"
)

# ----------------------------------------------------------------------------
#  ソース
# ----------------------------------------------------------------------------
target_sources(SPECTRA8 PRIVATE
    # --- プラグインコア ---
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp

    # --- DSP モジュール ---
    Source/DSP/ModMatrix.h
    Source/DSP/Wavetable.h
    Source/DSP/PitchTracker.h
    Source/DSP/ExcitationEngine.h
    Source/DSP/ExcitationEngine.cpp
    Source/DSP/FilterbankVocoder.h
    Source/DSP/FilterbankVocoder.cpp
    Source/DSP/LpcAnalyzer.h
    Source/DSP/LpcAnalyzer.cpp
    Source/DSP/LpcLattice.h
    Source/DSP/LpcVocoder.h
    Source/DSP/LpcVocoder.cpp
    Source/DSP/LspConverter.h
    Source/DSP/PostBandEq.h
    Source/DSP/Limiter.h
    Source/DSP/FxChain.h
    Source/DSP/ScaleSnap.h
    Source/DSP/AnalyzerDSP.h
    Source/DSP/AnalyzerDSP.cpp

    # --- Presets モジュール ---
    Source/Presets/FactoryPresets.h
    Source/Presets/PresetManager.h

    # --- GUI モジュール ---
    Source/GUI/ColorPalette.h
    Source/GUI/ArcDial.h
    Source/GUI/ArcDial.cpp
    Source/GUI/ValueKnob.h
    Source/GUI/GlowToggle.h
    Source/GUI/ModRing.h
    Source/GUI/ModDestSelector.h
    Source/GUI/VocoderPanel.h
    Source/GUI/VocoderPanel.cpp
    Source/GUI/ExcitationPanel.h
    Source/GUI/ExcitationPanel.cpp
    Source/GUI/ModPanel.h
    Source/GUI/ModPanel.cpp
    Source/GUI/FxPanel.h
    Source/GUI/FxPanel.cpp
    Source/GUI/BandsEqPanel.h
    Source/GUI/BandsEqPanel.cpp
    Source/GUI/PresetPanel.h
    Source/GUI/PresetPanel.cpp
)

# ----------------------------------------------------------------------------
#  インクルードパス
# ----------------------------------------------------------------------------
target_include_directories(SPECTRA8 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/DSP
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/GUI
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
    SPECTRA8_VERSION="0.2.0"
)

# ----------------------------------------------------------------------------
#  JUCE ヘッダー生成
# ----------------------------------------------------------------------------
juce_generate_juce_header(SPECTRA8)

# ----------------------------------------------------------------------------
#  MSVC コンパイルオプション（スカラー設計: /arch:AVX2 は使用しない）
# ----------------------------------------------------------------------------
if(MSVC)
    target_compile_options(SPECTRA8 PRIVATE
        /fp:fast
        /utf-8
        /W3
        /wd4819
        /external:W0          # SYSTEMインクルードの警告を抑制
        $<$<CONFIG:Release>:/O2>
    )
endif()
