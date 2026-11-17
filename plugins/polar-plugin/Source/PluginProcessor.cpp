#include "PluginProcessor.h"
#include "PluginEditor.h"

PolarProcessor::PolarProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Entrada", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Salida",  juce::AudioChannelSet::quadraphonic(), true)),
      apvts (*this, nullptr, "POLAR", createLayout()),
      link (PolarLink::acquire())
{
    trackParam = apvts.getRawParameterValue ("track");
}

juce::AudioProcessorValueTreeState::ParameterLayout PolarProcessor::createLayout()
{
    // «Pista» 1..16 = el número que ves en la ficha de la app POLAR (por dentro 0..15)
    return { std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "track", 1 }, "Pista", 0, 15, 0,
        juce::AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return juce::String (v + 1); })) };
}

bool PolarProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool PolarProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool PolarProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

bool PolarProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Pistas mono o estéreo; salida cuadrafónica, estéreo o mono (igual que Looper Búho)
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo())
        return false;
    return out == juce::AudioChannelSet::quadraphonic() || out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void PolarProcessor::prepareToPlay (double, int samplesPerBlock)
{
    const int cap = juce::jmax (samplesPerBlock, 512);
    src.setSize (2, cap);
    tmp.setSize (2, cap);
    dsp.reset();
}

int PolarProcessor::currentTrack() const
{
    return juce::jlimit (0, PolarProtocol::kTracks - 1, (int) std::lround (trackParam->load()));
}

PolarProtocol::TrackState PolarProcessor::currentState() const
{
    PolarProtocol::TrackState s;
    if (link->read (currentTrack(), s))
        return s;
    return held.load();
}

void PolarProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int n = buffer.getNumSamples();
    const int inCh = getTotalNumInputChannels(), outCh = getTotalNumOutputChannels();
    if (n <= 0 || inCh < 1 || outCh < 1)
        return;

    // Lo último que mandó POLAR; si POLAR no está, se queda la última posición conocida
    PolarProtocol::TrackState st;
    if (link->read (currentTrack(), st))
        held.store (st);
    else
        st = held.load();

    const int cap = src.getNumSamples();
    for (int pos = 0; pos < n; pos += cap)
    {
        const int m = juce::jmin (cap, n - pos);

        // Copia de la entrada (la salida cuadrafónica ocupa los mismos canales). Entrada mono: el canal 0 va también al 1.
        src.copyFrom (0, 0, buffer, 0, pos, m);
        src.copyFrom (1, 0, buffer, inCh > 1 ? 1 : 0, pos, m);

        if (outCh == 1)
        {
            // Salida mono: se procesa por dentro en estéreo y se suman los dos canales ×0,7071
            float* outs[2] = { tmp.getWritePointer (0), tmp.getWritePointer (1) };
            dsp.process (src.getReadPointer (0), src.getReadPointer (1), outs, 2, m, st);
            auto* dst = buffer.getWritePointer (0) + pos;
            for (int i = 0; i < m; ++i)
                dst[i] = 0.70710678f * (outs[0][i] + outs[1][i]);
        }
        else
        {
            const int chans = outCh >= 4 ? 4 : 2;
            float* outs[4] = {};
            for (int ch = 0; ch < chans; ++ch)
                outs[ch] = buffer.getWritePointer (ch) + pos;
            dsp.process (src.getReadPointer (0), src.getReadPointer (1), outs, chans, m, st);
        }
    }
}

juce::AudioProcessorEditor* PolarProcessor::createEditor()
{
    return new PolarEditor (*this);
}

void PolarProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
    {
        // Con POLAR cerrado, la pista vuelve a la posición en la que quedó (sin el volumen ni el mute de POLAR)
        const auto s = held.load();
        xml->setAttribute ("angle", (double) s.angle);
        xml->setAttribute ("radius", (double) s.radius);
        xml->setAttribute ("spread", (double) s.spread);
        xml->setAttribute ("flags", (int) (s.flags & ~PolarProtocol::kSilent));
        copyXmlToBinary (*xml, destData);
    }
}

void PolarProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            PolarProtocol::TrackState s;
            s.angle  = (float) xml->getDoubleAttribute ("angle", 0.0);
            s.radius = (float) xml->getDoubleAttribute ("radius", 0.0);
            s.spread = (float) xml->getDoubleAttribute ("spread", 0.0);
            s.gain   = 1.0f;
            s.flags  = (juce::uint8) xml->getIntAttribute ("flags", PolarProtocol::kBalance);
            held.store (s);
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PolarProcessor();
}
