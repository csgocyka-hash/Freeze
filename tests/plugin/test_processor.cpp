#include "../Check.h"
#include "../Signals.h"
#include "PluginProcessor.h"
#include <juce_audio_utils/juce_audio_utils.h>

namespace
{
juce::AudioBuffer<float> stereoPlucks (int numSamples, int offset)
{
    static const auto plucks = pluckTrain (48000, 10.0);
    juce::AudioBuffer<float> buffer (2, numSamples);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < numSamples; ++i)
            buffer.setSample (ch, i, plucks[(size_t) ((offset + i) % (int) plucks.size())]);
    return buffer;
}

void setFreeze (FreezeProcessor& p, float value)
{
    auto* param = p.apvts.getParameter ("freeze");
    param->setValueNotifyingHost (param->convertTo0to1 (value));
}
} // namespace

TEST (parameter_layout)
{
    FreezeProcessor p;
    auto* param = dynamic_cast<juce::RangedAudioParameter*> (p.apvts.getParameter ("freeze"));
    CHECK (param != nullptr);
    CHECK_NEAR (param->convertFrom0to1 (param->getDefaultValue()), 0.0, 1e-6);
    CHECK_NEAR (param->getNormalisableRange().start, 0.0, 1e-6);
    CHECK_NEAR (param->getNormalisableRange().end, 1.0, 1e-6);
    CHECK (param->getText (0.5f, 10) == "50 %");
}

TEST (passthrough_at_zero)
{
    FreezeProcessor p;
    p.prepareToPlay (48000, 256);
    juce::MidiBuffer midi;
    for (int block = 0; block < 20; ++block)
    {
        auto buffer = stereoPlucks (256, block * 256);
        const auto original = buffer;
        p.processBlock (buffer, midi);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 256; ++i)
                CHECK (buffer.getSample (ch, i) == original.getSample (ch, i));
    }
}

TEST (oversized_host_block)
{
    FreezeProcessor p;
    p.prepareToPlay (48000, 256);
    setFreeze (p, 1.0f);
    juce::MidiBuffer midi;
    float sumSquares = 0.0f;
    bool finite = true;
    for (int block = 0; block < 60; ++block) // ~2.5 s of plucks in 2048-sample blocks
    {
        auto buffer = stereoPlucks (2048, block * 2048);
        p.processBlock (buffer, midi);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 2048; ++i)
            {
                const float x = buffer.getSample (ch, i);
                finite = finite && std::isfinite (x);
                sumSquares += x * x;
            }
    }
    CHECK (finite);
    CHECK (sumSquares > 0.0f);
}

TEST (state_round_trip)
{
    FreezeProcessor a;
    setFreeze (a, 0.7f);
    juce::MemoryBlock state;
    a.getStateInformation (state);

    FreezeProcessor b;
    b.setStateInformation (state.getData(), (int) state.getSize());
    CHECK_NEAR (b.apvts.getRawParameterValue ("freeze")->load(), 0.7, 1e-4);
}

TEST (bus_layouts)
{
    FreezeProcessor p;
    juce::AudioProcessor::BusesLayout mono, stereo, monoToStereo;
    mono.inputBuses.add (juce::AudioChannelSet::mono());
    mono.outputBuses.add (juce::AudioChannelSet::mono());
    stereo.inputBuses.add (juce::AudioChannelSet::stereo());
    stereo.outputBuses.add (juce::AudioChannelSet::stereo());
    monoToStereo.inputBuses.add (juce::AudioChannelSet::mono());
    monoToStereo.outputBuses.add (juce::AudioChannelSet::stereo());
    CHECK (p.checkBusesLayoutSupported (mono));
    CHECK (p.checkBusesLayoutSupported (stereo));
    CHECK (! p.checkBusesLayoutSupported (monoToStereo));
}

TEST (editor_opens)
{
    juce::ScopedJuceInitialiser_GUI gui;
    FreezeProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
    CHECK (editor != nullptr);
    CHECK (editor->getWidth() > editor->getHeight() * 2); // wide one-knob rectangle
}
