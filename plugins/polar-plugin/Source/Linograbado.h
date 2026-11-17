#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

// =====================================================================
//  Estética «linograbado» de POLAR plugin: papel crema, tinta azul
//  tallada y sellos rojos. Solo de este plugin (la base común sigue en
//  shared/). La tinta se dibuja como máscara (blanco sobre negro) y
//  luego se «entinta»: color + grano irregular, como al estampar.
// =====================================================================
namespace Lino
{
    const juce::Colour tinta   { 0xff3d8fdc };
    const juce::Colour papel   { 0xfff1ebdd };
    const juce::Colour rojo    { 0xffd42a2a };
    const juce::Colour lapiz   { 0xff55534f };
    const juce::Colour plancha { 0xffd9d0bd };   // huella de la plancha
    const juce::Colour brillo  { 0xfffbf8f1 };

    // Letras del sistema (vienen con todos los Mac)
    inline juce::Font futura (float pt)
    {
        return juce::Font (juce::FontOptions().withName ("Futura").withStyle ("Bold").withPointHeight (pt));
    }
    inline juce::Font aLapiz (float pt)
    {
        return juce::Font (juce::FontOptions().withName ("Bradley Hand").withStyle ("Bold").withPointHeight (pt));
    }

    // ---- ruido (para el papel y el grano de la tinta)
    inline float hash (int x, int y, int seed)
    {
        uint32_t h = (uint32_t) x * 374761393u + (uint32_t) y * 668265263u + (uint32_t) seed * 2246822519u;
        h = (h ^ (h >> 13)) * 1274126177u;
        return (float) ((h ^ (h >> 16)) & 0xffffff) / 16777215.0f;
    }
    inline float ruido (float x, float y, int seed)
    {
        const int xi = (int) std::floor (x), yi = (int) std::floor (y);
        float fx = x - (float) xi, fy = y - (float) yi;
        fx = fx * fx * (3.0f - 2.0f * fx);
        fy = fy * fy * (3.0f - 2.0f * fy);
        const float a = hash (xi, yi, seed), b = hash (xi + 1, yi, seed);
        const float c = hash (xi, yi + 1, seed), d = hash (xi + 1, yi + 1, seed);
        return a + (b - a) * fx + (c - a) * fy + (a - b - c + d) * fx * fy;
    }

    // Papel crema con su fibra. s = píxeles reales por punto (2 en pantallas Retina).
    inline juce::Image papelImagen (int w, int h, float s)
    {
        juce::Image img (juce::Image::RGB, juce::roundToInt ((float) w * s), juce::roundToInt ((float) h * s), false);
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < bd.height; ++y)
            for (int x = 0; x < bd.width; ++x)
            {
                const float fino = ruido ((float) x / (0.9f * s), (float) y / (0.9f * s), 3);
                const float fibra = ruido ((float) x / (5.0f * s), (float) y / (2.2f * s), 4);
                const float k = 1.0f - 0.03f * fino - 0.02f * fibra;
                bd.setPixelColour (x, y, juce::Colour::fromFloatRGBA (papel.getFloatRed() * k, papel.getFloatGreen() * k,
                                                                      papel.getFloatBlue() * k, 1.0f));
            }
        return img;
    }

    // Máscara vacía (negra) para dibujar en blanco lo que llevará tinta, en puntos lógicos
    struct Mascara
    {
        Mascara (int w, int h, float s)
            : img (juce::Image::RGB, juce::roundToInt ((float) w * s), juce::roundToInt ((float) h * s), true), g (img)
        {
            g.addTransform (juce::AffineTransform::scale (s));
            g.setColour (juce::Colours::white);
        }
        juce::Image img;
        juce::Graphics g;
    };

    // Convierte la máscara en tinta del color dado, con grano y carga irregular
    inline juce::Image entintar (const juce::Image& mask, juce::Colour c, float s, int seed, float grano = 1.0f)
    {
        juce::Image out (juce::Image::ARGB, mask.getWidth(), mask.getHeight(), true);
        juce::Image::BitmapData in (mask, juce::Image::BitmapData::readOnly);
        juce::Image::BitmapData od (out, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < in.height; ++y)
            for (int x = 0; x < in.width; ++x)
            {
                const float a = in.getPixelColour (x, y).getFloatRed();
                if (a <= 0.0f)
                    continue;
                const float fino = 0.6f * ruido ((float) x / (0.7f * s), (float) y / (0.7f * s), seed)
                                 + 0.4f * hash (x, y, seed + 7);
                const float carga = 0.9f + 0.1f * ruido ((float) x / (38.0f * s), (float) y / (38.0f * s), seed + 1);
                const float k = juce::jlimit (0.0f, 1.0f, 1.0f + grano * (0.9f - 2.0f * fino * fino));
                od.setPixelColour (x, y, c.withAlpha (a * juce::jlimit (0.0f, 1.0f, k) * carga));
            }
        return out;
    }

    // Anillo tallado, algo torcido como hecho a gubia
    inline juce::Path anillo (juce::Point<float> c, float r, float amp, juce::Random& rnd, int n = 220)
    {
        const float k1 = rnd.nextFloat() * 6.28f, k2 = rnd.nextFloat() * 6.28f, k3 = rnd.nextFloat() * 6.28f;
        juce::Path p;
        for (int i = 0; i <= n; ++i)
        {
            const float t = juce::MathConstants<float>::twoPi * (float) i / (float) n;
            const float rr = r + amp * (0.6f * std::sin (3 * t + k1) + 0.3f * std::sin (7 * t + k2) + 0.25f * std::sin (13 * t + k3));
            const juce::Point<float> q (c.x + rr * std::cos (t), c.y + rr * std::sin (t));
            if (i == 0) p.startNewSubPath (q); else p.lineTo (q);
        }
        return p;
    }
}
