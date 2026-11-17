#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include <memory>
#include "PolarProtocol.h"

// =====================================================================
//  Receptor de la app POLAR: escucha UDP en 127.0.0.1:9360 y guarda el estado de las 16 pistas.
//  Es UNO solo por proceso: todas las instancias del plugin en Logic comparten el mismo
//  (el primero que se crea abre el puerto; el último que se cierra lo suelta).
//  Nada de esto toca el hilo de audio: el audio solo lee átomos.
// =====================================================================
class PolarLink : private juce::Thread
{
public:
    static constexpr int port = 9360;

    static std::shared_ptr<PolarLink> acquire();
    ~PolarLink() override;

    // false si de esta pista todavía no ha llegado nada
    bool read (int track, PolarProtocol::TrackState& out) const;
    bool isAlive() const;                          // llegó algún paquete en el último segundo y medio
    bool isBound() const { return bound.load(); }  // false = el puerto está ocupado por otro programa

private:
    PolarLink();
    void run() override;

    std::array<PolarProtocol::TrackAtomic, PolarProtocol::kTracks> tracks;
    std::atomic<bool> bound { false }, everReceived { false };
    std::atomic<juce::uint32> lastPacket { 0 };
    std::unique_ptr<juce::DatagramSocket> socket;   // solo lo toca el hilo de red
};
