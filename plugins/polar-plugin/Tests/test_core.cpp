// Pruebas del núcleo del plugin (sin JUCE): paquetes, paneo y rampas.
//   g++ -std=c++17 -I ../Source test_core.cpp -o test_core && ./test_core
#include <cstdio>
#include <vector>
#include "PolarDSP.h"
using namespace PolarProtocol;

static int fails = 0;
static void ok (bool c, const char* msg) { std::printf("%s %s\n", c ? "✔" : "✘", msg); if (! c) ++fails; }

static std::vector<uint8_t> makePacket (const std::array<TrackState, 16>& t, uint32_t seq)
{
    std::vector<uint8_t> b (kHeaderBytes + 16 * kTrackBytes, 0);
    std::memcpy (b.data(), "PLR1", 4); std::memcpy (b.data() + 4, &seq, 4); b[8] = 16;
    for (int i = 0; i < 16; ++i)
    {
        uint8_t* p = b.data() + kHeaderBytes + i * kTrackBytes;
        float f[4] = { t[(size_t) i].angle, t[(size_t) i].radius, t[(size_t) i].spread, t[(size_t) i].gain };
        std::memcpy (p, f, 16); p[16] = t[(size_t) i].flags;
    }
    return b;
}

// Pasa un bloque de ruido por el panner y devuelve la energía (RMS) de cada salida. L y R se pueden elegir.
static std::array<double, 4> rms (const TrackState& s, bool useL, bool useR, int outCh, PolarDSP& dsp, int blocks = 4)
{
    const int n = 256;
    std::vector<float> l (n), r (n); std::array<std::vector<float>, 4> o; for (auto& v : o) v.assign (n, 0.f);
    float* outs[4] = { o[0].data(), o[1].data(), o[2].data(), o[3].data() };
    std::array<double, 4> e {};
    unsigned seed = 12345;
    for (int b = 0; b < blocks; ++b)
    {
        for (int i = 0; i < n; ++i) { seed = seed * 1664525u + 1013904223u; const float x = ((seed >> 9) / 4194304.0f) - 1.0f; l[(size_t) i] = useL ? x : 0.f; r[(size_t) i] = useR ? x : 0.f; }
        dsp.process (l.data(), r.data(), outs, outCh, n, s);
        for (int c = 0; c < 4; ++c) for (int i = 0; i < n; ++i) e[(size_t) c] += (double) o[(size_t) c][(size_t) i] * o[(size_t) c][(size_t) i];
    }
    for (auto& v : e) v = std::sqrt (v / (blocks * n));
    return e;
}

