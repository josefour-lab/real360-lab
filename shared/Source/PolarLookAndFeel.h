#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// =====================================================================
//  Estética común de REAL360 (kit POLAR, nacida en Looper Búho): el diseño de POLAR
//  (tipografías, faders, botones y pad) con los temas comunes:
//  0 Normal (marrón), 1 Organelle (azul), 2 Blooper (amarillo), 3 Greenwoods (verde).
//
//  Tema propio de cada plugin: si el plugin tiene un Source/TemaPropio.h, se carga aquí.
//  Ese archivo es lo único de la estética que vive en el plugin; esta base no se copia
//  nunca dentro de un plugin. Forma de TemaPropio.h:
//
//      #pragma once
//      #define POLAR_TEMA_PROPIO 1
//      namespace PolarStyle::Propio
//      {
//          constexpr int  modo = 4;                 // número de tema que lo activa
//          constexpr bool claro = false;            // tema claro (papel) u oscuro
//          constexpr const char* fuenteMono = "Menlo";
//          constexpr bool botonOnConTinta = false;  // texto de botones encendidos en fg (temas claros)
//          constexpr bool rejillaConTema = false;   // la rejilla del pad sigue el tema (T())
//          constexpr bool marcaRConAcento = false;  // la marca «R» del pad en el color de acento
//          inline void colores() { ... }            // pone bg, panel, fg… y seq[]
//          inline juce::Colour T (juce::Colour c, float h, float sat, float b) { ... }  // tonos fijos → tema
//      }
// =====================================================================
namespace PolarStyle
{
    // Paleta activa (cambia con el modo: Normal marrón, Organelle rojo, Blooper amarillo)
    inline juce::Colour bg      { 0xff0e0907 };
    inline juce::Colour panel   { 0xff1a120d };
    inline juce::Colour border  { 0xff3a2a1f };
    inline juce::Colour fg      { 0xfff1e9df };
    inline juce::Colour muted   { 0xffa08b78 };
    inline juce::Colour accent  { 0xffd63a2c };
    inline juce::Colour track   { 0xff0a0605 };
    inline juce::Colour title   { 0xfffbf6f0 };
    inline juce::Colour danger  { 0xffe0533f };

    // Colores de las secuencias A, B, C y D
    inline juce::Colour seq[4] = { juce::Colour (0xffd63a2c), juce::Colour (0xfff1e9df),
                                   juce::Colour (0xffc8894a), juce::Colour (0xffa26a43) };

    inline int themeMode = 0;   // 0 Normal (marrón), 1 Organelle (azul), 2 Blooper (amarillo), 3 Greenwoods (verde)
}

// ---------------------------------------------------------------- tema propio del plugin
#if __has_include("TemaPropio.h")
 #include "TemaPropio.h"
#endif

#ifndef POLAR_TEMA_PROPIO
namespace PolarStyle::Propio
{
    constexpr int  modo = -1;                    // sin tema propio
    constexpr bool claro = false;
    constexpr const char* fuenteMono = "Menlo";
    constexpr bool botonOnConTinta = false;
    constexpr bool rejillaConTema = false;
    constexpr bool marcaRConAcento = false;
    inline void colores() {}
    inline juce::Colour T (juce::Colour c, float, float, float) { return c; }
}
#endif

namespace PolarStyle
{
    inline bool temaPropioActivo() { return Propio::modo >= 0 && themeMode == Propio::modo; }

    // Tema claro (papel) u oscuro: los comunes son oscuros
    inline bool isLight() { return temaPropioActivo() && Propio::claro; }

    // Blanco o negro de realce según el tema (aros, textos sobre fondos de color…)
    inline juce::Colour hi() { return isLight() ? fg : juce::Colours::white; }

    // Los tonos fijos de la interfaz (rejilla, altavoces, etiquetas…) están pensados en marrón
    // oscuro; T() los lleva al tema activo: otro tono (Blooper) o invertidos a claro (Organelle)
    inline juce::Colour T (juce::Colour c)
    {
        if (themeMode == 0)
            return c;
        float h, sat, b;
        c.getHSB (h, sat, b);
        if (temaPropioActivo())
            return Propio::T (c, h, sat, b);
        if (sat > 0.75f || b > 0.80f)
            return c;   // colores vivos o claros (acentos, blancos): se quedan
        const float hue = themeMode == 1 ? 0.585f : themeMode == 3 ? 0.27f : 0.115f;   // azul, verde o amarillo
        const float satMul = themeMode == 1 ? 1.7f : themeMode == 3 ? 1.4f : 1.35f;
        return juce::Colour::fromHSV (hue, juce::jlimit (0.0f, 1.0f, sat * satMul), b, c.getFloatAlpha());
    }

