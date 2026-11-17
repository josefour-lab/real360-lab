// Receptor de prueba: hace lo mismo que el plugin (escucha el puerto y lee los paquetes con PolarProtocol.h)
// y escribe lo que ha entendido, una línea por paquete: pista angulo radio spread ganancia banderas
//   g++ -std=c++17 -I ../Source udp_check.cpp -o udp_check && ./udp_check 9360 5
#include <cstdio>
#include <cstdlib>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/time.h>
#include "PolarProtocol.h"
int main (int argc, char** argv)
{
    const int port = argc > 1 ? std::atoi (argv[1]) : 9360, maxPk = argc > 2 ? std::atoi (argv[2]) : 5;
    int s = socket (AF_INET, SOCK_DGRAM, 0);
    sockaddr_in a {}; a.sin_family = AF_INET; a.sin_port = htons ((uint16_t) port); a.sin_addr.s_addr = inet_addr ("127.0.0.1");
    if (bind (s, (sockaddr*) &a, sizeof a) != 0) { std::printf("NO PUEDO ABRIR EL PUERTO %d\n", port); return 2; }
    timeval tv { 6, 0 }; setsockopt (s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    std::printf("LISTO\n"); std::fflush (stdout);
    uint8_t buf[2048]; int got = 0; std::array<PolarProtocol::TrackState, 16> st {}; uint32_t seq = 0;
    while (got < maxPk)
    {
        const ssize_t n = recv (s, buf, sizeof buf, 0);
        if (n <= 0) break;
        if (! PolarProtocol::parsePacket (buf, (size_t) n, seq, st)) { std::printf("PAQUETE NO VALIDO\n"); continue; }
        ++got;
        std::printf("PAQUETE %u\n", seq);
        for (int i = 0; i < 16; ++i) std::printf("%d %.4f %.4f %.4f %.4f %d\n", i, st[(size_t) i].angle, st[(size_t) i].radius, st[(size_t) i].spread, st[(size_t) i].gain, (int) st[(size_t) i].flags);
        std::fflush (stdout);
    }
    return 0;
}
