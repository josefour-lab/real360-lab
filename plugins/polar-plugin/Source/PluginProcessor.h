#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PolarProtocol.h"
#include "PolarDSP.h"
#include "PolarLink.h"

// POLAR (plugin de REAL360): panea una pista en cuadrafónico siguiendo lo que manda la app POLAR.
// Entrada mono o estéreo, salida cuadrafónica (L, R, Ls, Rs), como Looper Búho.
class PolarProcessor : public juce::AudioProcessor
{
public:
    PolarProcessor();
    ~PolarProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "POLAR"; }
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    int currentTrack() const;                          // 0..15
    PolarProtocol::TrackState currentState() const;    // lo último de POLAR, o lo último guardado si POLAR no está
    bool linkAlive() const { return link->isAlive(); }
    bool linkBound() const { return link->isBound(); }

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    std::shared_ptr<PolarLink> link;
    std::atomic<float>* trackParam = nullptr;
    PolarProtocol::TrackAtomic held;      // la última posición conocida: con POLAR cerrado, la pista se queda ahí
    PolarDSP dsp;
    juce::AudioBuffer<float> src, tmp;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PolarProcessor)
};
