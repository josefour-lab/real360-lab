#!/usr/bin/env python3
# Escucha lo que POLAR manda a los plugins (UDP 127.0.0.1:9360) y lo muestra legible.
# Sirve para comprobar la conexión antes de tener el plugin. Uso, en Terminal:
#     python3 escuchar-plugin.py
# En POLAR: Configuración → Conexión con Logic → Salida del paneo → «Plugin» o «Ambos».
# Cierra con Control+C. Importante: ciérralo antes de abrir Logic con el plugin (solo uno puede usar el puerto).
import socket, struct, sys, time
PORT = 9360
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.bind(('127.0.0.1', PORT))
print(f'Escuchando en 127.0.0.1:{PORT}… (Control+C para salir)')
last = 0
while True:
    d, _ = s.recvfrom(2048)
    if d[:4] != b'PLR1' or len(d) < 12:
        continue
    seq, n = struct.unpack_from('<IB', d, 4)
    if time.time() - last < 0.25:      # una línea por cuarto de segundo, para poder leerla
        continue
    last = time.time()
    print(f'\npaquete {seq}')
    for t in range(min(n, 16)):
        ang, r, sp, g = struct.unpack_from('<ffff', d, 12 + t * 20)
        fl = d[12 + t * 20 + 16]
        if not fl & 1:
            continue
        tags = ('mono ' if fl & 2 else ('balance ' if fl & 32 else 'apertura ')) + ('SILENCIO ' if fl & 4 else '') + ('efecto ' if fl & 8 else '') + ('protegida' if fl & 16 else '')
        db = f'{20 * __import__("math").log10(g):+.1f} dB' if g > 0 else '  −∞  '
        print(f'  pista {t + 1:2d}   ángulo {ang:+7.1f}°   radio {r:4.2f}   spread {sp:+6.1f}°   volumen {db}   {tags}')
