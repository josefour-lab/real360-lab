#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "PolarLookAndFeel.h"

// Ventana del plugin, estética «linograbado» (Source/Linograbado.h): el campo es una luna tallada en tinta azul,
// la pista un sello rojo que se mueve según lo que manda POLAR (aquí solo se mira), el estado va a lápiz
// y la pista se elige pulsando el sello rojo de abajo. Lleva el logo de REAL360.
class PolarEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PolarEditor (PolarProcessor&);
    ~PolarEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override {}
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void prepararImagenes (float escala);
    juce::Image selloImagen (int pista, bool grande) const;
    juce::Point<float> posicionPista() const;
    void elegirPista();

    PolarProcessor& proc;
    PolarLookAndFeel lnf;
    std::unique_ptr<juce::Drawable> logo;

    // Lo que se muestra (se repinta solo si cambia)
    PolarProtocol::TrackState shown;
    int pista = 0;
    bool alive = false, bound = true, quad = true;

    // Imágenes ya entintadas (se rehacen si cambia la escala de la pantalla, la pista o estéreo/cuadro)
    float escalaHecha = 0.0f;
    bool quadHecho = true;
    int pistaHecha = -1;
    juce::Image papel, tintaFija, selloCampo, selloSelector;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PolarEditor)
};
