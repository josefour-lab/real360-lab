#include "PluginEditor.h"
#include "Linograbado.h"
#include "BinaryData.h"

namespace
{
    constexpr int W = 440, H = 610;
    const juce::Point<float> centro { 220.0f, 262.0f };
    constexpr float R = 158.0f;                                   // radio de la luna
    const juce::Rectangle<float> zonaSelector { 16.0f, 538.0f, 230.0f, 62.0f };

    // Punto en la luna para un ángulo de pantalla (0° = derecha, como en el dibujo)
    juce::Point<float> polar (float grados, float r)
    {
        const float a = juce::degreesToRadians (grados);
        return { centro.x + r * std::cos (a), centro.y + r * std::sin (a) };
    }
}

PolarEditor::PolarEditor (PolarProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    PolarStyle::applyTheme (4);          // los colores del linograbado (Source/TemaPropio.h), para los menús
    lnf.applyThemeColours();
    setLookAndFeel (&lnf);

    logo = juce::Drawable::createFromImageData (BinaryData::real360_svg, BinaryData::real360_svgSize);
    if (logo != nullptr)
        logo->replaceColour (juce::Colour (0xfff5f8fa), juce::Colours::white);   // en la máscara: luego va en tinta azul

    setSize (W, H);
    timerCallback();
    startTimerHz (30);
}

PolarEditor::~PolarEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void PolarEditor::timerCallback()
{
    const auto s = proc.currentState();
    const int t = proc.currentTrack();
    const bool a = proc.linkAlive(), b = proc.linkBound(), q = proc.getTotalNumOutputChannels() >= 4;

    const bool cambio = s.angle != shown.angle || s.radius != shown.radius || s.spread != shown.spread
                     || s.gain != shown.gain || s.flags != shown.flags || t != pista || a != alive || b != bound || q != quad;
    shown = s; pista = t; alive = a; bound = b; quad = q;
    if (cambio)
        repaint();
}

// ------------------------------------------------------------------ imágenes entintadas

juce::Image PolarEditor::selloImagen (int numero, bool grande) const
{
    const float s = escalaHecha;
    const juce::String n (numero + 1);
    if (! grande)
    {
        // Sello pequeño del campo: cuadrado rojo lleno, marco y número en papel
        Lino::Mascara m (30, 30, s);
        m.g.addTransform (juce::AffineTransform::rotation (juce::degreesToRadians (-3.0f), 15.0f, 15.0f));
        m.g.fillRoundedRectangle (3.0f, 3.0f, 24.0f, 24.0f, 1.5f);
        m.g.setColour (juce::Colours::black);
        m.g.drawRect (juce::Rectangle<float> (5.5f, 5.5f, 19.0f, 19.0f), 1.2f);
        m.g.setFont (Lino::futura (n.length() > 1 ? 12.5f : 15.0f));
        m.g.drawText (n, juce::Rectangle<float> (3.0f, 4.0f, 24.0f, 24.0f), juce::Justification::centred, false);
        return Lino::entintar (m.img, Lino::rojo, s, 21, 0.8f);
    }

    // Sello del selector, como el monograma del grabado: marco rojo, «PISTA» y el número
    Lino::Mascara m (50, 56, s);
    m.g.drawRoundedRectangle (4.0f, 4.0f, 42.0f, 48.0f, 2.0f, 4.0f);
    m.g.setFont (Lino::futura (8.0f).withExtraKerningFactor (0.12f));
    m.g.drawSingleLineText ("PISTA", 25, 23, juce::Justification::horizontallyCentred);
    m.g.setFont (Lino::futura (20.0f));
    m.g.drawSingleLineText (n, 25, 46, juce::Justification::horizontallyCentred);
    return Lino::entintar (m.img, Lino::rojo, s, 33, 0.9f);
}

