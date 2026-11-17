#pragma once
#include <juce_core/juce_core.h>

// =====================================================================
//  Enlace de sincronía entre plugins de REAL360 (15 Step → Looper Búho…)
//  Cada plugin es un programa aparte dentro de Logic y no comparten
//  memoria: el que marca el ritmo escribe en un archivo pequeño dónde
//  está su «1» y cuánto dura su ciclo (en negras de Logic, PPQ), y los
//  demás lo leen en cada bloque. Archivo:
//  ~/Library/Application Support/REAL360/sync/<nombre>.sync
// =====================================================================
struct Real360SyncBlock
{
    char magic[8];             // "R360SYN"
    juce::uint32 version;      // 1
    juce::uint32 reserved;
    double cycleStartPpq;      // posición (en negras) de un «1» del ciclo
    double cycleLengthPpq;     // duración del ciclo en negras (15 semicorcheas = 3,75)
    double bpm;
    double updatedMs;          // hora de la última escritura (ms desde 1970)
};

class Real360Sync
{
public:
    static juce::File fileFor (const juce::String& name)
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Application Support").getChildFile ("REAL360")
                   .getChildFile ("sync").getChildFile (name + ".sync");
    }

    // Quien marca el ritmo (15 Step): llamar en prepareToPlay, fuera del audio
    bool openForWrite (const juce::String& name)
    {
        auto f = fileFor (name);
        f.getParentDirectory().createDirectory();
        if (f.getSize() != (juce::int64) sizeof (Real360SyncBlock))
        {
            juce::MemoryBlock zeros (sizeof (Real360SyncBlock), true);
            f.replaceWithData (zeros.getData(), zeros.getSize());
        }
        return publish (std::make_unique<juce::MemoryMappedFile> (f, juce::MemoryMappedFile::readWrite));
    }

    // Quien escucha (Looper Búho): llamar fuera del audio; si no existe aún, volver a probar más tarde
    bool openForRead (const juce::String& name)
    {
        if (isOpen())
            return true;
        auto f = fileFor (name);
        if (! f.existsAsFile() || f.getSize() < (juce::int64) sizeof (Real360SyncBlock))
            return false;
        return publish (std::make_unique<juce::MemoryMappedFile> (f, juce::MemoryMappedFile::readOnly));
    }

    bool isOpen() const { return live.load() != nullptr; }

    // En cada bloque de audio, desde quien marca el ritmo
    void write (double cycleStartPpq, double cycleLengthPpq, double bpm)
    {
        auto* m = live.load();
        if (m == nullptr)
            return;
        auto* b = static_cast<Real360SyncBlock*> (m->getData());
        std::memcpy (b->magic, "R360SYN", 8);
        b->version = 1;
        b->cycleStartPpq = cycleStartPpq;
        b->cycleLengthPpq = cycleLengthPpq;
        b->bpm = bpm;
        b->updatedMs = (double) juce::Time::currentTimeMillis();
    }

    // En cada bloque, desde quien escucha: true si hay datos recientes (menos de medio segundo)
    bool read (double& cycleStartPpq, double& cycleLengthPpq, double& bpm) const
    {
        const auto* m = live.load();
        if (m == nullptr)
            return false;
        const auto* b = static_cast<const Real360SyncBlock*> (m->getData());
        if (std::memcmp (b->magic, "R360SYN", 7) != 0)
            return false;
        if ((double) juce::Time::currentTimeMillis() - b->updatedMs > 500.0)
            return false;
        cycleStartPpq = b->cycleStartPpq;
        cycleLengthPpq = b->cycleLengthPpq;
        bpm = b->bpm;
        return true;
    }

private:
    // Se abre una sola vez y se publica de golpe: el audio nunca ve un estado a medias
    bool publish (std::unique_ptr<juce::MemoryMappedFile> m)
    {
        if (m == nullptr || m->getData() == nullptr || m->getSize() < sizeof (Real360SyncBlock))
            return false;
        owned = std::move (m);
        live.store (owned.get());
        return true;
    }

    std::unique_ptr<juce::MemoryMappedFile> owned;
    std::atomic<juce::MemoryMappedFile*> live { nullptr };
};