int main()
{
    // ---------- paquetes ----------
    std::array<TrackState, 16> t {};
    t[0] = { 58.5f, 0.46f, 103.5f, 1.0f, kActive };                     // apertura
    t[1] = { -136.6f, 0.49f, 0.0f, 1.99f, kActive | kBalance };         // balance
    t[2] = { 0.0f, 0.5f, 0.0f, 0.0f, kActive | kMono | kSilent };       // mono, silenciada
    auto pk = makePacket (t, 77);
    std::array<TrackState, 16> r {}; uint32_t seq = 0;
    ok (parsePacket (pk.data(), pk.size(), seq, r) && seq == 77, "paquete válido: se lee y la secuencia es 77");
    ok (r[0].angle == 58.5f && r[0].radius == 0.46f && r[0].spread == 103.5f && r[0].gain == 1.0f, "pista 1: ángulo, radio, spread y ganancia exactos");
    ok (r[1].angle == -136.6f && (r[1].flags & kBalance) && r[1].gain > 1.98f, "pista 2: balance, ángulo −136,6°");
    ok ((r[2].flags & kMono) && (r[2].flags & kSilent) && r[2].gain == 0.0f, "pista 3: mono y silenciada");
    ok (! parsePacket (pk.data(), 20, seq, r), "paquete cortado: se rechaza");
    auto bad = pk; bad[0] = 'X'; ok (! parsePacket (bad.data(), bad.size(), seq, r), "otra cabecera: se rechaza");
    auto nan = pk; float q = std::nanf (""); std::memcpy (nan.data() + kHeaderBytes, &q, 4); ok (! parsePacket (nan.data(), nan.size(), seq, r), "un NaN en el paquete: se rechaza");

    // ---------- balance: L solo por la izquierda, R solo por la derecha, en cualquier posición ----------
    {
        bool izq = true, der = true;
        const float pos[][2] = { { 0, 0 }, { 0, 0.6f }, { 45, 1 }, { -90, 1 }, { 180, 0.8f }, { 135, 1 }, { 30, 0.3f } };
        for (auto& p : pos)
        {
            TrackState s { p[0], p[1], 0, 1, kBalance };
            PolarDSP d1, d2;
            auto el = rms (s, true, false, 4, d1), er = rms (s, false, true, 4, d2);
            izq &= el[1] < 1e-6 && el[3] < 1e-6;   // L no llega a los altavoces de la derecha (2 y 4)
            der &= er[0] < 1e-6 && er[2] < 1e-6;   // R no llega a los de la izquierda (1 y 3)
        }
        ok (izq, "balance: el canal L nunca suena por los altavoces 2 y 4 (derecha)");
        ok (der, "balance: el canal R nunca suena por los altavoces 1 y 3 (izquierda)");
    }
    // balance en el centro: todos los altavoces suenan y L y R siguen separados (no se mezclan: sin cancelación)
    {
        TrackState s { 0, 0, 0, 1, kBalance }; PolarDSP d;
        auto e = rms (s, true, true, 4, d);
        ok (e[0] > 0.1 && e[1] > 0.1 && e[2] > 0.1 && e[3] > 0.1, "balance en el centro: suenan los cuatro altavoces");
        // con L = −R (fuera de fase) en el centro: la suma por altavoz no se anula en balance
        const int n = 256; std::vector<float> l (n), rr (n); std::array<std::vector<float>, 4> o; for (auto& v : o) v.assign (n, 0.f);
        float* outs[4] = { o[0].data(), o[1].data(), o[2].data(), o[3].data() };
        for (int i = 0; i < n; ++i) { l[(size_t) i] = std::sin (i * 0.2f); rr[(size_t) i] = -l[(size_t) i]; }
        PolarDSP d2; d2.process (l.data(), rr.data(), outs, 4, n, s);
        double en = 0; for (int c = 0; c < 4; ++c) for (int i = 0; i < n; ++i) en += (double) o[(size_t) c][(size_t) i] * o[(size_t) c][(size_t) i];
        ok (en > 50.0, "balance en el centro con L y R en oposición de fase: no se cancela");
    }
    // ---------- balance, pista a la izquierda: todo por la izquierda ----------
    {
        TrackState s { -90, 1, 0, 1, kBalance }; PolarDSP d; auto e = rms (s, true, true, 4, d);
        ok (e[1] < 1e-6 && e[3] < 1e-6 && e[0] > 0.1 && e[2] > 0.1, "balance a la izquierda del todo: solo suenan los altavoces 1 y 3");
    }
    // ---------- mono: un punto ----------
    {
        TrackState s { 45, 1, 0, 1, kMono }; PolarDSP d; auto e = rms (s, true, true, 4, d);
        ok (e[1] > 0.5 && e[0] < 1e-6 && e[2] < 1e-6 && e[3] < 1e-6, "mono en la esquina delantera derecha: solo suena el altavoz 2");
    }
    // ---------- apertura ----------
    {
        TrackState s { 0, 1, 90, 1, 0 }; PolarDSP d1, d2;
        auto el = rms (s, true, false, 4, d1), er = rms (s, false, true, 4, d2);
        ok (el[0] > 0.5 && el[1] < 1e-3 && el[2] < 1e-3 && el[3] < 1e-3, "apertura 90° delante: L solo en el altavoz 1");
        ok (er[1] > 0.5 && er[0] < 1e-3 && er[2] < 1e-3 && er[3] < 1e-3, "apertura 90° delante: R solo en el altavoz 2");
        TrackState inv { 0, 1, -90, 1, 0 }; PolarDSP d3; auto ei = rms (inv, true, false, 4, d3);
        ok (ei[1] > 0.5 && ei[0] < 1e-3, "apertura con signo negativo: L y R se intercambian");
    }
    // ---------- potencia constante del punto en mono ----------
    {
        bool cte = true;
        for (float a = -180; a <= 180; a += 23) for (float rad = 0; rad <= 1.0f; rad += 0.25f)
        { TrackState s { a, rad, 0, 1, kMono }; QuadPanner::Gains gl, gr; PolarDSP::targetGains (s, false, gl, gr);
          double p = 0; for (int i = 0; i < 4; ++i) p += (double) gl[(size_t) i] * gl[(size_t) i]; cte &= std::abs (p - 0.5) < 1e-5; }
        ok (cte, "mono: la potencia total es la misma en cualquier punto del pad");
    }
    // ---------- volumen, mute y rampa ----------
    {
        TrackState s { 0, 0.5f, 0, 0.0f, kBalance | kSilent }; PolarDSP d; auto e = rms (s, true, true, 4, d);
        ok (e[0] + e[1] + e[2] + e[3] < 1e-9, "silenciada (ganancia 0): no sale nada");
        TrackState u { 0, 0.5f, 0, 1.0f, kBalance }, v = u; v.gain = 2.0f;
        PolarDSP d1, d2; auto a = rms (u, true, true, 4, d1), b = rms (v, true, true, 4, d2);
        ok (std::abs (b[0] / a[0] - 2.0) < 1e-3, "ganancia 2 (+6 dB): el doble de nivel");
    }
    {   // continuidad: al cambiar de posición entre bloques no hay saltos grandes
        const int n = 128; std::vector<float> l (n, 1.0f), rr (n, 1.0f); std::array<std::vector<float>, 4> o; for (auto& v : o) v.assign (n, 0.f);
        float* outs[4] = { o[0].data(), o[1].data(), o[2].data(), o[3].data() };
        PolarDSP d; TrackState a { -90, 1, 0, 1, kBalance }, b { 90, 1, 0, 1, kBalance };
        d.process (l.data(), rr.data(), outs, 4, n, a); const float fin = o[0][n - 1];
        d.process (l.data(), rr.data(), outs, 4, n, b); const float ini = o[0][0];
        float maxStep = 0; for (int i = 1; i < n; ++i) maxStep = std::max (maxStep, std::abs (o[0][(size_t) i] - o[0][(size_t) i - 1]));
        ok (std::abs (ini - fin) < 0.05f && maxStep < 0.05f, "salto de −90° a +90° entre bloques: sin clic (rampa de ganancia)");
    }
    // ---------- salida estéreo ----------
    {
        TrackState s { 0, 0.4f, 0, 1, kBalance }; PolarDSP d1, d2;
        auto el = rms (s, true, false, 2, d1), er = rms (s, false, true, 2, d2);
        ok (el[0] > 0.5 && el[1] < 1e-6 && er[1] > 0.5 && er[0] < 1e-6 && el[2] == 0 && el[3] == 0, "salida estéreo en balance: L al canal izquierdo, R al derecho");
    }
    std::printf("\n%s\n", fails ? "HAY FALLOS" : "TODO CORRECTO");
    return fails ? 1 : 0;
}
