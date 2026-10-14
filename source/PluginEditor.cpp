#include "PluginEditor.h"
#include "dsp/Presets.h"

namespace
{
const juce::Colour panelTop (0xfff4f5f7), panelBottom (0xffe2e5ea), ink (0xff3b4250), inkSoft (0xff8a92a0);

constexpr int kWidth = 560, kHeight = 330;
constexpr float kSplashRadius = 86.0f;

juce::Path hexagon (juce::Point<float> c, float r)
{
    juce::Path p;
    for (int i = 0; i < 6; ++i)
    {
        const float a = juce::MathConstants<float>::pi / 3.0f * (float) i + juce::MathConstants<float>::pi / 6.0f;
        const auto pt = c.getPointOnCircumference (r, a);
        i == 0 ? p.startNewSubPath (pt) : p.lineTo (pt);
    }
    p.closeSubPath();
    return p;
}
} // namespace

void FreezeLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                          float startAngle, float endAngle, juce::Slider&)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const auto c = bounds.getCentre();
    const float ringRadius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 4.0f;
    const float capRadius = ringRadius - juce::jmax (8.0f, ringRadius * 0.2f);
    const float angle = startAngle + sliderPos * (endAngle - startAngle);

    // Tick dots around the ring
    for (int i = 0; i <= 20; ++i)
    {
        const float a = startAngle + (endAngle - startAngle) * (float) i / 20.0f;
        const auto p = c.getPointOnCircumference (ringRadius + 3.5f, a);
        g.setColour (juce::Colour (0xffaab1bc).withAlpha (0.8f));
        g.fillEllipse (juce::Rectangle<float> (2.0f, 2.0f).withCentre (p));
    }

    // Track and value arc
    juce::Path track, value;
    track.addCentredArc (c.x, c.y, ringRadius - 3.0f, ringRadius - 3.0f, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colour (0xffc9ced6).withAlpha (0.6f));
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (sliderPos > 0.0f)
    {
        value.addCentredArc (c.x, c.y, ringRadius - 3.0f, ringRadius - 3.0f, 0.0f, startAngle, angle, true);
        g.setGradientFill (juce::ColourGradient (accent, c.x - ringRadius, c.y + ringRadius, accentLight,
                                                 c.x + ringRadius, c.y - ringRadius, false));
        g.strokePath (value, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Cap with soft shadow
    juce::Path cap;
    cap.addEllipse (juce::Rectangle<float> (capRadius * 2.0f, capRadius * 2.0f).withCentre (c));
    juce::DropShadow (juce::Colour (0x55102040), 14, { 0, 5 }).drawForPath (g, cap);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white, c.x, c.y - capRadius, juce::Colour (0xffdfe3e9),
                                             c.x, c.y + capRadius, false));
    g.fillPath (cap);
    g.setColour (juce::Colour (0xffc9ced6));
    g.strokePath (cap, juce::PathStrokeType (1.0f));

    // Pointer
    const auto inner = c.getPointOnCircumference (capRadius * 0.35f, angle);
    const auto outer = c.getPointOnCircumference (capRadius * 0.85f, angle);
    g.setColour (ink);
    g.drawLine ({ inner, outer }, 2.5f);
}