void PolarEditor::prepararImagenes (float s)
{
    const bool todo = s != escalaHecha;
    escalaHecha = s;

    if (todo)
        papel = Lino::papelImagen (W, H, s);

    if (todo || quad != quadHecho)
    {
        quadHecho = quad;
        Lino::Mascara m (W, H, s);
        auto& g = m.g;
        juce::Random rnd (7);

        // Cabecera: POLAR y el logo de REAL360, en tinta
        g.setFont (Lino::futura (25.0f).withExtraKerningFactor (0.33f));
        g.drawSingleLineText ("POLAR", 16, 44);
        if (logo != nullptr)
            logo->drawWithin (g, { 296.0f, 30.0f, 128.7f, 17.7f }, juce::RectanglePlacement::xRight | juce::RectanglePlacement::yMid, 1.0f);

        // La luna: disco de tinta con el borde algo irregular…
        juce::Path disco;
        for (int i = 0; i <= 360; ++i)
        {
            const float a = juce::degreesToRadians ((float) i);
            const float r = R + 0.7f * (Lino::ruido ((float) i / 6.0f, 0.0f, 12) - 0.5f) * 2.0f;
            const juce::Point<float> q (centro.x + r * std::cos (a), centro.y + r * std::sin (a));
            if (i == 0) disco.startNewSubPath (q); else disco.lineTo (q);
        }
        disco.closeSubPath();
        g.fillPath (disco);

        // …y tallada: anillos (las distancias) y el copo hacia los altavoces
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (disco);
            g.setColour (juce::Colours::black);
            for (float r = 7.0f; r < R - 3.0f; r += 3.4f + 1.2f * rnd.nextFloat())
            {
                const float w = (0.9f + 0.7f * rnd.nextFloat()) * (0.75f + 0.5f * r / R);
                juce::Array<float> dash;
                for (float total = 0.0f; total < juce::MathConstants<float>::twoPi * r;)
                {
                    const float on = 30.0f + 130.0f * rnd.nextFloat(), off = 1.5f + 4.5f * rnd.nextFloat();
                    dash.add (on); dash.add (off);
                    total += on + off;
                }
                juce::Path talla;
                juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)
                    .createDashedStroke (talla, Lino::anillo (centro, r, 1.2f + r * 0.018f, rnd), dash.getRawDataPointer(), dash.size());
                g.fillPath (talla);
            }

            const std::pair<float, bool> radios[] = { { -135.0f, true }, { -45.0f, true }, { 45.0f, true }, { 135.0f, true },
                                                      { -90.0f, false }, { 90.0f, false } };
            for (auto [ang, principal] : radios)
            {
                const float L = principal ? R - 10.0f : R - 40.0f;
                const int n = principal ? 5 : 3;
                for (int j = 0; j < n; ++j)
                {
                    const float off = (float) (j - n / 2) * 0.69f;   // grados
                    g.drawLine (juce::Line<float> (centro, polar (ang + off, L)), 1.4f + rnd.nextFloat());
                }
                if (principal)
                    for (float f : { 0.45f, 0.68f })
                    {
                        const auto base = polar (ang, L * f);
                        for (float lado : { -38.0f, 38.0f })
                        {
                            const float b = juce::degreesToRadians (ang + lado);
                            g.drawLine (juce::Line<float> (base, base + juce::Point<float> (std::cos (b), std::sin (b)) * 26.0f * f), 2.2f);
                        }
                    }
            }
            g.setColour (juce::Colours::white);
        }

        // Altavoces: marquitas de tinta fuera de la luna (1 del. izq., 2 del. der., 3 det. izq., 4 det. der.)
        const std::pair<int, float> altavoces[] = { { 1, -135.0f }, { 2, -45.0f }, { 3, 135.0f }, { 4, 45.0f } };
        for (auto [n, ang] : altavoces)
        {
            if (! quad && n > 2)
                continue;
            juce::Path marca;
            marca.addQuadrilateral (-6, -3, 6, -3, 5, 4, -5, 4);
            const auto p = polar (ang, R + 16.0f);
            g.fillPath (marca, juce::AffineTransform::rotation (juce::degreesToRadians (ang + 90.0f)).translated (p));
        }
        for (float ang : { -90.0f, 0.0f, 90.0f, 180.0f })
            g.fillEllipse (juce::Rectangle<float> (2.6f, 2.6f).withCentre (polar (ang, R + 9.0f)));

        tintaFija = Lino::entintar (m.img, Lino::tinta, s, 5);
    }

    if (todo || pista != pistaHecha)
    {
        pistaHecha = pista;
        selloCampo = selloImagen (pista, false);
        selloSelector = selloImagen (pista, true);
    }
}