    inline void applyTheme (int mode)
    {
        themeMode = juce::jlimit (0, juce::jmax (3, Propio::modo), mode);
        if (temaPropioActivo())
        {
            Propio::colores();
        }
        else if (themeMode == 1)
        {
            // Organelle: azul, como el propio Organelle, con la madera de sus teclas
            bg = juce::Colour (0xff070b12);  panel = juce::Colour (0xff0d1522); border = juce::Colour (0xff20364f);
            fg = juce::Colour (0xffeaf1f8);  muted = juce::Colour (0xff8098b3); accent = juce::Colour (0xff3f8fd6);
            track = juce::Colour (0xff04070c); title = juce::Colour (0xfff3f7fb); danger = juce::Colour (0xffe8774a);
            seq[0] = juce::Colour (0xffe0915a); seq[1] = juce::Colour (0xff7fb8e6);
            seq[2] = juce::Colour (0xfff1ede4); seq[3] = juce::Colour (0xffc8894a);
        }
        else if (themeMode == 3)
        {
            // Greenwoods: verdes y marrones
            bg = juce::Colour (0xff080c07);  panel = juce::Colour (0xff121a0f); border = juce::Colour (0xff2e3f22);
            fg = juce::Colour (0xffeef2e4);  muted = juce::Colour (0xff97a585); accent = juce::Colour (0xff6fae4a);
            track = juce::Colour (0xff050804); title = juce::Colour (0xfff6f8ef); danger = juce::Colour (0xffd63a2c);
            seq[0] = juce::Colour (0xff6fae4a); seq[1] = juce::Colour (0xffc8894a);
            seq[2] = juce::Colour (0xffb7d67a); seq[3] = juce::Colour (0xff8a6a43);
        }
        else if (themeMode == 2)
        {
            // Blooper: amarillo, con toques de rojo y marrón
            bg = juce::Colour (0xff14100a);  panel = juce::Colour (0xff231a0b); border = juce::Colour (0xff4d3c17);
            fg = juce::Colour (0xfffaf1dc);  muted = juce::Colour (0xffbaa47a); accent = juce::Colour (0xfff2c230);
            track = juce::Colour (0xff0d0a04); title = juce::Colour (0xfffffaf0); danger = juce::Colour (0xffd63a2c);
            seq[0] = juce::Colour (0xfff2c230); seq[1] = juce::Colour (0xffd63a2c);
            seq[2] = juce::Colour (0xfffaf1dc); seq[3] = juce::Colour (0xffc8894a);
        }
        else
        {
            // Normal: marrones
            bg = juce::Colour (0xff0e0907);  panel = juce::Colour (0xff1a120d); border = juce::Colour (0xff3a2a1f);
            fg = juce::Colour (0xfff1e9df);  muted = juce::Colour (0xffa08b78); accent = juce::Colour (0xffd63a2c);
            track = juce::Colour (0xff0a0605); title = juce::Colour (0xfffbf6f0); danger = juce::Colour (0xffe0533f);
            seq[0] = juce::Colour (0xffd63a2c); seq[1] = juce::Colour (0xfff1e9df);
            seq[2] = juce::Colour (0xffc8894a); seq[3] = juce::Colour (0xffa26a43);
        }
    }

    inline juce::Font mono (float size, bool bold = false)
    {
        auto f = juce::Font (juce::FontOptions (temaPropioActivo() ? Propio::fuenteMono : "Menlo", size,
                                                bold ? juce::Font::bold : juce::Font::plain));
        return f;
    }

    inline juce::Font ui (float size, bool bold = false)
    {
        return juce::Font (juce::FontOptions ("Helvetica Neue", size, bold ? juce::Font::bold : juce::Font::plain));
    }

    inline juce::Font titleFont (float size)
    {
        return juce::Font (juce::FontOptions ("Helvetica Neue", size, juce::Font::plain).withStyle ("Light"));
    }

