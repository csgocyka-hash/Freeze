#pragma once
#include "dsp/FreezeEngine.h"
#include "dsp/Snowfall.h"
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

    // Rhythm divisions: 1/8, 1/8 dotted, 1/4, 1/2 and 1 bar, in beats
    static constexpr double kDivisionBeats[] = { 0.5, 0.75, 1.0, 2.0, 4.0 };
    static juce::StringArray divisionNames() { return { "1/8", "1/8D", "1/4", "1/2", "1" }; }
    static juce::String pulseTimeText (float time); // e.g. "1/16"

private:
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;
    void setParameterValue (const char* id, float value);

    gf::FreezeEngine engine;
    gf::Snowfall snowfall;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> freezeSmoothed, fxSmoothed, noiseSmoothed;
    std::vector<float> amountBuffer, fxBuffer, noiseBuffer;
    std::atomic<float>* freezeParam = nullptr;
    std::atomic<float>* grainParam = nullptr;
    std::atomic<float>* fxParam = nullptr;
    std::atomic<float>* presetParam = nullptr;
    std::atomic<float>* holdParam = nullptr;
    std::atomic<float>* snowParam = nullptr;
    std::atomic<float>* detuneParam = nullptr;
    std::atomic<float>* divisionParam = nullptr;
    std::atomic<float>* unlinkParam = nullptr;
    std::atomic<float>* timeParam = nullptr;
    std::atomic<float>* invertParam = nullptr;
    int appliedPreset = -1;               // audio thread: preset whose density/FX the engine uses
    std::atomic<int> pendingPreset { -1 }; // preset whose starting values still need applying
    bool restoringState = false;
    int preparedBlockSize = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FreezeProcessor)
};
