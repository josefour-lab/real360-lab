# POLAR · plugin (REAL360)

Panea una pista en cuadrafónico siguiendo lo que manda la app POLAR. Mismo esquema que Looper Búho:
efecto con entrada MIDI, entrada mono o estéreo, salida cuadrafónica (L, R, Ls, Rs), y el mismo `QuadPanner.h`.

## Compilar (como Looper Búho)

    cd real360-lab/plugins/polar-plugin
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release -j

(El generador de Xcode falla en este Mac: usar el de por defecto.)

`COPY_PLUGIN_AFTER_BUILD` lo copia solo a `~/Library/Audio/Plug-Ins/Components` (AU) y `.../VST3`.
Después, en Logic: **Ajustes › Gestor de módulos › «Restablecer y volver a comprobar la selección»**.
Si luego cambias los canales aceptados, sube `VERSION` en el `CMakeLists.txt`.

## Usar

1. En la app POLAR, **Configuración › Salida del paneo › Plugin** (es lo que hace por defecto al abrirse).
2. En Logic, el proyecto con formato surround **Cuadrafónico** (Archivo › Ajustes del proyecto › Audio).
3. Inserta **Audio Units › REAL360 › POLAR** en una pista de audio y elige la variante **«Estéreo → Cuadrafónico»**.
4. En el plugin, pulsa el **sello rojo «PISTA»** (abajo a la izquierda) y elige **Pista 1…16**: el número que ves en la ficha de POLAR. Una instancia por pista.
5. Los puntos de colores de POLAR mandan: posición, volumen, mute, solo, Balance / Mono / Apertura.
   En el plugin, la pista es el **sello rojo pequeño** que se mueve por la luna; con «Apertura» se talla un arco entre L y R.

Si cierras POLAR, la pista **se queda en la última posición** (también al reabrir el proyecto).
El punto de abajo a la derecha indica la conexión: **azul** = conectado, **hueco** = sin señal (se queda en la última posición), **rojo** = puerto ocupado.

## Notas

- Escucha en `127.0.0.1:9360` (UDP). Todas las instancias del plugin comparten un solo receptor.
  Si ves «Puerto 9360 ocupado», hay otro programa usándolo (por ejemplo `escuchar-plugin.py`: ciérralo).
- El plugin usa «Balance» por defecto, como Looper Búho: L suena por los altavoces de la izquierda y R por los de la derecha.
- La protección del centro la resuelve POLAR (la posición que manda ya viene fuera de la zona); el plugin no hace nada más.
- Para cambiar el nombre que se ve en Logic: `PRODUCT_NAME` y `PLUGIN_NAME` en el `CMakeLists.txt`.
- El logo de REAL360 va en la cabecera, en tinta azul (`shared/Resources/real360.svg`).
- Estética **linograbado** (papel crema, tinta azul tallada, sellos rojos): la maqueta aprobada es `maqueta-linograbado.jpg`.
  Las letras (Futura y Bradley Hand) vienen con todos los Mac.

## Pruebas (sin JUCE, en Terminal)

    cd Tests
    g++ -std=c++17 -I ../Source -I ../../../shared/Source test_core.cpp -o test_core && ./test_core     # paneo, balance, mono, apertura, rampas, paquetes
    g++ -std=c++17 -I ../Source -I ../../../shared/Source udp_check.cpp -o udp_check && ./udp_check 9360 5   # lee 5 paquetes de POLAR como lo hace el plugin

## Archivos

| Archivo | Qué es |
|---|---|
| `Source/PolarProtocol.h` | Formato del paquete UDP y su lectura (solo biblioteca estándar) |
| `Source/PolarDSP.h` | El paneo de una pista, con rampas de ganancia (solo biblioteca estándar) |
| `shared/Source/QuadPanner.h` | La ley de paneo de Looper Búho, sin cambios |
| `Source/PolarLink.h/.cpp` | Receptor UDP compartido por todas las instancias |
| `Source/PluginProcessor.h/.cpp` | El procesador: buses, parámetro «Pista», guardado de estado |
| `Source/PluginEditor.h/.cpp` | La ventana: la luna tallada, el sello de la pista, el pie a lápiz, el selector, la conexión, el logo |
| `Source/Linograbado.h` | La estética linograbado: colores, letras, papel, grano de la tinta, anillos tallados |
| `Source/TemaPropio.h` | Colores del tema 4 para los menús (lo carga `shared/Source/PolarLookAndFeel.h`) |
