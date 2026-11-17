#pragma once
#include <atomic>

// Posición de una fuente en el polar de Looper Polar.
// Ángulo en grados: 0 = frente, positivo = hacia la derecha, ±180 = detrás.
// Radio: 0 = centro (sonido repartido en los 4 altavoces), 1 = borde.
// En ejes X/Y: x = sin(ángulo) * radio, y = cos(ángulo) * radio (y positivo = frente).
// Apertura: separación en grados entre el canal L y el R de la fuente estéreo.
// Modo: 0 = Fijo; 1..N = modos definidos en movimientos.js.

struct SpatialPoint
{
    std::atomic<float> angle  { 0.0f };
    std::atomic<float> radius { 0.8f };
    std::atomic<float> width  { 60.0f };
    std::atomic<float> speed  { 20.0f };
    std::atomic<int>   mode   { 0 };
    std::atomic<bool>  dragging { false };

    static float wrap (float deg)
    {
        while (deg > 180.0f)  deg -= 360.0f;
        while (deg < -180.0f) deg += 360.0f;
        return deg;
    }
};
