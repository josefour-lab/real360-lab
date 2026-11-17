#include "PolarPad.h"
#include <array>
#include <cmath>

// ====================================================================== PolarPad

namespace
{
    // Geometría de POLAR: punto del campo (0..1) para un ángulo y una distancia de tipo cuadrado
    juce::Point<float> pointAt (float angleDeg, float r)
    {
        const float t = juce::degreesToRadians (angleDeg);
        const float s = std::sin (t), c = std::cos (t);
        const float m = juce::jmax (std::abs (s), std::abs (c));
        return { 0.5f + 0.5f * r * s / m, 0.5f - 0.5f * r * c / m };
    }
}

juce::Rectangle<float> PolarPad::padRect() const
{
    auto b = getLocalBounds().toFloat();
    const float size = juce::jmin (b.getWidth(), b.getHeight() - 50.0f);
    return { b.getX(), b.getY(), size, size };
}

juce::Rectangle<float> PolarPad::fieldRect() const
{
    auto p = padRect();
    return p.reduced (juce::jmax (40.0f, p.getWidth() * 0.065f));
}

float PolarPad::shownRadius (float r) const
{
    if (! host.isProtected())
        return r;
    const float k = QuadPanner::kProtectRadius;
    return k + (1.0f - k) * r;
}

juce::Point<float> PolarPad::toScreen (float angleDeg, float radius) const
{
    const auto f = fieldRect();
    const auto p = pointAt (angleDeg, juce::jlimit (0.0f, 1.0f, radius));
    return { f.getX() + p.x * f.getWidth(), f.getY() + p.y * f.getHeight() };
}

SpatialPoint* PolarPad::pointFor (int src)
{
    return src >= 0 && src < host.numSources() ? &host.point (src) : nullptr;
}

bool PolarPad::isActive (int src)
{
    return src >= 0 && src < host.numSources() && host.isSourceActive (src);
}

juce::Colour PolarPad::colourFor (int src)
{
    return host.sourceColour (src);
}

juce::String PolarPad::labelFor (int src)
{
    return host.sourceLabel (src);
}

void PolarPad::paintGrid (juce::Graphics& g, juce::Rectangle<float> f)
{
    auto P = [&f] (float x, float y) { return juce::Point<float> (f.getX() + x / 100.0f * f.getWidth(), f.getY() + y / 100.0f * f.getHeight()); };

    // Marcos cuadrados al 25, 50 y 75 %
    g.setColour (PolarStyle::T (juce::Colour (0xff33251c)));
    for (float k : { 0.25f, 0.5f, 0.75f })
    {
        auto r = juce::Rectangle<float> (P (50 - 50 * k, 50 - 50 * k), P (50 + 50 * k, 50 + 50 * k));
        g.drawRoundedRectangle (r, (5.0f * k + 1.0f) / 100.0f * f.getWidth(), 1.0f);
    }

    // Diagonales hacia los altavoces
    g.setColour (PolarStyle::T (juce::Colour (0xff2a1e17)));
    const float dash[] = { 3.0f, 5.0f };
    for (auto corner : { P (10, 10), P (90, 10), P (10, 90), P (90, 90) })
        g.drawDashedLine (juce::Line<float> (P (50, 50), corner), dash, 2, 1.0f);

    // Ejes
    g.setColour (PolarStyle::T (juce::Colour (0xff2e2119)));
    g.drawLine (juce::Line<float> (P (50, 3), P (50, 97)), 1.0f);
    g.drawLine (juce::Line<float> (P (3, 50), P (97, 50)), 1.0f);

    // Marcas cada 15° (sin marca en las esquinas: ahí están los altavoces)
    for (int a = 0; a < 360; a += 15)
    {
        if (a % 90 != 0 && a % 45 == 0)
            continue;
        const float t = juce::degreesToRadians ((float) a);
        const float s = std::sin (t), c = std::cos (t), m = juce::jmax (std::abs (s), std::abs (c));
        const float bx = 50 + 50 * s / m, by = 50 - 50 * c / m, L = a % 90 == 0 ? 3.2f : 1.8f;
        {
            const juce::Colour col (a % 90 == 0 ? 0xff5a4434 : 0xff3a2c22);
            g.setColour (PolarStyle::Propio::rejillaConTema ? PolarStyle::T (col) : col);
        }
        g.drawLine (juce::Line<float> (P (bx, by), P (bx - s / m * L, by + c / m * L)), 1.0f);
    }

    // Grados
    g.setColour (PolarStyle::T (juce::Colour (0xff7a6655)));
    g.setFont (PolarStyle::mono (10.0f));
    const std::pair<juce::Point<float>, const char*> labels[] = {
        { P (50, 7.2f), "0°" }, { P (94, 51), "90°" }, { P (50, 95), "180°" }, { P (6, 51), "−90°" } };
    for (auto& l : labels)
        g.drawText (juce::String::fromUTF8 (l.second), juce::Rectangle<float> (40.0f, 14.0f).withCentre (l.first),
                    juce::Justification::centred);
}

