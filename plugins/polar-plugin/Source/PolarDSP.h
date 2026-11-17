#pragma once
#include <algorithm>
#include "QuadPanner.h"
#include "PolarProtocol.h"

// =====================================================================
//  Paneo de una pista de POLAR. Solo biblioteca estándar: se prueba fuera de JUCE.
//  Usa la misma ley que Looper Búho (QuadPanner.h): balance estéreo, punto mono o con apertura.
//  Orden de salida: 0 = L (delante izq.), 1 = R (delante der.), 2 = Ls (detrás izq.), 3 = Rs (detrás der.).
// =====================================================================
struct PolarDSP
{
    QuadPanner::Gains lastL {}, lastR {};
    bool primed = false;

    void reset() { primed = false; }

    static int panModeOf (const PolarProtocol::TrackState& s)
    {
        if (s.flags & PolarProtocol::kMono)    return QuadPanner::PanMono;
        if (s.flags & PolarProtocol::kBalance) return QuadPanner::PanBalance;
        return QuadPanner::PanApertura;
    }

    // Ganancias destino de L y R, con el volumen de POLAR (mute y solo incluidos) ya aplicado.
    // La protección del centro la resuelve POLAR (la posición que manda ya viene fuera de la zona).
    static void targetGains (const PolarProtocol::TrackState& s, bool stereoOut, QuadPanner::Gains& gl, QuadPanner::Gains& gr)
    {
        QuadPanner::sourceGains (s.angle, s.radius, s.spread, panModeOf (s), false, stereoOut, gl, gr);
        for (auto& g : gl) g *= s.gain;
        for (auto& g : gr) g *= s.gain;
    }

    // inL / inR: entrada (no pueden ser los mismos búferes que la salida).
    // out: punteros a los canales de salida; outChannels = 2 (estéreo) o 4 (cuadrafónico).
    // Las ganancias suben o bajan en rampa a lo largo del bloque: sin clics al moverse.
    void process (const float* inL, const float* inR, float* const* out, int outChannels, int n, const PolarProtocol::TrackState& s)
    {
        const bool stereoOut = outChannels < 4;
        const int chans = stereoOut ? 2 : 4;
        QuadPanner::Gains gl, gr;
        targetGains (s, stereoOut, gl, gr);
        if (! primed) { lastL = gl; lastR = gr; primed = true; }   // la primera vez no hay rampa

        for (int ch = 0; ch < chans; ++ch)
        {
            float* o = out[ch];
            const float l0 = lastL[(size_t) ch], r0 = lastR[(size_t) ch];
            const float dl = gl[(size_t) ch] - l0, dr = gr[(size_t) ch] - r0;
            for (int i = 0; i < n; ++i)
            {
                const float t = (float) (i + 1) / (float) n;
                o[i] = inL[i] * (l0 + dl * t) + inR[i] * (r0 + dr * t);
            }
        }
        lastL = gl;
        lastR = gr;
    }
};