// ------------------------------------------------------------------ dibujo

juce::Point<float> PolarEditor::posicionPista() const
{
    // POLAR: 0° delante, + hacia la derecha; radio 0 centro … 1 borde
    const float a = juce::degreesToRadians (shown.angle);
    const float r = juce::jlimit (0.0f, 1.0f, shown.radius) * (R - 6.0f);
    return { centro.x + r * std::sin (a), centro.y - r * std::cos (a) };
}

void PolarEditor::paint (juce::Graphics& g)
{
    const float s = juce::jlimit (1.0f, 4.0f, std::ceil (g.getInternalContext().getPhysicalPixelScaleFactor()));
    prepararImagenes (s);
    const auto todo = getLocalBounds().toFloat();

    g.drawImage (papel, todo);

    // Huella de la plancha
    g.setColour (Lino::plancha);
    g.drawRoundedRectangle (14.5f, 64.5f, 411.0f, 410.0f, 2.0f, 1.0f);
    g.setColour (Lino::brillo);
    g.drawRoundedRectangle (15.5f, 65.5f, 409.0f, 408.0f, 2.0f, 0.8f);

    g.drawImage (tintaFija, todo);

    // Donde va la pista, la luna se talla: hueco alrededor del sello y, con apertura, el arco entre L y R
    const auto pp = posicionPista();
    const bool apertura = ! (shown.flags & (PolarProtocol::kMono | PolarProtocol::kBalance));
    {
        juce::Path hueco;
        hueco.addEllipse (juce::Rectangle<float> (38.0f, 38.0f).withCentre (pp));
        const float sd = std::abs (shown.spread);
        if (apertura && sd >= 3.0f && shown.radius >= 0.03f)
        {
            const float rr = pp.getDistanceFrom (centro);
            const float a = std::atan2 (pp.x - centro.x, centro.y - pp.y), half = juce::degreesToRadians (sd * 0.5f);
            juce::Path arco;
            arco.addCentredArc (centro.x, centro.y, rr, rr, 0.0f, a - half, a + half, true);
            juce::Path grueso;
            juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded).createStrokedPath (grueso, arco);
            hueco.addPath (grueso);
        }
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path enLuna;
        enLuna.addEllipse (juce::Rectangle<float> (2 * R, 2 * R).withCentre (centro));
        g.reduceClipRegion (enLuna);
        g.reduceClipRegion (hueco);
        g.drawImage (papel, todo);
    }

    // El sello rojo de la pista (más tenue si está en silencio)
    const bool silencio = shown.gain <= 0.0001f || (shown.flags & PolarProtocol::kSilent);
    g.setOpacity (silencio ? 0.4f : 1.0f);
    g.drawImage (selloCampo, juce::Rectangle<float> (30.0f, 30.0f).withCentre (pp));
    g.setOpacity (1.0f);

    // A lápiz: números de los altavoces, delante / detrás
    g.setColour (Lino::lapiz);
    g.setFont (Lino::aLapiz (15.0f));
    const std::pair<int, float> altavoces[] = { { 1, -135.0f }, { 2, -45.0f }, { 3, 135.0f }, { 4, 45.0f } };
    for (auto [n, ang] : altavoces)
        if (quad || n <= 2)
        {
            const auto p = polar (ang, R + 34.0f);
            g.drawSingleLineText (juce::String (n), juce::roundToInt (p.x), juce::roundToInt (p.y + 5.0f), juce::Justification::horizontallyCentred);
        }
    g.setFont (Lino::aLapiz (12.0f));
    g.drawSingleLineText ("delante", 220, juce::roundToInt (centro.y - R - 26.0f), juce::Justification::horizontallyCentred);
    g.drawSingleLineText (juce::String::fromUTF8 ("detrás"), 220, juce::roundToInt (centro.y + R + 36.0f), juce::Justification::horizontallyCentred);

    // Pie a lápiz, como el del grabado: pista · modo · volumen
    juce::String modo = (shown.flags & PolarProtocol::kMono) ? "Mono" : ((shown.flags & PolarProtocol::kBalance) ? "Balance" : "Apertura");
    if (! quad)
        modo << juce::String::fromUTF8 (" (estéreo)");
    const juce::String vol = shown.gain > 0.0001f
        ? juce::String (20.0f * std::log10 (shown.gain), 1).replaceCharacter ('.', ',').replace ("-", juce::String::fromUTF8 ("−")) + " dB"
        : juce::String ("silencio");
    g.setFont (Lino::aLapiz (15.0f));
    g.drawSingleLineText ("- Pista " + juce::String (pista + 1) + " -", 40, 508);
    g.drawSingleLineText (modo, 220, 508, juce::Justification::horizontallyCentred);
    g.drawSingleLineText (vol, 400, 508, juce::Justification::right);
    g.setColour (Lino::plancha);
    g.drawLine (24.0f, 526.0f, 416.0f, 526.0f, 0.8f);

    // Selector de pista: el sello rojo
    g.drawImage (selloSelector, juce::Rectangle<float> (16.0f, 538.0f, 50.0f, 56.0f));
    g.setColour (Lino::lapiz);
    g.setFont (Lino::aLapiz (12.0f));
    g.drawSingleLineText (juce::String::fromUTF8 ("elige pista ▾"), 76, 562);
    g.setColour (Lino::lapiz.withAlpha (0.75f));
    g.setFont (Lino::aLapiz (11.0f));
    g.drawSingleLineText (juce::String::fromUTF8 ("1 … 16, la de la ficha de POLAR"), 76, 580);

    // Conexión con la app: punto de tinta (conectado), hueco (sin señal) o rojo (puerto ocupado)
    const auto punto = juce::Rectangle<float> (10.0f, 10.0f).withCentre ({ 262.0f, 568.0f });
    if (alive)
    {
        g.setColour (Lino::tinta);
        g.fillEllipse (punto);
    }
    else
    {
        g.setColour (bound ? Lino::lapiz : Lino::rojo);
        if (bound) g.drawEllipse (punto.reduced (0.6f), 1.2f); else g.fillEllipse (punto);
    }
    g.setColour (Lino::lapiz);
    g.setFont (Lino::aLapiz (13.0f));
    if (alive)
        g.drawSingleLineText ("Conectado a POLAR", 274, 573);
    else
    {
        g.drawSingleLineText (bound ? juce::String::fromUTF8 ("Sin señal de POLAR") : juce::String ("Puerto 9360 ocupado"), 274, 566);
        g.setFont (Lino::aLapiz (11.0f));
        g.drawSingleLineText (bound ? juce::String::fromUTF8 ("se queda en la última posición") : juce::String ("por otro programa"), 274, 582);
    }
}

// ------------------------------------------------------------------ elegir pista

void PolarEditor::mouseMove (const juce::MouseEvent& e)
{
    setMouseCursor (zonaSelector.contains (e.position) ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void PolarEditor::mouseDown (const juce::MouseEvent& e)
{
    if (zonaSelector.contains (e.position))
        elegirPista();
}

void PolarEditor::elegirPista()
{
    juce::PopupMenu menu;
    for (int i = 1; i <= PolarProtocol::kTracks; ++i)
        menu.addItem (i, "Pista " + juce::String (i), true, i == pista + 1);

    juce::Component::SafePointer<PolarEditor> self (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this)
                            .withTargetScreenArea (localAreaToGlobal (zonaSelector.toNearestInt())),
                        [self] (int id)
                        {
                            if (self == nullptr || id <= 0)
                                return;
                            if (auto* prm = self->proc.apvts.getParameter ("track"))
                            {
                                prm->beginChangeGesture();
                                prm->setValueNotifyingHost (prm->convertTo0to1 ((float) (id - 1)));
                                prm->endChangeGesture();
                            }
                        });
}
