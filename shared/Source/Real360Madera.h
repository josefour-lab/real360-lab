#pragma once
// =====================================================================
//  REAL360 · Marco de madera de sinte vintage, para todos los plugins.
//
//  Laterales gruesos de nogal (veta vertical), listones finos arriba y abajo
//  (veta horizontal) con ingletes a 45°, canto biselado, una ranura oscura
//  donde la madera toca el panel y cuatro tornillos de latón en los laterales.
//  Todo se dibuja por código; solo depende de juce_gui_basics.
//
//  Uso en un editor (el contenido queda dentro, desplazado por el marco):
//
//    #include "Real360Madera.h"
//    real360::Madera madera;                              // miembro del editor
//    setSize (contentW + madera.extraW(), contentH + madera.extraH());
//    // resized(): maqueta el contenido en (0, 0, contentW, contentH) y luego
//    madera.shiftChildren (*this);                        // lo mete dentro del marco
//    // paint(): pinta el contenido con g trasladado y luego el marco
//    { juce::Graphics::ScopedSaveState s (g);
//      g.addTransform (madera.contentTransform()); pintarContenido (g); }
//    madera.paint (g, getLocalBounds());
//
//  Copiar este archivo tal cual en cada plugin (como Real360Sync.h).
// =====================================================================
#include <juce_gui_basics/juce_gui_basics.h>

namespace real360
{
class Madera
{
public:
    int side = 52;   // laterales (las mejillas del sinte)
    int rail = 16;   // listones de arriba y abajo

    int extraW() const { return side * 2; }
    int extraH() const { return rail * 2; }
    juce::AffineTransform contentTransform() const { return juce::AffineTransform::translation ((float) side, (float) rail); }
    juce::Rectangle<int> contentArea (juce::Rectangle<int> window) const { return window.reduced (side, rail); }

    // Mete dentro del marco a todos los hijos ya maquetados en coordenadas de contenido
    void shiftChildren (juce::Component& parent) const
    {
        for (auto* c : parent.getChildren())
            c->setBounds (c->getBounds().translated (side, rail));
    }

    void paint (juce::Graphics& g, juce::Rectangle<int> window)
    {
        if (cache.getWidth() != window.getWidth() || cache.getHeight() != window.getHeight())
            cache = render (window.getWidth(), window.getHeight());
        g.drawImageAt (cache, window.getX(), window.getY());
    }

private:
    juce::Image cache;

    // Nogal
    static juce::Colour base()  { return juce::Colour (0xff6a3b1f); }
    static juce::Colour light() { return juce::Colour (0xff94592f); }
    static juce::Colour dark()  { return juce::Colour (0xff331a0c); }

