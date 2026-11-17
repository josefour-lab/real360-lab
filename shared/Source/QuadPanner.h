#pragma once
#include <algorithm>
#include <array>
#include <cmath>

// Orden de salida (quadraphonic de JUCE, el que usa Logic en Quadro):
// 0 = L (frontal izq), 1 = R (frontal der), 2 = Ls (trasero izq), 3 = Rs (trasero der)
//
// Reparto por pares de altavoces vecinos con ley de potencia constante,
// mezclado con un reparto uniforme según el radio: en el centro suena igual
// en los cuatro, en el borde solo en los dos altavoces más cercanos.
// La potencia total se mantiene constante en cualquier posición.

struct QuadPanner
{
    // Modos de paneo: balance estéreo (como Logic), punto mono, o L y R separados por la apertura
    enum PanMode { PanBalance = 0, PanMono, PanApertura };
    static constexpr float kProtectRadius = 0.5f;   // con «Proteger el centro», nada entra en este radio

    using Gains = std::array<float, 4>;

    static Gains pointGains (float angleDeg, float radius)
    {
        // Altavoces en orden circular desde FL: FL(-45), FR(45), RR(135), RL(-135)
        static constexpr int outIndex[4] = { 0, 1, 3, 2 };
        constexpr float halfPi = 1.57079632679f;

        float a = std::fmod (angleDeg + 45.0f, 360.0f);
        if (a < 0.0f) a += 360.0f;

        const int seg = std::min (3, (int) (a / 90.0f));
        const float f = (a - (float) seg * 90.0f) / 90.0f;

        Gains pair {};
        pair[(size_t) outIndex[seg]]           = std::cos (f * halfPi);
        pair[(size_t) outIndex[(seg + 1) % 4]] = std::sin (f * halfPi);

        const float r = std::clamp (radius, 0.0f, 1.0f);
        const float rr = r * r;

        Gains g {};
        for (size_t i = 0; i < 4; ++i)
            g[i] = std::sqrt (rr * pair[i] * pair[i] + (1.0f - rr) * 0.25f);

        return g;
    }

    // Balance estéreo, como el panner y el Surround Balancer de Logic: el canal L va siempre a
    // los altavoces de la izquierda (1 y 3) y el R a los de la derecha (2 y 4). La posición
    // reparte entre delante y detrás, y entre los dos lados (como el balance de un equipo de
    // música). En el centro suena por los cuatro altavoces, sin mezclar L y R.
    // Geometría de POLAR (distancia de tipo cuadrado). stereo = salida de 2 canales.
    static void balanceGains (float angleDeg, float radius, bool stereo, Gains& gl, Gains& gr)
    {
        constexpr float halfPi = 1.57079632679f;
        const float t = angleDeg * 0.0174532925f;
        const float s = std::sin (t), c = std::cos (t);
        const float m = std::max (std::max (std::abs (s), std::abs (c)), 1.0e-6f);
        const float r = std::clamp (radius, 0.0f, 1.0f);
        const float u = std::clamp (r * s / m, -1.0f, 1.0f);   // derecha (+) / izquierda (-)
        const float w = std::clamp (r * c / m, -1.0f, 1.0f);   // delante (+) / detrás (-)

        const float sideL = u > 0.0f ? std::cos (u * halfPi) : 1.0f;
        const float sideR = u < 0.0f ? std::cos (-u * halfPi) : 1.0f;
        const float front = std::sqrt ((1.0f + w) * 0.5f);
        const float back  = std::sqrt ((1.0f - w) * 0.5f);

        gl = {};
        gr = {};
        if (stereo)
        {
            gl[0] = sideL;
            gr[1] = sideR;
            return;
        }
        gl[0] = sideL * front;   // L → altavoz 1 (delante izquierda)
        gl[2] = sideL * back;    // L → altavoz 3 (detrás izquierda)
        gr[1] = sideR * front;   // R → altavoz 2 (delante derecha)
        gr[3] = sideR * back;    // R → altavoz 4 (detrás derecha)
    }

    // Salida estéreo: reparto izquierda/derecha según la posición horizontal en el pad
    // (geometría de POLAR, distancia de tipo cuadrado). Índices 0 = izquierda, 1 = derecha.
    static Gains stereoGains (float angleDeg, float radius)
    {
        constexpr float halfPi = 1.57079632679f;
        const float t = angleDeg * 0.0174532925f;
        const float s = std::sin (t), c = std::cos (t);
        const float m = std::max (std::abs (s), std::abs (c));
        const float u = std::clamp (std::clamp (radius, 0.0f, 1.0f) * s / std::max (m, 1.0e-6f), -1.0f, 1.0f);
        const float p = (u + 1.0f) * 0.5f;
        Gains g {};
        g[0] = std::cos (p * halfPi);
        g[1] = std::sin (p * halfPi);
        return g;
    }

    // Ganancias de una fuente estéreo (L y R) según su punto, el modo de paneo, la protección
    // del centro y si la salida es estéreo (2 canales) o cuadrafónica (4)
    static void sourceGains (float angleDeg, float radius, float widthDeg, int panMode, bool protect, bool stereo,
                             Gains& gl, Gains& gr)
    {
        if (panMode == PanBalance)
        {
            balanceGains (angleDeg, radius, stereo, gl, gr);
            return;
        }
        if (panMode == PanMono)
        {
            gl = stereo ? stereoGains (angleDeg, radius) : pointGains (angleDeg, radius);
            for (auto& g : gl)
                g *= 0.70710678f;
            gr = gl;
            return;
        }
        float r = radius;
        if (protect)
            r = kProtectRadius + (1.0f - kProtectRadius) * r;
        gl = stereo ? stereoGains (angleDeg - 0.5f * widthDeg, r) : pointGains (angleDeg - 0.5f * widthDeg, r);
        gr = stereo ? stereoGains (angleDeg + 0.5f * widthDeg, r) : pointGains (angleDeg + 0.5f * widthDeg, r);
    }
};
