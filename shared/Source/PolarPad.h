#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "SpatialPoint.h"
#include "QuadPanner.h"
#include "PolarLookAndFeel.h"

// =====================================================================
//  Kit POLAR (REAL360): pad cuadrafónico/estéreo de Looper Búho, listo
//  para cualquier plugin. El pad no sabe nada del plugin: le pregunta
//  todo a un PolarHost, que implementas en tu editor.
//  Fuente 0 = la principal (en Looper Búho, la guitarra en directo «V»).
// =====================================================================
struct PolarHost
{
    virtual ~PolarHost() = default;

    virtual int numSources() const = 0;                       // cuántas fuentes hay (1 o más)
    virtual SpatialPoint& point (int src) = 0;                // la posición de cada fuente
    virtual bool isSourceActive (int src) const { return src == 0; }
    virtual juce::String sourceLabel (int src) const { return src == 0 ? "V" : juce::String (src); }
    // Nombre largo de la fuente (globo junto al punto). Por defecto, el de Looper Búho.
    virtual juce::String sourceName (int src) const
    {
        return src == 0 ? juce::String::fromUTF8 ("Guitarra en directo") : juce::String::fromUTF8 ("Toma ") + sourceLabel (src);
    }
    virtual juce::Colour sourceColour (int) const { return PolarStyle::fg; }
    virtual bool isSourceDimmed (int) const { return false; }        // se dibuja más tenue
    virtual bool isSourceRecording (int) const { return false; }     // aro rojo de grabación

    virtual bool isQuadOutput() const = 0;                    // salida de 4 canales (o 2)
    virtual int panMode() const { return QuadPanner::PanBalance; }
    virtual bool isProtected() const { return false; }        // «Proteger el centro» (solo con apertura)
    virtual juce::StringArray movementNames() const { return { "Fijo" }; }

    virtual float progress() const { return -1.0f; }          // barra bajo el pad (0..1); < 0 = sin barra
    virtual juce::Colour progressColour() const { return PolarStyle::accent; }
    virtual juce::String statusText() const { return {}; }    // línea de estado bajo el pad
};

class PolarPad : public juce::Component
{
public:
    explicit PolarPad (PolarHost& h) : host (h) {}

    std::function<void (int)> onSelect;   // al elegir una fuente con el ratón
    std::function<void()> onInteract;     // al arrastrar (por si quieres refrescar controles)
    int selected = 0;

    bool isActive (int src);
    juce::Colour colourFor (int src);
    juce::String labelFor (int src);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    SpatialPoint* pointFor (int src);
    juce::Rectangle<float> padRect() const;
    juce::Rectangle<float> fieldRect() const;
    float shownRadius (float r) const;
    juce::Point<float> toScreen (float angleDeg, float radius) const;
    void paintGrid (juce::Graphics&, juce::Rectangle<float> f);
    void paintSpeakers (juce::Graphics&, juce::Rectangle<float> f);

    PolarHost& host;
    SpatialPoint* dragging = nullptr;
};