    // Tabla de madera con la veta a lo largo de x (se gira para los laterales)
    static juce::Image plank (int len, int thick, int seed)
    {
        juce::Image img (juce::Image::ARGB, juce::jmax (1, len), juce::jmax (1, thick), false);
        juce::Graphics g (img);
        juce::Random r (seed);
        g.setGradientFill (juce::ColourGradient (light(), 0.0f, 0.0f, base(), 0.0f, (float) thick, false));
        g.fillAll();

        // vetas: líneas onduladas a lo largo, de grosor y tono variables
        const int lines = juce::jmax (8, thick / 2);
        for (int i = 0; i < lines; ++i)
        {
            const float y0 = r.nextFloat() * (float) thick;
            const float amp = 0.6f + r.nextFloat() * 2.2f;
            const float freq = 0.004f + r.nextFloat() * 0.012f;
            const float ph = r.nextFloat() * 6.28f;
            juce::Path p;
            p.startNewSubPath (0.0f, y0);
            for (int x = 6; x <= len; x += 6)
                p.lineTo ((float) x, y0 + amp * std::sin ((float) x * freq + ph) + amp * 0.4f * std::sin ((float) x * freq * 3.1f));
            const bool darkLine = r.nextFloat() < 0.7f;
            g.setColour ((darkLine ? dark() : light().brighter (0.2f)).withAlpha (0.10f + r.nextFloat() * (darkLine ? 0.35f : 0.18f)));
            g.strokePath (p, juce::PathStrokeType (0.6f + r.nextFloat() * 1.6f));
        }
        // poros: rayitas cortas
        for (int i = 0; i < len * thick / 40; ++i)
        {
            const float x = r.nextFloat() * (float) len, y = r.nextFloat() * (float) thick;
            g.setColour (dark().withAlpha (0.12f + r.nextFloat() * 0.2f));
            g.fillRect (x, y, 2.0f + r.nextFloat() * 6.0f, 0.7f);
        }
        // algún nudo (ojo de la madera) en tablas largas
        if (len > 300)
        {
            const float kx = (float) len * (0.25f + 0.5f * r.nextFloat()), ky = (float) thick * (0.3f + 0.4f * r.nextFloat());
            for (int k = 6; k > 0; --k)
            {
                const float rx = (float) k * 4.5f, ry = (float) k * 1.6f;
                g.setColour (dark().withAlpha (0.10f + 0.04f * (float) (6 - k)));
                g.drawEllipse (kx - rx, ky - ry, rx * 2.0f, ry * 2.0f, 0.9f);
            }
        }
        // barniz: brillo suave arriba
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.10f), 0.0f, 0.0f,
                                                 juce::Colours::transparentWhite, 0.0f, (float) thick * 0.6f, false));
        g.fillAll();
        return img;
    }

    static void screw (juce::Graphics& g, juce::Point<float> c, float r, float angle)
    {
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillEllipse (c.x - r + 0.8f, c.y - r + 1.4f, r * 2.0f, r * 2.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe3c98a), c.x - r * 0.5f, c.y - r * 0.6f,
                                                 juce::Colour (0xff7a5a26), c.x + r * 0.6f, c.y + r * 0.8f, true));
        g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
        g.setColour (juce::Colour (0xff4a3412));
        g.drawEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f, 0.7f);
        const auto d = juce::Point<float> (std::cos (angle), std::sin (angle)) * (r * 0.75f);
        g.drawLine ({ c - d, c + d }, 1.4f);
    }

    juce::Image render (int w, int h) const
    {
        juce::Image img (juce::Image::ARGB, w, h, true);
        juce::Graphics g (img);
        const auto outer = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h);
        const auto inner = outer.reduced ((float) side, (float) rail);

        // Cuatro piezas con inglete: arriba, abajo, izquierda, derecha
        auto quad = [] (juce::Point<float> a, juce::Point<float> b, juce::Point<float> c, juce::Point<float> d)
        {
            juce::Path p;
            p.startNewSubPath (a); p.lineTo (b); p.lineTo (c); p.lineTo (d); p.closeSubPath();
            return p;
        };
        const juce::Path top    = quad (outer.getTopLeft(), outer.getTopRight(), inner.getTopRight(), inner.getTopLeft());
        const juce::Path bottom = quad (outer.getBottomLeft(), outer.getBottomRight(), inner.getBottomRight(), inner.getBottomLeft());
        const juce::Path left   = quad (outer.getTopLeft(), inner.getTopLeft(), inner.getBottomLeft(), outer.getBottomLeft());
        const juce::Path right  = quad (outer.getTopRight(), inner.getTopRight(), inner.getBottomRight(), outer.getBottomRight());

        const auto hPlank = plank (w, rail, 11), hPlank2 = plank (w, rail, 12);
        const auto vPlank = plank (h, side, 21), vPlank2 = plank (h, side, 22);

        auto fillWith = [&] (const juce::Path& p, const juce::Image& im, const juce::AffineTransform& t)
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (p);
            g.drawImageTransformed (im, t);
        };
        fillWith (top, hPlank, {});
        fillWith (bottom, hPlank2, juce::AffineTransform::translation (0.0f, (float) (h - rail)));
        // laterales: la tabla girada 90° (veta vertical)
        fillWith (left, vPlank, juce::AffineTransform::rotation (juce::MathConstants<float>::halfPi).translated ((float) side, 0.0f));
        fillWith (right, vPlank2, juce::AffineTransform::rotation (juce::MathConstants<float>::halfPi).translated ((float) w, 0.0f));

        // juntas de los ingletes
        g.setColour (dark().withAlpha (0.55f));
        g.drawLine ({ outer.getTopLeft(), inner.getTopLeft() }, 0.8f);
        g.drawLine ({ outer.getTopRight(), inner.getTopRight() }, 0.8f);
        g.drawLine ({ outer.getBottomLeft(), inner.getBottomLeft() }, 0.8f);
        g.drawLine ({ outer.getBottomRight(), inner.getBottomRight() }, 0.8f);

        // canto exterior biselado: luz arriba/izquierda, sombra abajo/derecha
        g.setColour (juce::Colours::white.withAlpha (0.16f));
        g.drawLine (0.5f, 0.5f, (float) w, 0.5f, 1.0f);
        g.drawLine (0.5f, 0.5f, 0.5f, (float) h, 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.drawLine (0.0f, (float) h - 0.5f, (float) w, (float) h - 0.5f, 1.0f);
        g.drawLine ((float) w - 0.5f, 0.0f, (float) w - 0.5f, (float) h, 1.0f);

        // ranura donde la madera toca el panel, y la sombra que cae sobre él
        g.setColour (juce::Colour (0xff1a0d05));
        g.drawRect (inner.expanded (1.0f), 2.0f);
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.drawRect (inner.expanded (3.0f), 1.0f);
        for (int i = 0; i < 6; ++i)
        {
            g.setColour (juce::Colours::black.withAlpha (0.10f - 0.015f * (float) i));
            g.drawRect (inner.reduced ((float) i), 1.0f);
        }

        // tornillos de latón en los laterales
        const float sr = juce::jmin (5.5f, (float) side * 0.15f);
        const float sx0 = (float) side * 0.5f, sx1 = (float) w - (float) side * 0.5f;
        screw (g, { sx0, (float) rail + 40.0f }, sr, 0.6f);
        screw (g, { sx0, (float) h - (float) rail - 40.0f }, sr, 2.1f);
        screw (g, { sx1, (float) rail + 40.0f }, sr, 1.2f);
        screw (g, { sx1, (float) h - (float) rail - 40.0f }, sr, 0.2f);
        return img;
    }
};
}  // namespace real360
