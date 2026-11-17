#include "Movimientos.h"
#include "BinaryData.h"
#include <cmath>

namespace
{
    juce::String u (const char* s) { return juce::String::fromUTF8 (s); }
    float num (const juce::var& v) { return (float) (double) v; }
}

Movimientos::Movimientos()
    : juce::Thread ("Movimientos"), scriptFile (defaultScriptFile())
{
    sharedModeNames.add ("Fijo");
    threadModeNames.add ("Fijo");
    status = u ("Cargando movimientos…");
}

Movimientos::~Movimientos()
{
    stopThread (2000);
}

juce::File Movimientos::defaultScriptFile()
{
    // En el Mac: ~/Library/Application Support/REAL360/LooperPolar/movimientos.js
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Application Support")
               .getChildFile ("REAL360")
               .getChildFile ("Looper Polar")
               .getChildFile ("movimientos.js");
}

void Movimientos::setSources (std::vector<SpatialPoint*> points, std::function<bool (int)> isActive)
{
    sources = std::move (points);
    activeFn = std::move (isActive);
}

juce::StringArray Movimientos::getModeNames() const
{
    const juce::ScopedLock sl (lock);
    return sharedModeNames;
}

juce::String Movimientos::getStatus() const
{
    const juce::ScopedLock sl (lock);
    return status;
}

void Movimientos::setStatus (const juce::String& s)
{
    const juce::ScopedLock sl (lock);
    status = s;
}

void Movimientos::run()
{
    const double start = juce::Time::getMillisecondCounterHiRes();
    double last = start;
    int counter = 30;

    while (! threadShouldExit())
    {
        if (++counter >= 30) // cada ~0,5 s mira si el archivo ha cambiado
        {
            counter = 0;
            reloadIfChanged();
        }

        const double now = juce::Time::getMillisecondCounterHiRes();
        const double dt = juce::jlimit (0.0, 0.1, (now - last) * 0.001);
        last = now;

        tick (dt, (now - start) * 0.001);
        wait (16);
    }

    engine.reset();
}

void Movimientos::reloadIfChanged()
{
    if (! scriptFile.existsAsFile())
    {
        // Primera vez: copia el movimientos.js que viene dentro del plugin
        scriptFile.getParentDirectory().createDirectory();
        scriptFile.replaceWithData (BinaryData::movimientos_js, (size_t) BinaryData::movimientos_jsSize);
    }
    else if (engine == nullptr)
    {
        upgradeUntouchedScript();
    }

    const auto modified = scriptFile.getLastModificationTime();
    if (engine != nullptr && modified == lastModified)
        return;
    lastModified = modified;

    auto fresh = std::make_unique<juce::JavascriptEngine>();
    const auto result = fresh->execute (scriptFile.loadFileAsString());

    if (result.failed())
    {
        setStatus (u ("Error en movimientos.js: ") + result.getErrorMessage()
                   + (engine != nullptr ? u (" (sigue el anterior)") : juce::String()));
        return;
    }

    juce::Result r = juce::Result::ok();
    const auto names = fresh->callFunction ("nombresDeModos", juce::var::NativeFunctionArgs (juce::var(), nullptr, 0), &r);

    juce::StringArray list;
    list.add ("Fijo");
    if (auto* arr = names.getArray())
        for (auto& v : *arr)
            list.add (v.toString());

    engine = std::move (fresh);
    threadModeNames = list;

    {
        const juce::ScopedLock sl (lock);
        sharedModeNames = list;
        status = u ("Movimientos cargados: ") + juce::String (list.size() - 1) + u (" modos · ")
                 + juce::Time::getCurrentTime().toString (false, true, true, true);
        if (! scriptFile.loadFileAsString().contains ("moverEnEstereo"))
            status << u (" · tu movimientos.js es antiguo: en estéreo los movimientos pueden ir hacia atrás");
    }
    ++version;
}

// El movimientos.js es compartido con Looper Búho y Stutter Quad. Si el instalado es
// exactamente la versión anterior de serie (nadie lo ha tocado), se sustituye por la
// nueva, que sabe moverse por 2 altavoces en estéreo. La nueva es compatible: si un
// plugin no pasa p.altavoces, se comporta como antes (4 altavoces).
// Si el archivo está personalizado, no se toca nunca.
void Movimientos::upgradeUntouchedScript()
{
    juce::MemoryBlock installed;
    if (! scriptFile.loadFileAsData (installed))
        return;

    // v1: la del kit original (4 altavoces siempre). v2: «Paseo por altavoces» en estéreo.
    // v3: todo en estéreo por delante (moverEnEstereo). La de serie ahora añade «Ping-pong».
    const juce::MemoryBlock previous[] = {
        { BinaryData::movimientos_v1_js, (size_t) BinaryData::movimientos_v1_jsSize },
        { BinaryData::movimientos_v2_js, (size_t) BinaryData::movimientos_v2_jsSize },
        { BinaryData::movimientos_v3_js, (size_t) BinaryData::movimientos_v3_jsSize } };
    for (auto& old : previous)
        if (installed == old)
        {
            scriptFile.replaceWithData (BinaryData::movimientos_js, (size_t) BinaryData::movimientos_jsSize);
            return;
        }
}

void Movimientos::tick (double dt, double t)
{
    if (engine == nullptr)
        return;

    for (size_t i = 0; i < sources.size(); ++i)
    {
        if (activeFn && ! activeFn ((int) i))
            continue;

        auto* p = sources[i];
        if (p->dragging.load())
            continue;

        const int m = p->mode.load();
        if (m <= 0 || m >= threadModeNames.size())
            continue;

        const float a = p->angle.load();
        const float r = p->radius.load();
        const float rad = juce::degreesToRadians (a);

        auto* o = new juce::DynamicObject();
        o->setProperty ("id", (int) i);
        o->setProperty ("angulo", a);
        o->setProperty ("radio", r);
        o->setProperty ("x", r * std::sin (rad));
        o->setProperty ("y", r * std::cos (rad));
        o->setProperty ("apertura", p->width.load());
        o->setProperty ("velocidad", p->speed.load());
        o->setProperty ("altavoces", numSpeakers.load (std::memory_order_relaxed));

        juce::var args[] = { juce::var (threadModeNames[m]), juce::var (o), juce::var (dt), juce::var (t) };
        juce::Result res = juce::Result::ok();
        const auto out = engine->callFunction ("mover", juce::var::NativeFunctionArgs (juce::var(), args, 4), &res);

        if (res.failed())
        {
            setStatus (u ("Error en el modo «") + threadModeNames[m] + u ("»: ") + res.getErrorMessage());
            continue;
        }

        auto* obj = out.getDynamicObject();
        if (obj == nullptr)
            continue;

        if (obj->hasProperty ("x") && obj->hasProperty ("y"))
        {
            const float x = num (obj->getProperty ("x"));
            const float y = num (obj->getProperty ("y"));
            p->angle = SpatialPoint::wrap (juce::radiansToDegrees (std::atan2 (x, y)));
            p->radius = juce::jlimit (0.0f, 1.0f, std::sqrt (x * x + y * y));
        }
        else
        {
            if (obj->hasProperty ("angulo"))
                p->angle = SpatialPoint::wrap (num (obj->getProperty ("angulo")));
            if (obj->hasProperty ("radio"))
                p->radius = juce::jlimit (0.0f, 1.0f, num (obj->getProperty ("radio")));
        }

        if (obj->hasProperty ("apertura"))
            p->width = juce::jlimit (0.0f, 180.0f, num (obj->getProperty ("apertura")));
    }
}
