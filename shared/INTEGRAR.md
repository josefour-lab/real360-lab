# Kit POLAR · REAL360

Pad cuadrafónico y estéreo de Looper Búho (geometría de POLAR), con su paneo, los
movimientos (órbita, paseo aleatorio, paseo por el borde, por altavoces, rebote, ocho,
péndulo, respiración…) y la estética. Independiente de Looper Búho: sirve para el
Codex Resonator o cualquier otro plugin.

## Archivos

| Archivo | Qué es |
|---|---|
| `Source/QuadPanner.h` | Paneo: cuadrafónico (4 altavoces) y estéreo, modos **balance estéreo (como Logic)**, **punto mono** y **con apertura** (L y R separados), con «Proteger el centro». `QuadPanner::sourceGains (...)` da las ganancias de una fuente estéreo. |
| `Source/SpatialPoint.h` | Posición de una fuente: ángulo, distancia, apertura, velocidad y modo de movimiento (atómicos: se leen desde el audio sin bloqueos). |
| `Source/Movimientos.h/.cpp` | Motor de movimientos en JavaScript, recargable en caliente (en su propio hilo). |
| `Scripts/movimientos.js` | Los modos de movimiento. Se copia a `~/Library/Application Support/REAL360/Looper Polar/movimientos.js`, **el mismo archivo que usa Looper Búho**: lo que cambies sirve para los dos. |
| `Source/PolarPad.h/.cpp` | El pad (dibujo, altavoces con porcentaje, abanico L/R, zona protegida, puntos arrastrables). Se conecta al plugin con `PolarHost`. |
| `Source/PolarLookAndFeel.h` | La estética: paleta, fuentes, faders y botones de POLAR / Looper Búho, con temas. |
| `Resources/` | Logotipos de REAL360 y del búho (opcionales). |

## 1. CMakeLists.txt

```cmake
set (SHARED ${CMAKE_CURRENT_SOURCE_DIR}/../../shared)
add_subdirectory (${SHARED}/../ThirdParty/JUCE ${CMAKE_BINARY_DIR}/JUCE)

target_sources (CodexResonator PRIVATE
    ${SHARED}/Source/Movimientos.cpp
    ${SHARED}/Source/PolarPad.cpp)
target_include_directories (CodexResonator PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source ${SHARED}/Source)

juce_add_binary_data (CodexData SOURCES        # el script de serie, sus versiones anteriores y el logo
    ${SHARED}/Scripts/movimientos.js
    ${SHARED}/Scripts/anteriores/movimientos_v1.js
    ${SHARED}/Scripts/anteriores/movimientos_v2.js
    ${SHARED}/Resources/real360.svg)
target_link_libraries (CodexResonator PRIVATE CodexData juce::juce_javascript)
```

Si ya tienes un `juce_add_binary_data`, añade esos archivos a su lista. No copies nada de shared/ dentro del plugin.

**Salida cuadrafónica:** el plugin necesita un bus de salida de 4 canales (además del estéreo):

```cpp
// en el constructor del procesador
: AudioProcessor (BusesProperties()
      .withInput  ("Entrada", juce::AudioChannelSet::stereo(), true)
      .withOutput ("Salida",  juce::AudioChannelSet::quadraphonic(), true))

bool isBusesLayoutSupported (const BusesLayout& l) const override
{
    // Pistas mono, estéreo o cuadrafónicas
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    const bool inOk = in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
    return inOk && (out == juce::AudioChannelSet::quadraphonic()
                    || out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono());
}
```

Con **salida mono**, haz como Looper Búho: procesa por dentro en un búfer estéreo (con la entrada mono
copiada a los dos canales) y al final suma los dos canales en uno, multiplicando cada uno por 0,7071.
Con **entrada mono**, copia el canal 0 también al canal 1 antes de procesar.

## 2. Procesador