FreezeEditor::FreezeEditor (FreezeProcessor& p)
    : AudioProcessorEditor (p),
      freezeProcessor (p)
{
    setupKnob (amount, "Amount", "freeze");
    setupKnob (grain, "Grain", "grain");
    setupKnob (fx, "FX", "fx");
    amount.slider.onValueChange = [this] { updateLabel (amount); repaint(); };

    for (auto* button : { &previous, &next })
    {
        button->setLookAndFeel (&lookAndFeel);
        button->setColour (juce::TextButton::textColourOffId, inkSoft);
        button->setColour (juce::TextButton::textColourOnId, ink);
        addAndMakeVisible (*button);
    }
    previous.onClick = [this] { stepPreset (-1); };
    next.onClick = [this] { stepPreset (1); };

    presetName.setJustificationType (juce::Justification::centred);
    presetName.setFont (juce::FontOptions (17.0f, juce::Font::bold));
    presetName.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (presetName);

    // Shards scattered around the hexagon edge; fixed seed so the splash always looks the same.
    juce::Random rng (2026);
    for (int i = 0; i < 70; ++i)
    {
        const float a = rng.nextFloat() * juce::MathConstants<float>::twoPi;
        const float reach = rng.nextFloat();
        const float dist = kSplashRadius * (0.85f + 0.55f * reach);
        const float size = 3.0f + 9.0f * rng.nextFloat() * (1.0f - 0.6f * reach);
        const auto centre = juce::Point<float>().getPointOnCircumference (dist, a);
        juce::Path tri;
        tri.addTriangle (centre.getPointOnCircumference (size, rng.nextFloat() * 6.28f),
                         centre.getPointOnCircumference (size, rng.nextFloat() * 6.28f),
                         centre.getPointOnCircumference (size * 0.6f, rng.nextFloat() * 6.28f));
        shards.push_back ({ tri, reach, 0.45f + 0.55f * rng.nextFloat() });
    }

    applyPresetColours();
    setSize (kWidth, kHeight);
    startTimerHz (20);
}

FreezeEditor::~FreezeEditor()
{
    for (auto* knob : { &amount, &grain, &fx })
        knob->slider.setLookAndFeel (nullptr);
    previous.setLookAndFeel (nullptr);
    next.setLookAndFeel (nullptr);
}

void FreezeEditor::setupKnob (Knob& knob, const juce::String& name, const juce::String& paramId)
{
    knob.name = name;
    knob.paramId = paramId;
    knob.slider.setLookAndFeel (&lookAndFeel);
    knob.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob.slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    knob.slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    knob.slider.onValueChange = [this, &knob] { updateLabel (knob); };
    knob.slider.onDragStart = knob.slider.onDragEnd = [this, &knob] { updateLabel (knob); };
    knob.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        freezeProcessor.apvts, paramId, knob.slider);
    addAndMakeVisible (knob.slider);

    knob.label.setJustificationType (juce::Justification::centred);
    knob.label.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    knob.label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (knob.label);
    updateLabel (knob);
}

void FreezeEditor::updateLabel (Knob& knob)
{
    // Shows the knob's name, or its value while it is being dragged.
    const bool showValue = knob.slider.isMouseButtonDown();
    knob.label.setText (showValue ? freezeProcessor.apvts.getParameter (knob.paramId)->getCurrentValueAsText() : knob.name,
                        juce::dontSendNotification);
}

void FreezeEditor::stepPreset (int delta)
{
    const int index = (freezeProcessor.currentPreset() + delta + gf::kNumPresets) % gf::kNumPresets;
    freezeProcessor.setCurrentProgram (index);
}

void FreezeEditor::timerCallback()
{
    if (freezeProcessor.currentPreset() != shownPreset)
        applyPresetColours();
}

void FreezeEditor::applyPresetColours()
{
    shownPreset = freezeProcessor.currentPreset();
    const auto& preset = gf::preset (shownPreset);
    accent = juce::Colour (preset.accent);
    accentLight = juce::Colour (preset.accentLight);
    accentDeep = juce::Colour (preset.accentDeep);
    lookAndFeel.accent = accent;
    lookAndFeel.accentLight = accentLight;

    presetName.setText (preset.name, juce::dontSendNotification);
    presetName.setColour (juce::Label::textColourId, ink);
    for (auto* knob : { &amount, &grain, &fx })
        knob->label.setColour (juce::Label::textColourId, accentDeep);
    repaint();
}

