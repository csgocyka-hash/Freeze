#pragma once
#include "PluginProcessor.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

// Light knobs with a soft shadow and a thin value ring in the preset's accent colours.
class FreezeLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override {}

    juce::Colour accent { 0xff4f9dff }, accentLight { 0xffa8e0ff }, accentDeep { 0xff2a55e6 };
};

// The snowflake in the top right: toggles Freeze (hold the sound playing now). Grey when off, lit in the preset colour with a glow when on.
class SnowflakeButton : public juce::Button
{
public:
    SnowflakeButton() : juce::Button ("Freeze")
    {
        setClickingTogglesState (true);
        setTooltip ("Freeze: keep playing the sound frozen right now");
    }

    void paintButton (juce::Graphics&, bool isHighlighted, bool isDown) override;

    juce::Colour accent { 0xff4f9dff }, accentLight { 0xffa8e0ff };
};

// A small drag control drawn as an icon that fills with the preset colour from the bottom up as its value
// rises (grey at 0 %, fully lit at 100 %). Drag up or down; double-click resets to 0.
class FillControl : public juce::Slider
{
public:
    explicit FillControl (const juce::String& name);
    void paint (juce::Graphics&) override;

    juce::Colour accent { 0xff4f9dff }, accentLight { 0xffa8e0ff };

protected:
    // Draws the icon in one colour inside `area`; top and bottom (0..1) are where the fill starts and ends
    virtual void drawIcon (juce::Graphics&, juce::Rectangle<float> area, juce::Colour) = 0;
    float iconTop = 0.0f, iconBottom = 1.0f;
};

// Bottom left, Noise: a halftone dot pattern that reads as noise
class NoiseControl : public FillControl
{
public:
    NoiseControl();

private:
    void drawIcon (juce::Graphics&, juce::Rectangle<float>, juce::Colour) override;
    struct Dot
    {
        float x, y, radius; // in a unit square
    };
    std::vector<Dot> dots;
};

// Bottom right, Detune: thin horizontal lines, each nudged slightly left or right and warped like a twisted
// ribbon
class DetuneControl : public FillControl
{
public:
    DetuneControl();

private:
    void drawIcon (juce::Graphics&, juce::Rectangle<float>, juce::Colour) override;
    struct Line
    {
        float y, thickness, left, right; // in a unit square
    };
    std::vector<Line> lines;
};

// The plugin's face, at its design size: wordmark and preset selector on top, the big Amount knob in the middle over an ice splash
// that shatters as the amount rises, and the small Grain and FX knobs underneath. Colours follow the preset.
class FreezePanel : public juce::Component, private juce::Timer
{
public:
    static constexpr int designWidth = 440, designHeight = 400; // compact

    explicit FreezePanel (FreezeProcessor&);
    ~FreezePanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Shard
    {
        juce::Path shape;
        float reach; // 0..1: how far out the shard sits, so outer shards appear later
        float alpha;
    };

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        juce::String name, paramId;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void timerCallback() override;
    void setupKnob (Knob&, const juce::String& name, const juce::String& paramId);
    void updateLabel (Knob&);
    void stepPreset (int delta);
    void applyPresetColours();
    void drawSplash (juce::Graphics&, juce::Point<float> centre, float amount) const;
    juce::Path splashShape (juce::Point<float> centre, float radius, float rough, int seedOffset, float wobbleScale) const;

    FreezeProcessor& freezeProcessor;
    FreezeLookAndFeel lookAndFeel;
    Knob amount, grain, fx;
    juce::TextButton previous { juce::CharPointer_UTF8 ("\xe2\x80\xb9") }, next { juce::CharPointer_UTF8 ("\xe2\x80\xba") };
    juce::Label presetName;
    SnowflakeButton snowflake;
    NoiseControl noiseControl;
    DetuneControl detuneControl;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> holdAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> noiseAttachment, detuneAttachment;
    std::vector<Shard> shards;
    std::vector<float> edgeNoise; // -1..1 per edge point of the splash
    int shownPreset = -1;
    juce::Image noise;
    juce::Colour accent, accentLight, accentDeep;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FreezePanel)
};

// The plugin window: holds the panel, drawn at its design size and scaled to the window. It opens small
// (70 %) and can be resized from the corner, keeping its proportions; the chosen size is saved.
class FreezeEditor : public juce::AudioProcessorEditor
{
public:
    explicit FreezeEditor (FreezeProcessor&);
    void resized() override;

    // Typefaces for the wordmark (light), preset name (medium) and labels (semibold): embedded Montserrat,
    // replaceable for previews
    struct FontSet
    {
        juce::Typeface::Ptr light, medium, semibold;
        bool capitals = false; // preset name and labels in spaced capitals
    };
    static FontSet& fonts();

    static constexpr float defaultScale = 0.7f, minScale = 0.5f, maxScale = 1.5f;

private:
    FreezeProcessor& freezeProcessor;
    FreezePanel panel;
    bool sizeIsSet = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FreezeEditor)
};