void PolarPad::paintSpeakers (juce::Graphics& g, juce::Rectangle<float> f)
{
    // Porcentaje estimado por altavoz para la fuente seleccionada (potencia de L y R promediada)
    const bool stereo = ! host.isQuadOutput();
    std::array<float, 4> pct { 25.0f, 25.0f, 25.0f, 25.0f };
    if (stereo)
        pct = { 50.0f, 50.0f, 0.0f, 0.0f };
    if (isActive (selected))
    {
        auto* p = pointFor (selected);
        const float r = shownRadius (p->radius.load()), a = p->angle.load(), w = p->width.load();
        QuadPanner::Gains gl {}, gr {};
        const int mode = host.panMode();
        if (mode == QuadPanner::PanBalance)
        {
            QuadPanner::balanceGains (a, r, stereo, gl, gr);
        }
        else if (mode == QuadPanner::PanMono)
        {
            gl = stereo ? QuadPanner::stereoGains (a, r) : QuadPanner::pointGains (a, r);
            for (auto& g : gl) g *= 0.70710678f;
            gr = gl;
        }
        else
        {
            gl = stereo ? QuadPanner::stereoGains (a - 0.5f * w, r) : QuadPanner::pointGains (a - 0.5f * w, r);
            gr = stereo ? QuadPanner::stereoGains (a + 0.5f * w, r) : QuadPanner::pointGains (a + 0.5f * w, r);
        }
        for (size_t i = 0; i < 4; ++i)
            pct[i] = 50.0f * (gl[i] * gl[i] + gr[i] * gr[i]);
    }

    // 1 = sup. izq. (L), 2 = sup. der. (R), 3 = inf. izq. (Ls), 4 = inf. der. (Rs)
    const juce::Rectangle<float> boxes[4] = {
        { f.getX(), f.getY(), 78.0f, 46.0f },
        { f.getRight() - 78.0f, f.getY(), 78.0f, 46.0f },
        { f.getX(), f.getBottom() - 46.0f, 78.0f, 46.0f },
        { f.getRight() - 78.0f, f.getBottom() - 46.0f, 78.0f, 46.0f } };

    for (int i = 0; i < 4; ++i)
    {
        if (stereo && i >= 2)
            continue; // en estéreo solo hay izquierda y derecha
        const auto b = boxes[i];
        g.setColour (PolarStyle::T (juce::Colour (0xff160f0b)));
        g.fillRoundedRectangle (b, 9.0f);
        g.setColour (PolarStyle::T (juce::Colour (0xff3e2d22)));
        g.drawRoundedRectangle (b.reduced (0.5f), 9.0f, 1.0f);

        // el texto queda hacia dentro del campo
        auto inner = (i % 2 == 0) ? b.withTrimmedLeft (18.0f) : b.withTrimmedRight (18.0f);
        inner = inner.withTrimmedTop (6.0f);
        g.setColour (PolarStyle::T (juce::Colour (0xff9a8571)));
        const juce::String name = stereo ? juce::String (i == 0 ? "IZQUIERDA" : "DERECHA") : "ALTAVOZ " + juce::String (i + 1);
        PolarStyle::drawSpaced (g, name, inner.removeFromTop (14.0f),
                                PolarStyle::ui (8.5f), 0.7f, juce::Justification::horizontallyCentred);
        g.setColour (PolarStyle::accent);
        g.setFont (PolarStyle::mono (16.0f, true));
        g.drawText (juce::String (juce::roundToInt (pct[(size_t) i])) + "%", inner, juce::Justification::centredTop);
    }
}