    // Ancho de un texto (en JUCE 9 ya no está Font::getStringWidthFloat)
    inline float textWidth (const juce::Font& f, const juce::String& t)
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (f, t, 0.0f, 0.0f);
        return ga.getBoundingBox (0, -1, true).getWidth();
    }

    inline float spacedWidth (const juce::String& text, const juce::Font& font, float spacing)
    {
        float x = 0.0f;
        for (auto c : text)
            x += textWidth (font, juce::String::charToString (c)) + spacing;
        return juce::jmax (0.0f, x - spacing);
    }

    // Texto con espaciado entre letras (las etiquetas de POLAR van en mayúsculas espaciadas)
    inline void drawSpaced (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area,
                            const juce::Font& font, float spacing, juce::Justification just)
    {
        juce::GlyphArrangement ga;
        float x = 0.0f;
        for (auto c : text)
        {
            const juce::String ch = juce::String::charToString (c);
            ga.addLineOfText (font, ch, x, 0.0f);
            x += textWidth (font, ch) + spacing;
        }
        const float w = x - spacing;
        float x0 = area.getX();
        if (just.testFlags (juce::Justification::horizontallyCentred)) x0 = area.getCentreX() - w * 0.5f;
        else if (just.testFlags (juce::Justification::right))          x0 = area.getRight() - w;
        const float y0 = area.getCentreY() + font.getAscent() * 0.5f - font.getDescent() * 0.25f;
        ga.moveRangeOfGlyphs (0, ga.getNumGlyphs(), x0, y0);
        ga.draw (g);
    }
}

class PolarLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PolarLookAndFeel() { applyThemeColours(); }

    // Vuelve a aplicar la paleta activa (al cambiar de modo)
    void applyThemeColours()
    {
        using namespace PolarStyle;
        setColour (juce::ResizableWindow::backgroundColourId, bg);
        setColour (juce::Label::textColourId, muted);
        setColour (juce::TextButton::buttonColourId, panel);
        setColour (juce::TextButton::buttonOnColourId, accent);
        setColour (juce::TextButton::textColourOffId, fg);
        setColour (juce::TextButton::textColourOnId, temaPropioActivo() && Propio::botonOnConTinta ? fg : bg);
        setColour (juce::ToggleButton::textColourId, fg);
        setColour (juce::ToggleButton::tickColourId, accent);
        setColour (juce::ComboBox::backgroundColourId, panel);
        setColour (juce::ComboBox::outlineColourId, border);
        setColour (juce::ComboBox::textColourId, fg);
        setColour (juce::ComboBox::arrowColourId, muted);
        setColour (juce::PopupMenu::backgroundColourId, panel);
        setColour (juce::PopupMenu::textColourId, fg);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.18f));
        setColour (juce::PopupMenu::highlightedTextColourId, hi());
        setColour (juce::Slider::textBoxTextColourId, fg);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxHighlightColourId, accent.withAlpha (0.3f));
        setColour (juce::TextEditor::backgroundColourId, panel);
        setColour (juce::TextEditor::textColourId, fg);
        setColour (juce::TextEditor::highlightColourId, accent.withAlpha (0.3f));
        setColour (juce::CaretComponent::caretColourId, accent);
        setColour (juce::AlertWindow::backgroundColourId, panel);
        setColour (juce::AlertWindow::textColourId, fg);
    }

    // ---------------------------------------------------------------- tipografías
    juce::Font getLabelFont (juce::Label& l) override { return PolarStyle::mono (juce::jmax (10.0f, l.getFont().getHeight())); }
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return PolarStyle::mono (juce::jmin (12.5f, (float) h * 0.48f)); }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return PolarStyle::mono (12.0f); }
    juce::Font getPopupMenuFont() override { return PolarStyle::mono (12.5f); }

    // ---------------------------------------------------------------- faders
    // Como el input[type=range] de POLAR: carril redondeado y hundido, relleno turquesa
    // hasta el tirador, y tirador plateado.
    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle style, juce::Slider& s) override
    {
        using namespace PolarStyle;
        if (style == juce::Slider::LinearVertical)
        {
            // Fader vertical (mezclador): carril hundido, relleno rojo desde abajo, tirador crema
            const float wTrack = juce::jmin (16.0f, (float) width);
            auto r = juce::Rectangle<float> ((float) x + ((float) width - wTrack) * 0.5f, (float) y, wTrack, (float) height);
            const float alpha = s.isEnabled() ? 1.0f : 0.4f;
            g.setColour (track.withMultipliedAlpha (alpha));
            g.fillRoundedRectangle (r, wTrack * 0.5f);
            g.setColour (accent.withAlpha (0.34f * alpha));
            g.fillRoundedRectangle (juce::Rectangle<float>::leftTopRightBottom (r.getX(), sliderPos, r.getRight(), r.getBottom()), wTrack * 0.5f);
            g.setColour (border.withMultipliedAlpha (alpha));
            g.drawRoundedRectangle (r.reduced (0.5f), wTrack * 0.5f, 1.0f);
            auto thumb = juce::Rectangle<float> (26.0f, 12.0f).withCentre ({ r.getCentreX(), juce::jlimit (r.getY() + 6.0f, r.getBottom() - 6.0f, sliderPos) });
            g.setColour (juce::Colours::black.withAlpha (0.6f));
            g.fillRoundedRectangle (thumb.translated (0.0f, 1.0f), 4.0f);
            g.setColour (PolarStyle::T (juce::Colour (0xffe8dccd)).withMultipliedAlpha (alpha));
            g.fillRoundedRectangle (thumb, 4.0f);
            g.setColour (accent.withMultipliedAlpha (alpha));
            g.fillRect (thumb.withSizeKeepingCentre (thumb.getWidth() - 8.0f, 1.5f));
            return;
        }

        if (style != juce::Slider::LinearHorizontal && style != juce::Slider::TwoValueHorizontal)
        {
            LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, s);
            return;
        }

        const float h = juce::jmin (20.0f, (float) height);
        auto r = juce::Rectangle<float> ((float) x, (float) y + ((float) height - h) * 0.5f, (float) width, h);
        const float rad = h * 0.5f;
        const float alpha = s.isEnabled() ? 1.0f : 0.4f;

        // carril
        g.setColour (track.withMultipliedAlpha (alpha));
        g.fillRoundedRectangle (r, rad);

        // relleno turquesa
        const bool two = style == juce::Slider::TwoValueHorizontal;
        const float from = two ? minSliderPos : r.getX();
        const float to   = two ? maxSliderPos : sliderPos;
        {
            juce::Graphics::ScopedSaveState ss (g);
            juce::Path clip;
            clip.addRoundedRectangle (r, rad);
            g.reduceClipRegion (clip);
            g.setColour (accent.withAlpha (0.34f * alpha));
            g.fillRect (juce::Rectangle<float>::leftTopRightBottom (from, r.getY(), to, r.getBottom()));
            // sombra interior (hundido)
            g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.55f), 0.0f, r.getY(),
                                                     juce::Colours::transparentBlack, 0.0f, r.getY() + 6.0f, false));
            g.fillRect (r);
        }
        g.setColour (border.withMultipliedAlpha (alpha));
        g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.0f);

        // tirador(es)
        auto thumb = [&] (float cx)
        {
            const float d = 16.0f;
            auto t = juce::Rectangle<float> (d, d).withCentre ({ cx, r.getCentreY() });
            g.setColour (juce::Colours::black.withAlpha (0.6f));
            g.fillEllipse (t.translated (0.0f, 1.0f).expanded (1.0f));
            juce::ColourGradient cg (PolarStyle::T (juce::Colour (0xfffbf3ea)).withMultipliedAlpha (alpha), t.getX() + d * 0.35f, t.getY() + d * 0.28f,
                                     PolarStyle::T (juce::Colour (0xffa8917a)).withMultipliedAlpha (alpha), t.getRight(), t.getBottom(), true);
            cg.addColour (0.7, PolarStyle::T (juce::Colour (0xffd8c7b4)).withMultipliedAlpha (alpha));
            g.setGradientFill (cg);
            g.fillEllipse (t);
            g.setColour (juce::Colours::white.withAlpha (0.12f * alpha));
            g.drawEllipse (t.reduced (0.5f), 1.0f);
        };

        const float lo = r.getX() + 9.0f, hi = r.getRight() - 9.0f;
        if (two)
        {
            thumb (juce::jlimit (lo, hi, minSliderPos));
            thumb (juce::jlimit (lo, hi, maxSliderPos));
        }
        else
        {
            thumb (juce::jlimit (lo, hi, sliderPos));
        }
    }

    int getSliderThumbRadius (juce::Slider&) override { return 9; }

    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = LookAndFeel_V4::createSliderTextBox (s);
        l->setFont (PolarStyle::mono (12.0f));
        l->setColour (juce::Label::textColourId, PolarStyle::fg);
        l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
        l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        l->setJustificationType (juce::Justification::centredRight);
        return l;
    }

    // ---------------------------------------------------------------- botones
    // Botones de POLAR: panel oscuro, borde fino, esquinas de 4–6 px.
    // Con la propiedad «polarTab» se dibujan como los botones de pista de POLAR:
    // franja de color arriba, fondo teñido y, si están activos, borde de color con brillo.
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bgColour,
                               bool over, bool down) override
    {
        using namespace PolarStyle;
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const float alpha = b.isEnabled() ? 1.0f : 0.35f;

        if (b.getProperties()["polarTab"])
        {
            const auto c = b.findColour (juce::TextButton::buttonOnColourId);
            const bool on = b.getToggleState();
            if (on)
            {
                g.setColour (c.withAlpha (0.25f * alpha));
                g.drawRoundedRectangle (r.expanded (1.5f), 9.0f, 4.0f); // brillo
            }
            g.setColour (panel.withMultipliedAlpha (alpha));
            g.fillRoundedRectangle (r, 8.0f);
            g.setColour (c.withAlpha ((on ? 0.20f : 0.07f) * alpha));
            g.fillRoundedRectangle (r, 8.0f);
            {
                juce::Graphics::ScopedSaveState ss (g);
                juce::Path clip;
                clip.addRoundedRectangle (r, 8.0f);
                g.reduceClipRegion (clip);
                g.setColour (c.withMultipliedAlpha (alpha));
                g.fillRect (r.withHeight (3.0f));
            }
            g.setColour (c.withAlpha ((on ? 1.0f : 0.3f) * alpha));
            g.drawRoundedRectangle (r, 8.0f, on ? 1.5f : 1.0f);
            return;
        }

        // botones normales; si tienen un color propio (grabar, exportar…), se usa como acento
        const bool custom = bgColour != panel;
        auto fill = custom ? bgColour.withAlpha (0.22f) : panel;
        if (over) fill = fill.brighter (0.12f);
        if (down) fill = fill.brighter (0.25f);
        g.setColour (panel.withMultipliedAlpha (alpha));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (fill.withMultipliedAlpha (alpha));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour ((custom ? bgColour : border).withMultipliedAlpha (alpha));
        g.drawRoundedRectangle (r, 5.0f, custom ? 1.5f : 1.0f);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        using namespace PolarStyle;
        const bool tab = (bool) b.getProperties()["polarTab"];
        const auto colour = tab ? (b.getToggleState() ? hi() : b.findColour (juce::TextButton::buttonOnColourId))
                                : fg;
        g.setColour (colour.withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.4f));
        g.setFont (tab ? mono (13.0f, true) : getTextButtonFont (b, b.getHeight()));
        g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (6, 2), juce::Justification::centred, 2);
    }

    // casillas: cuadradito con el acento de POLAR
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool) override
    {
        using namespace PolarStyle;
        const float alpha = b.isEnabled() ? 1.0f : 0.4f;
        auto box = juce::Rectangle<float> (2.0f, ((float) b.getHeight() - 16.0f) * 0.5f, 16.0f, 16.0f);
        g.setColour (panel.withMultipliedAlpha (alpha));
        g.fillRoundedRectangle (box, 4.0f);
        if (b.getToggleState())
        {
            g.setColour (accent.withAlpha (0.14f * alpha));
            g.fillRoundedRectangle (box, 4.0f);
            g.setColour (accent.withMultipliedAlpha (alpha));
            g.fillRoundedRectangle (box.reduced (4.0f), 2.0f);
        }
        g.setColour ((b.getToggleState() ? accent : (over ? muted : border)).withMultipliedAlpha (alpha));
        g.drawRoundedRectangle (box, 4.0f, 1.0f);

        g.setColour (fg.withMultipliedAlpha (alpha));
        g.setFont (mono (12.0f));
        g.drawFittedText (b.getButtonText(), b.getLocalBounds().withTrimmedLeft (26), juce::Justification::centredLeft, 1);
    }

    // ---------------------------------------------------------------- menús
    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        using namespace PolarStyle;
        auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (0.5f);
        const float alpha = box.isEnabled() ? 1.0f : 0.4f;
        g.setColour (panel.withMultipliedAlpha (alpha));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour ((box.hasKeyboardFocus (true) ? accent : border).withMultipliedAlpha (alpha));
        g.drawRoundedRectangle (r, 4.0f, 1.0f);

        juce::Path arrow;
        const float ax = (float) w - 16.0f, ay = (float) h * 0.5f;
        arrow.startNewSubPath (ax - 4.0f, ay - 2.0f);
        arrow.lineTo (ax, ay + 2.5f);
        arrow.lineTo (ax + 4.0f, ay - 2.0f);
        g.setColour (muted.withMultipliedAlpha (alpha));
        g.strokePath (arrow, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (8, 1, box.getWidth() - 30, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }

    void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
    {
        g.fillAll (PolarStyle::panel);
        g.setColour (PolarStyle::border);
        g.drawRect (0, 0, w, h, 1);
    }
};
