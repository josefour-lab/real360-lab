#pragma once
// Tema propio de POLAR plugin (lo carga shared/Source/PolarLookAndFeel.h).
// Solo lo que es de este plugin: la base común está en shared/ y no se copia aquí.
// Estética «linograbado»: papel crema, tinta azul y sello rojo. El dibujo de la ventana está en
// Source/Linograbado.h y Source/PluginEditor.cpp; aquí solo los colores de los menús y controles comunes.
#define POLAR_TEMA_PROPIO 1

namespace PolarStyle::Propio
{
    constexpr int  modo = 4;                     // 4 POLAR (linograbado)
    constexpr bool claro = true;                 // papel
    constexpr const char* fuenteMono = "Menlo";
    constexpr bool botonOnConTinta = true;
    constexpr bool rejillaConTema = false;
    constexpr bool marcaRConAcento = false;

    inline void colores()
    {
        bg = juce::Colour (0xfff1ebdd);  panel = juce::Colour (0xffe9e1cf); border = juce::Colour (0xffd9d0bd);
        fg = juce::Colour (0xff2b4f7a);  muted = juce::Colour (0xff55534f); accent = juce::Colour (0xff3d8fdc);
        track = juce::Colour (0xffe2d9c5); title = juce::Colour (0xff3d8fdc); danger = juce::Colour (0xffd42a2a);
        seq[0] = juce::Colour (0xff3d8fdc); seq[1] = juce::Colour (0xffd42a2a);
        seq[2] = juce::Colour (0xff2b4f7a); seq[3] = juce::Colour (0xff55534f);
    }

    inline juce::Colour T (juce::Colour c, float, float, float) { return c; }
}
