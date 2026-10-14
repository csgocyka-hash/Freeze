#pragma once
#include "dsp/FreezeEngine.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

// Choosing a preset moves Grain and FX to the preset's starting values. That happens on the message
// thread through the AsyncUpdater (public so tests can flush it with handleUpdateNowIfNeeded()).
class FreezeProcessor : public juce::AudioProcessor,
                        public juce::AsyncUpdater,
                        private juce::AudioProcessorValueTreeState::Listener
{
public:
    FreezeProcessor();
    ~FreezeProcessor() override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Freeze"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    int currentPreset() const;

private:
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;
    void setParameterValue (const char* id, float value);

    gf::FreezeEngine engine;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> freezeSmoothed, fxSmoothed;
    std::vector<float> amountBuffer, fxBuffer;
    std::atomic<float>* freezeParam = nullptr;
    std::atomic<float>* grainParam = nullptr;
    std::atomic<float>* fxParam = nullptr;
    std::atomic<float>* presetParam = nullptr;
    int appliedPreset = -1;               // audio thread: preset whose density/FX the engine uses
    std::atomic<int> pendingPreset { -1 }; // preset whose starting values still need applying
    bool restoringState = false;
    int preparedBlockSize = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FreezeProcessor)
};
