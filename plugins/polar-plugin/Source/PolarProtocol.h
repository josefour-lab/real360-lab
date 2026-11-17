#pragma once
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

// =====================================================================
//  Protocolo POLAR → plugins (UDP, 127.0.0.1:9360). Solo biblioteca estándar: se prueba fuera de JUCE.
//
//  Paquete (little-endian). Cabecera de 12 bytes:  'PLR1' · uint32 secuencia · uint8 nº de pistas (16) · 3 bytes libres.
//  Después, por pista (20 bytes):
//    float32 ángulo en grados (0 delante, + derecha, ±180 detrás)
//    float32 radio (0 centro … 1 borde; distancia «cuadrada», la del pad)
//    float32 spread en grados (−180…180; solo vale en «con apertura»; el signo intercambia L y R)
//    float32 ganancia lineal (volumen, mute y solo ya aplicados)
//    uint8   banderas: 1 activa · 2 mono · 4 silenciada · 8 efecto · 16 protegida · 32 balance
//            (ni mono ni balance = «con apertura»)
//    3 bytes libres
//  POLAR lo manda cuando algo cambia (como mucho 60 veces por segundo) y, en reposo, un latido cada 250 ms.
// =====================================================================
namespace PolarProtocol
{
    constexpr int kTracks = 16;
    constexpr size_t kHeaderBytes = 12, kTrackBytes = 20;
    constexpr uint8_t kActive = 1, kMono = 2, kSilent = 4, kEffect = 8, kProtected = 16, kBalance = 32;

    struct TrackState
    {
        float angle = 0.0f, radius = 0.0f, spread = 0.0f, gain = 1.0f;
        uint8_t flags = kBalance;
    };

    // Versión con átomos: la escribe el hilo de red y la lee el audio sin bloqueos
    struct TrackAtomic
    {
        std::atomic<float> angle { 0.0f }, radius { 0.0f }, spread { 0.0f }, gain { 1.0f };
        std::atomic<uint8_t> flags { kBalance };
        std::atomic<bool> valid { false };

        void store (const TrackState& s)
        {
            angle = s.angle; radius = s.radius; spread = s.spread; gain = s.gain; flags = s.flags; valid = true;
        }
        TrackState load() const
        {
            TrackState s;
            s.angle = angle.load(); s.radius = radius.load(); s.spread = spread.load(); s.gain = gain.load(); s.flags = flags.load();
            return s;
        }
    };

    inline float clampf (float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

    // Lee un paquete. Devuelve false si no es de POLAR o está incompleto; los valores raros se acotan.
    inline bool parsePacket (const uint8_t* d, size_t len, uint32_t& seq, std::array<TrackState, kTracks>& out)
    {
        if (d == nullptr || len < kHeaderBytes || std::memcmp (d, "PLR1", 4) != 0)
            return false;
        std::memcpy (&seq, d + 4, 4);
        const size_t n = d[8] < kTracks ? d[8] : (size_t) kTracks;
        if (len < kHeaderBytes + n * kTrackBytes)
            return false;
        for (size_t i = 0; i < n; ++i)
        {
            const uint8_t* p = d + kHeaderBytes + i * kTrackBytes;
            float f[4];
            std::memcpy (f, p, 16);
            for (float v : f)
                if (! std::isfinite (v))
                    return false;
            auto& s = out[i];
            s.angle = clampf (f[0], -360.0f, 360.0f);
            s.radius = clampf (f[1], 0.0f, 1.0f);
            s.spread = clampf (f[2], -180.0f, 180.0f);
            s.gain = clampf (f[3], 0.0f, 4.0f);
            s.flags = p[16];
        }
        return true;
    }
}
