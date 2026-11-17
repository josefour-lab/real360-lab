#pragma once
#include <juce_core/juce_core.h>
#include <juce_javascript/juce_javascript.h>
#include <atomic>
#include <functional>
#include <vector>
#include "SpatialPoint.h"

// Movimientos: mueve las fuentes según los modos escritos en movimientos.js.
// Corre en su propio hilo a ~60 Hz (nunca en el hilo de audio) y vigila el archivo:
// al guardarlo, se recarga solo, sin reinstalar ni reiniciar Logic.
// Si el script nuevo tiene un error, sigue funcionando con el anterior y muestra el error.
class Movimientos : private juce::Thread
{
public:
    Movimientos();
    ~Movimientos() override;

    void setSources (std::vector<SpatialPoint*> points, std::function<bool (int)> isActive);
    void startMovimientos() { startThread(); }

    // Altavoces de la pista: 4 (cuadrafónica) o 2 (estéreo). Se pasa al script como
    // p.altavoces para que los movimientos por altavoces usen solo los que existen.
    void setNumSpeakers (int n) { numSpeakers.store (n >= 4 ? 4 : 2, std::memory_order_relaxed); }

    juce::StringArray getModeNames() const;   // índice 0 = "Fijo"
    juce::String getStatus() const;
    int getVersion() const { return version.load(); }
    juce::File getScriptFile() const { return scriptFile; }

    static juce::File defaultScriptFile();

private:
    void run() override;
    void reloadIfChanged();
    void upgradeUntouchedScript();
    void tick (double dt, double t);
    void setStatus (const juce::String&);

    std::vector<SpatialPoint*> sources;
    std::function<bool (int)> activeFn;

    // Solo se usan dentro del hilo del Movimientos
    std::unique_ptr<juce::JavascriptEngine> engine;
    juce::StringArray threadModeNames;
    juce::Time lastModified;

    const juce::File scriptFile;

    mutable juce::CriticalSection lock;
    juce::StringArray sharedModeNames;
    juce::String status;
    std::atomic<int> version { 0 };
    std::atomic<int> numSpeakers { 4 };
};