void PolarPad::paint (juce::Graphics& g)
{
    using namespace PolarStyle;
    if (! bg.isTransparent())
        g.fillAll (bg);

    const auto pad = padRect();
    const auto f = fieldRect();
    const bool protect = host.isProtected();

    // Marco del pad
    {
        juce::ColourGradient grad (PolarStyle::T (juce::Colour (0xff1d140e)), pad.getCentreX(), pad.getCentreY(),
                                   PolarStyle::T (juce::Colour (0xff0c0806)), pad.getRight(), pad.getBottom(), true);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (pad, 18.0f);
        g.setColour (PolarStyle::T (juce::Colour (0xff33251c)));
        g.drawRoundedRectangle (pad.reduced (0.5f), 18.0f, 1.0f);
    }

    paintGrid (g, f);

    // Zona «sin estéreo»: con la protección activada, ninguna fuente entra aquí
    if (protect)
    {
        const float k = QuadPanner::kProtectRadius;
        auto z = juce::Rectangle<float> (f.getWidth() * k, f.getHeight() * k).withCentre (f.getCentre());
        {
            juce::Graphics::ScopedSaveState ss (g);
            juce::Path clip;
            clip.addRoundedRectangle (z, 10.0f);
            g.reduceClipRegion (clip);
            g.setColour (danger.withAlpha (0.07f));
            for (float d = -z.getHeight(); d < z.getWidth(); d += 14.0f)
                g.drawLine (z.getX() + d, z.getBottom(), z.getX() + d + z.getHeight(), z.getY(), 7.0f);
        }
        g.setColour (danger.withAlpha (0.6f));
        juce::Path outline;
        outline.addRoundedRectangle (z, 10.0f);
        juce::Path dashed;
        const float dash[] = { 4.0f, 3.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (dashed, outline, dash, 2);
        g.fillPath (dashed);
        g.setColour (danger.withAlpha (0.85f));
        g.setFont (ui (10.0f));
        g.drawText (juce::String::fromUTF8 ("sin estéreo"), z.withTrimmedTop (z.getHeight() - 18.0f), juce::Justification::centred);
    }

    paintSpeakers (g, f);

    // Abanico del Spread y marcas L / R de la fuente seleccionada
    juce::Point<float> selPos;
    if (isActive (selected))
    {
        auto* p = pointFor (selected);
        const float ang = p->angle.load(), r = shownRadius (p->radius.load()), sd = p->width.load();
        selPos = toScreen (ang, r);

        if (host.panMode() == QuadPanner::PanApertura && sd >= 3.0f && r >= 0.03f)
        {
            auto m = [] (float a) { const float t = juce::degreesToRadians (a); return juce::jmax (std::abs (std::sin (t)), std::abs (std::cos (t))); };
            const float Ro = ((r / m (ang - sd / 2) + r / m (ang + sd / 2)) / 2.0f) * 0.5f * 1.12f;
            const int n = juce::jmax (3, (int) std::ceil (sd / 8.0f));
            juce::Path fan;
            fan.startNewSubPath (f.getCentre());
            for (int i = 0; i <= n; ++i)
            {
                const float a = juce::degreesToRadians (ang - sd / 2 + sd * (float) i / (float) n);
                fan.lineTo (f.getCentreX() + Ro * std::sin (a) * f.getWidth(), f.getCentreY() - Ro * std::cos (a) * f.getHeight());
            }
            fan.closeSubPath();
            g.setColour (PolarStyle::hi().withAlpha (0.05f));
            g.fillPath (fan);

            auto marker = [&] (float a, const char* t, juce::Colour c)
            {
                auto box = juce::Rectangle<float> (22.0f, 22.0f).withCentre (toScreen (a, r));
                g.setColour (c.withAlpha (0.18f));
                g.fillEllipse (box);
                g.setColour (c);
                g.drawEllipse (box.reduced (0.5f), 1.0f);
                g.setFont (ui (11.0f, true));
                g.drawText (t, box, juce::Justification::centred);
            };
            marker (ang - sd / 2, "L", PolarStyle::T (juce::Colour (0xfff1e9df)));
            marker (ang + sd / 2, "R", PolarStyle::Propio::marcaRConAcento ? PolarStyle::accent : PolarStyle::T (juce::Colour (0xffd63a2c)));
        }
    }

    // Objetos: primero los demás, el seleccionado encima
    auto drawObject = [&] (int src)
    {
        auto* p = pointFor (src);
        const auto pc = toScreen (p->angle.load(), shownRadius (p->radius.load()));
        const bool sel = src == selected;
        const bool other = host.isSourceDimmed (src);
        const auto c = colourFor (src).withMultipliedAlpha (other && ! sel ? 0.5f : 1.0f);
        const float d = sel ? 26.0f : 22.0f;
        auto box = juce::Rectangle<float> (d, d).withCentre (pc);

        if (sel)
        {
            for (int i = 3; i >= 1; --i) // brillo
            {
                g.setColour (c.withAlpha (0.10f));
                g.fillEllipse (box.expanded (2.0f + 3.0f * (float) i));
            }
            g.setColour (PolarStyle::hi());
            g.fillEllipse (box.expanded (2.0f));
            g.setColour (c);
            g.fillEllipse (box.reduced (1.5f));
        }
        else
        {
            g.setColour (c.withAlpha (0.25f * c.getFloatAlpha()));
            g.fillEllipse (box);
            g.setColour (c);
            g.drawEllipse (box.reduced (1.5f), 3.0f);
        }

        if (host.isSourceRecording (src))
        {
            g.setColour (danger);
            g.drawEllipse (box.expanded (6.0f), 2.0f);
        }

        g.setColour (sel ? PolarStyle::T (juce::Colour (0xff14100a)) : c);
        g.setFont (mono (sel ? 11.0f : 10.0f, true));
        g.drawText (labelFor (src), box, juce::Justification::centred);
    };

    for (int src = 0; src < host.numSources(); ++src)
        if (src != selected && isActive (src))
            drawObject (src);
    if (isActive (selected))
        drawObject (selected);

    // Etiqueta del seleccionado (se esconde en las esquinas, donde se lee el % del altavoz)
    if (isActive (selected))
    {
        const float x = (selPos.x - f.getX()) / f.getWidth(), y = (selPos.y - f.getY()) / f.getHeight();
        if (! (std::abs (x - 0.5f) > 0.36f && std::abs (y - 0.5f) > 0.36f))
        {
            auto* p = pointFor (selected);
            const auto names = host.movementNames();
            const int mode = p->mode.load();
            juce::String text = host.sourceName (selected);
            if (mode > 0 && mode < names.size())
                text << juce::String::fromUTF8 (" · ") << names[mode];

            const auto font = ui (11.5f);
            const float w = PolarStyle::textWidth (font, text) + 22.0f;
            auto chip = juce::Rectangle<float> (w, 22.0f);
            chip.setPosition (x > 0.7f ? selPos.x - w - 24.0f : selPos.x + 24.0f,
                              y < 0.14f ? selPos.y + 22.0f - 11.0f : selPos.y - 46.0f + 11.0f);
            g.setColour (PolarStyle::T (juce::Colour (0xff1c130e)));
            g.fillRoundedRectangle (chip, 11.0f);
            g.setColour (PolarStyle::T (juce::Colour (0xff4a3628)));
            g.drawRoundedRectangle (chip.reduced (0.5f), 11.0f, 1.0f);
            g.setColour (fg);
            g.setFont (font);
            g.drawText (text, chip, juce::Justification::centred);
        }
    }

    // Debajo del pad: progreso de la secuencia activa, estado y aviso de salida estéreo
    auto below = getLocalBounds().toFloat().withTop (pad.getBottom() + 8.0f);
    {
        auto bar = below.removeFromTop (6.0f).reduced (2.0f, 0.0f);
        g.setColour (track);
        g.fillRoundedRectangle (bar, 3.0f);
        const float prog = host.progress();
        if (prog >= 0.0f)
        {
            g.setColour (host.progressColour().withAlpha (0.8f));
            g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, prog)), 3.0f);
        }
    }
    below.removeFromTop (6.0f);
    g.setColour (fg.withAlpha (0.8f));
    g.setFont (mono (12.0f));
    auto statusRow = below.removeFromTop (18.0f);
    if (! host.isQuadOutput())
    {
        // Aviso de salida estéreo, discreto, a la derecha de la línea de estado
        g.setColour (muted);
        g.drawText (juce::String::fromUTF8 ("Salida estéreo"), statusRow.removeFromRight (120.0f), juce::Justification::centredRight);
        g.setColour (fg.withAlpha (0.8f));
    }
    g.drawText (host.statusText(), statusRow, juce::Justification::centredLeft);
}

