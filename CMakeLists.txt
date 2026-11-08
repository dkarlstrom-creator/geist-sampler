cmake_minimum_required(VERSION 3.22)
project(GeistPad VERSION 0.1.0 LANGUAGES CXX)
if(NOT DEFINED JUCE_DIR OR NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
  message(FATAL_ERROR "Set -DJUCE_DIR=/path/to/JUCE")
endif()
add_subdirectory("${JUCE_DIR}" JUCE-build)
juce_add_plugin(GeistPad
  COMPANY_NAME "GeistPad"
  BUNDLE_ID "com.geistpad.sampler"
  PLUGIN_MANUFACTURER_CODE GsPd
  PLUGIN_CODE Gp01
  FORMATS VST3
  PRODUCT_NAME "GeistPad"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE)
juce_generate_juce_header(GeistPad)
target_sources(GeistPad PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_compile_features(GeistPad PRIVATE cxx_std_17)
target_compile_definitions(GeistPad PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(GeistPad PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
