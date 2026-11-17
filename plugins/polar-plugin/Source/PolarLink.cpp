#include "PolarLink.h"

std::shared_ptr<PolarLink> PolarLink::acquire()
{
    static juce::CriticalSection lock;
    static std::weak_ptr<PolarLink> weak;
    const juce::ScopedLock sl (lock);
    if (auto existing = weak.lock())
        return existing;
    std::shared_ptr<PolarLink> fresh (new PolarLink());
    weak = fresh;
    return fresh;
}

PolarLink::PolarLink() : juce::Thread ("POLAR Link")
{
    startThread();
}

PolarLink::~PolarLink()
{
    stopThread (2000);
}

bool PolarLink::read (int track, PolarProtocol::TrackState& out) const
{
    if (track < 0 || track >= PolarProtocol::kTracks || ! tracks[(size_t) track].valid.load())
        return false;
    out = tracks[(size_t) track].load();
    return true;
}

bool PolarLink::isAlive() const
{
    return everReceived.load() && (juce::Time::getMillisecondCounter() - lastPacket.load()) < 1500;
}

void PolarLink::run()
{
    std::array<PolarProtocol::TrackState, PolarProtocol::kTracks> st {};
    juce::uint8 buf[2048];

    while (! threadShouldExit())
    {
        if (socket == nullptr)
        {
            auto s = std::make_unique<juce::DatagramSocket> (false);
            if (s->bindToPort (port, "127.0.0.1"))
            {
                socket = std::move (s);
                bound = true;
            }
            else
            {
                bound = false;      // puerto ocupado: se reintenta cada segundo
                wait (1000);
                continue;
            }
        }

        const int ready = socket->waitUntilReady (true, 100);
        if (ready < 0)              // error del socket: se vuelve a abrir
        {
            socket.reset();
            bound = false;
            wait (500);
            continue;
        }
        if (ready == 0)
            continue;

        const int n = socket->read (buf, (int) sizeof (buf), false);
        uint32_t seq = 0;
        if (n > 0 && PolarProtocol::parsePacket (buf, (size_t) n, seq, st))
        {
            for (size_t i = 0; i < st.size(); ++i)
                tracks[i].store (st[i]);
            lastPacket = juce::Time::getMillisecondCounter();
            everReceived = true;
        }
    }

    socket.reset();
    bound = false;
}
