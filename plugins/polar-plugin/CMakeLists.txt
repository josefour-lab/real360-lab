cmake_minimum_required(VERSION 3.22)
# Para que Logic vea un cambio en los canales aceptados hay que subir esta versión
# (después, en Logic: Ajustes › Gestor de módulos › «Restablecer y volver a comprobar la selección»).
project(PolarPlugin VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "" FORCE)
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "" FORCE)

# JUCE: una sola copia para todo REAL360, en ThirdParty/JUCE
set(REAL360_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(SHARED ${REAL360_ROOT}/shared)
add_subdirectory(${REAL360_ROOT}/ThirdParty/JUCE ${CMAKE_BINARY_DIR}/JUCE)

# Mismo esquema que Looper Búho: efecto con entrada MIDI (en Logic se registra como efecto «aumf»).
# El nombre que verás en Logic es PLUGIN_NAME; para cambiarlo, edita las dos líneas PRODUCT_NAME y PLUGIN_NAME.
juce_add_plugin(PolarPlugin
    COMPANY_NAME "REAL360"
    PLUGIN_MANUFACTURER_CODE Cote
    PLUGIN_CODE Pol1
    FORMATS AU VST3
    PRODUCT_NAME "POLAR"
    PLUGIN_NAME "POLAR"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE)

juce_add_binary_data(PolarPluginData SOURCES ${SHARED}/Resources/real360.svg)

target_sources(PolarPlugin PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/PolarLink.cpp)

# Cabeceras: las del plugin (Source/, con su TemaPropio.h) y las comunes de shared/
target_include_directories(PolarPlugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source ${SHARED}/Source)

target_compile_features(PolarPlugin PRIVATE cxx_std_17)

target_compile_definitions(PolarPlugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(PolarPlugin
    PRIVATE
        PolarPluginData
        juce::juce_audio_utils
        juce::juce_audio_processors
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