void FreezeEditor::drawSplash (juce::Graphics& g, juce::Point<float> c, float level) const
{
    const float grow = 0.82f + 0.18f * level;
    auto hex = hexagon (c, kSplashRadius * grow);
    g.setGradientFill (juce::ColourGradient (accentLight.withAlpha (0.55f + 0.45f * level), c.x - 70.0f, c.y - 70.0f,
                                             accentDeep.withAlpha (0.45f + 0.55f * level), c.x + 70.0f, c.y + 80.0f, false));
    g.fillPath (hex);

    // Faint facets inside the hexagon
    g.setColour (juce::Colours::white.withAlpha (0.12f + 0.1f * level));
    for (int i = 0; i < 6; ++i)
    {
        const float a = juce::MathConstants<float>::pi / 3.0f * (float) i + juce::MathConstants<float>::pi / 6.0f;
        g.drawLine ({ c, c.getPointOnCircumference (kSplashRadius * grow, a) }, 1.0f);
    }

    // Shards break away further as the amount rises
    for (const auto& s : shards)
    {
        const float visible = juce::jlimit (0.0f, 1.0f, (level * 1.3f - s.reach * 0.9f) * 2.0f + 0.25f);
        if (visible <= 0.0f)
            continue;
        const float push = 1.0f + 0.12f * level * s.reach;
        auto shape = s.shape;
        shape.applyTransform (juce::AffineTransform::scale (push).translated (c));
        g.setColour (accent.interpolatedWith (accentLight, s.reach).withAlpha (s.alpha * visible));
        g.fillPath (shape);
    }
}

void FreezeEditor::drawSnowflake (juce::Graphics& g, juce::Point<float> c, float r, float level) const
{
    g.setColour (inkSoft.interpolatedWith (accent, level).withAlpha (0.45f + 0.55f * level));
    for (int i = 0; i < 6; ++i)
    {
        const float a = juce::MathConstants<float>::pi / 3.0f * (float) i;
        g.drawLine ({ c, c.getPointOnCircumference (r, a) }, 1.4f);
        const auto mid = c.getPointOnCircumference (r * 0.6f, a);
        g.drawLine ({ mid, mid.getPointOnCircumference (r * 0.3f, a - 0.8f) }, 1.2f);
        g.drawLine ({ mid, mid.getPointOnCircumference (r * 0.3f, a + 0.8f) }, 1.2f);
    }
}

void FreezeEditor::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float level = (float) amount.slider.getValue();

    g.setGradientFill (juce::ColourGradient (panelTop, 0.0f, 0.0f, panelBottom, 0.0f, bounds.getHeight(), false));
    g.fillAll();
    g.setColour (juce::Colour (0xffc8ccd3));
    g.drawRect (bounds, 1.0f);

    // Top strip
    g.setColour (juce::Colour (0xffd5d9df));
    g.drawHorizontalLine (40, 12.0f, bounds.getRight() - 12.0f);

    drawSplash (g, amount.slider.getBounds().toFloat().getCentre(), level);

    // Letter-spaced wordmark
    juce::GlyphArrangement word;
    word.addLineOfText (juce::FontOptions (20.0f), "FREEZE", 18.0f, 27.0f);
    for (int i = 0; i < word.getNumGlyphs(); ++i)
        word.moveRangeOfGlyphs (i, 1, 3.0f * (float) i, 0.0f);
    g.setColour (ink);
    word.draw (g);

    drawSnowflake (g, { bounds.getRight() - 28.0f, 20.0f }, 10.0f, level);
}

void FreezeEditor::resized()
{
    const auto centreX = getWidth() / 2;
    presetName.setBounds (juce::Rectangle<int> (140, 24).withCentre ({ centreX, 20 }));
    previous.setBounds (presetName.getX() - 28, 8, 26, 24);
    next.setBounds (presetName.getRight() + 2, 8, 26, 24);

    // Big Amount knob in the middle, Grain and FX side by side underneath it
    amount.slider.setBounds (juce::Rectangle<int> (136, 136).withCentre ({ centreX, 132 }));
    grain.slider.setBounds (juce::Rectangle<int> (66, 66).withCentre ({ centreX - 78, 262 }));
    fx.slider.setBounds (juce::Rectangle<int> (66, 66).withCentre ({ centreX + 78, 262 }));

    for (auto* knob : { &amount, &grain, &fx })
        knob->label.setBounds (juce::Rectangle<int> (110, 20).withCentre ({ knob->slider.getBounds().getCentreX(),
                                                                           knob->slider.getBottom() + 8 }));
}