```cpp
#include "QuadPanner.h"
#include "SpatialPoint.h"
#include "Movimientos.h"

// miembros
SpatialPoint point;                    // la fuente (el resonador)
Movimientos movimientos;
QuadPanner::Gains lastL {}, lastR {};  // para suavizar entre bloques
std::atomic<int> panMode { QuadPanner::PanBalance };
std::atomic<bool> protect { false };

// en el constructor
point.angle = 0.0f; point.radius = 0.0f; point.width = 90.0f; point.speed = 20.0f; point.mode = 0;
movimientos.setSources ({ &point }, [] (int) { return true; });
movimientos.startMovimientos();

// al final de processBlock, cuando tu señal estéreo ya está en los canales 0 y 1
const int n = buffer.getNumSamples();
const bool stereoOut = buffer.getNumChannels() < 4;
QuadPanner::Gains gl, gr;
QuadPanner::sourceGains (point.angle.load(), point.radius.load(), point.width.load(),
                         panMode.load(), protect.load(), stereoOut, gl, gr);

juce::AudioBuffer<float> src;          // copia de L y R (mejor como miembro, con setSize en prepareToPlay)
src.makeCopyOf (buffer, true);
const int outs = stereoOut ? 2 : 4;
for (int ch = 0; ch < outs; ++ch)
{
    auto* o = buffer.getWritePointer (ch);
    const float* l = src.getReadPointer (0);
    const float* r = src.getReadPointer (1);
    for (int s = 0; s < n; ++s)
    {
        const float t = (float) (s + 1) / (float) n;    // rampa: sin clics al moverse
        const float gL = lastL[ch] + (gl[ch] - lastL[ch]) * t;
        const float gR = lastR[ch] + (gr[ch] - lastR[ch]) * t;
        o[s] = l[s] * gL + r[s] * gR;
    }
}
lastL = gl;
lastR = gr;
```

Guarda la posición con el proyecto (en `getStateInformation` / `setStateInformation`):
`angle`, `radius`, `width`, `speed` y `mode` de `point`, y `panMode` / `protect`.

## 3. Editor

```cpp
#include "PolarPad.h"

class CodexEditor : public juce::AudioProcessorEditor, private PolarHost, private juce::Timer
{
    // PolarHost
    int numSources() const override { return 1; }
    SpatialPoint& point (int) override { return proc.point; }
    bool isQuadOutput() const override { return proc.getTotalNumOutputChannels() >= 4; }
    int panMode() const override { return proc.panMode.load(); }
    bool isProtected() const override { return proc.protect.load() && proc.panMode.load() == QuadPanner::PanApertura; }
    juce::StringArray movementNames() const override { return proc.movimientos.getModeNames(); }
    juce::String sourceLabel (int) const override { return "C"; }   // C de Codex
    juce::Colour sourceColour (int) const override { return PolarStyle::accent; }

    void timerCallback() override { pad.repaint(); }   // el punto se mueve solo con los movimientos

    PolarLookAndFeel lnf;
    PolarPad pad { *this };
    CodexProcessor& proc;
};

// en el constructor
setLookAndFeel (&lnf);
addAndMakeVisible (pad);
startTimerHz (30);
// en el destructor
setLookAndFeel (nullptr);
// en resized()
pad.setBounds (10, 10, 420, 470);   // el pad es cuadrado; deja ~50 px debajo para el estado
```

Para elegir el movimiento, un `ComboBox` con `proc.movimientos.getModeNames()` que escriba
`proc.point.mode = id - 1`. Para la apertura y la velocidad, dos faders que escriban
`point.width` y `point.speed`. Para el paneo, otro `ComboBox` con
«Balance estéreo (como Logic) / Punto mono / Con apertura» que escriba `panMode`.

## 4. Temas

`PolarStyle::applyTheme (n)` cambia la paleta: 0 marrón (Looper Búho Normal), 1 azul,
2 amarillo, 3 verde. Después llama a `lnf.applyThemeColours()` y `repaint()`.
Para un tema propio, crea `Source/TemaPropio.h` en el plugin (la forma está explicada al principio de
`PolarLookAndFeel.h`; ejemplos en Stutter Quad, Microloops, Codex Resonator, Pájaro y POLAR plugin).
No cambies `shared/Source/PolarLookAndFeel.h` para un solo plugin.

---
Kit extraído de Looper Búho · REAL360. El pad usa la geometría de la app POLAR.