void PolarPad::mouseDown (const juce::MouseEvent& e)
{
    if (onInteract)
        onInteract();

    int best = -1;
    float bestDist = 18.0f;

    for (int src = 0; src < host.numSources(); ++src)
    {
        if (! isActive (src))
            continue;
        auto* p = pointFor (src);
        const float d = toScreen (p->angle.load(), shownRadius (p->radius.load())).getDistanceFrom (e.position);
        if (d < bestDist || (src == selected && d < 18.0f))
        {
            bestDist = d;
            best = src;
        }
    }

    if (best < 0)
        return;

    selected = best;
    dragging = pointFor (best);
    dragging->dragging = true;
    if (onSelect)
        onSelect (best);
    repaint();
}

void PolarPad::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging == nullptr)
        return;

    // Inversa de la geometría de POLAR: ángulo y distancia de tipo cuadrado
    const auto f = fieldRect();
    const float x = juce::jlimit (0.0f, 1.0f, (e.position.x - f.getX()) / f.getWidth());
    const float y = juce::jlimit (0.0f, 1.0f, (e.position.y - f.getY()) / f.getHeight());
    const float u = (x - 0.5f) * 2.0f, w = (0.5f - y) * 2.0f;
    float rho = juce::jmin (1.0f, juce::jmax (std::abs (u), std::abs (w)));

    if (host.isProtected())
    {
        const float k = QuadPanner::kProtectRadius;
        rho = juce::jlimit (0.0f, 1.0f, (rho - k) / (1.0f - k));
    }

    if (std::abs (u) > 1.0e-4f || std::abs (w) > 1.0e-4f)
        dragging->angle = juce::radiansToDegrees (std::atan2 (u, w));
    dragging->radius = rho;
}

void PolarPad::mouseUp (const juce::MouseEvent&)
{
    if (dragging != nullptr)
        dragging->dragging = false;
    dragging = nullptr;
}

// ====================================================================== Mezclador
