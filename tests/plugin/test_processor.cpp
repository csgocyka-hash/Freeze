#include "../Check.h"
#include "../Signals.h"
#include "PluginProcessor.h"
#include "dsp/Presets.h"
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
    CHECK (editor->getWidth() > editor->getHeight()); // wide rectangle
}

namespace
{
float paramValue (FreezeProcessor& p, const char* id)
{
    return p.apvts.getRawParameterValue (id)->load();
}

void setParam (FreezeProcessor& p, const char* id, float value)
{
    auto* param = p.apvts.getParameter (id);
    param->setValueNotifyingHost (param->convertTo0to1 (value));
}
} // namespace

TEST (three_parameters_and_preset)
{
    FreezeProcessor p;
    for (auto* id : { "freeze", "grain", "fx", "preset" })
        CHECK (p.apvts.getParameter (id) != nullptr);
    CHECK_NEAR (paramValue (p, "grain"), gf::grainSecondsToKnob (0.120), 1e-4); // Ice
    CHECK_NEAR (paramValue (p, "fx"), 0.0, 1e-6);
    CHECK_NEAR (paramValue (p, "preset"), 0.0, 1e-6);
    auto* preset = dynamic_cast<juce::AudioParameterChoice*> (p.apvts.getParameter ("preset"));
    CHECK (preset != nullptr && preset->choices.size() == 5 && preset->choices[2] == "Haze");
}

TEST (preset_sets_grain_and_fx)
{
    juce::ScopedJuceInitialiser_GUI gui;
    FreezeProcessor p;
    setParam (p, "preset", 2.0f); // Haze
    p.handleUpdateNowIfNeeded();
    CHECK_NEAR (paramValue (p, "grain"), gf::grainSecondsToKnob (0.300), 1e-3);
    CHECK_NEAR (paramValue (p, "fx"), 0.4, 1e-3);
}

TEST (programs_are_presets)
{
    juce::ScopedJuceInitialiser_GUI gui;
    FreezeProcessor p;
    CHECK (p.getNumPrograms() == 5);
    CHECK (p.getProgramName (4) == "Abyss");
    p.setCurrentProgram (3);
    p.handleUpdateNowIfNeeded();
    CHECK (p.getCurrentProgram() == 3);
    CHECK_NEAR (paramValue (p, "fx"), 0.35, 1e-3);
}

TEST (state_round_trip_all_params)
{
    juce::ScopedJuceInitialiser_GUI gui;
    FreezeProcessor a;
    setParam (a, "preset", 4.0f);
    a.handleUpdateNowIfNeeded();
    setParam (a, "grain", 0.2f); // user tweaks after choosing the preset
    setParam (a, "fx", 0.9f);
    setParam (a, "freeze", 0.6f);
    juce::MemoryBlock state;
    a.getStateInformation (state);

    FreezeProcessor b;
    b.setStateInformation (state.getData(), (int) state.getSize());
    b.handleUpdateNowIfNeeded(); // restoring must not reset grain/fx to the preset's starting values
    CHECK_NEAR (paramValue (b, "preset"), 4.0, 1e-4);
    CHECK_NEAR (paramValue (b, "grain"), 0.2, 1e-3);
    CHECK_NEAR (paramValue (b, "fx"), 0.9, 1e-3);
    CHECK_NEAR (paramValue (b, "freeze"), 0.6, 1e-3);
}

TEST (editor_has_three_knobs)
{
    juce::ScopedJuceInitialiser_GUI gui;
    FreezeProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
    int sliders = 0;
    for (auto* child : editor->getChildren())
        sliders += dynamic_cast<juce::Slider*> (child) != nullptr ? 1 : 0;
    CHECK (sliders == 3);

    // Big Amount knob in the middle, the two small knobs below it
    juce::Slider* knobs[3] {};
    int n = 0;
    for (auto* child : editor->getChildren())
        if (auto* s = dynamic_cast<juce::Slider*> (child))
            knobs[n++] = s;
    std::sort (std::begin (knobs), std::end (knobs), [] (auto* a, auto* b) { return a->getWidth() > b->getWidth(); });
    CHECK (std::abs (knobs[0]->getBounds().getCentreX() - editor->getWidth() / 2) <= 1);
    CHECK (knobs[1]->getY() > knobs[0]->getBottom() - 10);
    CHECK (knobs[2]->getY() > knobs[0]->getBottom() - 10);
}
