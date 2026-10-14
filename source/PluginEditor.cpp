#include "PluginEditor.h"

namespace
{
const juce::Colour panelTop (0xfff4f5f7), panelBottom (0xffe2e5ea), ink (0xff3b4250), inkSoft (0xff8a92a0);
const juce::Colour iceLight (0xffa8e0ff), iceMid (0xff4f9dff), iceDeep (0xff2a55e6);

constexpr int kWidth = 560, kHeight = 210;
constexpr float kSplashRadius = 82.0f;

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
    const float capRadius = ringRadius - 11.0f;
    const float angle = startAngle + sliderPos * (endAngle - startAngle);

    // Tick dots around the ring
    for (int i = 0; i <= 20; ++i)
    {
        const float a = startAngle + (endAngle - startAngle) * (float) i / 20.0f;
        const auto p = c.getPointOnCircumference (ringRadius + 3.5f, a);
        g.setColour (juce::Colours::white.withAlpha (0.75f));
        g.fillEllipse (juce::Rectangle<float> (2.0f, 2.0f).withCentre (p));
    }

    // Track and value arc
    juce::Path track, value;
    track.addCentredArc (c.x, c.y, ringRadius - 3.0f, ringRadius - 3.0f, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (sliderPos > 0.0f)
    {
        value.addCentredArc (c.x, c.y, ringRadius - 3.0f, ringRadius - 3.0f, 0.0f, startAngle, angle, true);
        g.setGradientFill (juce::ColourGradient (iceLight, c.x - ringRadius, c.y + ringRadius, juce::Colours::white,
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
      freezeProcessor (p),
      attachment (p.apvts, "freeze", knob)
{
    knob.setLookAndFeel (&lookAndFeel);
    knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    knob.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    knob.setDoubleClickReturnValue (true, 0.0);
    knob.onValueChange = [this] { updateLabel(); repaint(); };
    knob.onDragStart = knob.onDragEnd = [this] { updateLabel(); };
    addAndMakeVisible (knob);

    valueLabel.setJustificationType (juce::Justification::centred);
    valueLabel.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    valueLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (valueLabel);

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

    updateLabel();
    setSize (kWidth, kHeight);
}

FreezeEditor::~FreezeEditor()
{
    knob.setLookAndFeel (nullptr);
}

void FreezeEditor::updateLabel()
{
    const bool showValue = knob.isMouseButtonDown();
    valueLabel.setText (showValue ? freezeProcessor.apvts.getParameter ("freeze")->getCurrentValueAsText() : "Freeze",
                        juce::dontSendNotification);
    valueLabel.setColour (juce::Label::textColourId, iceDeep);
}

void FreezeEditor::drawSplash (juce::Graphics& g, juce::Point<float> c, float amount) const
{
    const float grow = 0.82f + 0.18f * amount;
    auto hex = hexagon (c, kSplashRadius * grow);
    g.setGradientFill (juce::ColourGradient (iceLight.withAlpha (0.55f + 0.45f * amount), c.x - 70.0f, c.y - 70.0f,
                                             iceDeep.withAlpha (0.45f + 0.55f * amount), c.x + 70.0f, c.y + 80.0f, false));
    g.fillPath (hex);

    // Faint facets inside the hexagon
    g.setColour (juce::Colours::white.withAlpha (0.12f + 0.1f * amount));
    for (int i = 0; i < 6; ++i)
    {
        const float a = juce::MathConstants<float>::pi / 3.0f * (float) i + juce::MathConstants<float>::pi / 6.0f;
        g.drawLine ({ c, c.getPointOnCircumference (kSplashRadius * grow, a) }, 1.0f);
    }

    // Shards break away further as the amount rises
    for (const auto& s : shards)
    {
        const float visible = juce::jlimit (0.0f, 1.0f, (amount * 1.3f - s.reach * 0.9f) * 2.0f + 0.25f);
        if (visible <= 0.0f)
            continue;
        const float push = 1.0f + 0.12f * amount * s.reach;
        auto shape = s.shape;
        shape.applyTransform (juce::AffineTransform::scale (push).translated (c));
        g.setColour (iceMid.interpolatedWith (iceLight, s.reach).withAlpha (s.alpha * visible));
        g.fillPath (shape);
    }
}

void FreezeEditor::drawSnowflake (juce::Graphics& g, juce::Point<float> c, float r, float amount) const
{
    g.setColour (inkSoft.interpolatedWith (iceMid, amount).withAlpha (0.45f + 0.55f * amount));
    for (int i = 0; i < 6; ++i)
    {
        const float a = juce::MathConstants<float>::pi / 3.0f * (float) i;
        const auto tip = c.getPointOnCircumference (r, a);
        g.drawLine ({ c, tip }, 1.6f);
        const auto mid = c.getPointOnCircumference (r * 0.6f, a);
        g.drawLine ({ mid, mid.getPointOnCircumference (r * 0.3f, a - 0.8f) }, 1.4f);
        g.drawLine ({ mid, mid.getPointOnCircumference (r * 0.3f, a + 0.8f) }, 1.4f);
    }
}

void FreezeEditor::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float amount = (float) knob.getValue();

    g.setGradientFill (juce::ColourGradient (panelTop, 0.0f, 0.0f, panelBottom, 0.0f, bounds.getHeight(), false));
    g.fillAll();
    g.setColour (juce::Colour (0xffc8ccd3));
    g.drawRect (bounds, 1.0f);

    drawSplash (g, knob.getBounds().toFloat().getCentre(), amount);

    // Wordmark
    g.setColour (ink);
    g.setFont (juce::FontOptions (34.0f));
    juce::GlyphArrangement word;
    word.addLineOfText (juce::FontOptions (34.0f), "FREEZE", 32.0f, bounds.getCentreY() + 12.0f);
    // Letter-space the wordmark by hand for a light, airy look
    for (int i = 0; i < word.getNumGlyphs(); ++i)
        word.moveRangeOfGlyphs (i, 1, 4.0f * (float) i, 0.0f);
    word.draw (g);

    drawSnowflake (g, { bounds.getRight() - 70.0f, bounds.getCentreY() }, 22.0f, amount);
}

void FreezeEditor::resized()
{
    const auto centre = getLocalBounds().getCentre();
    knob.setBounds (juce::Rectangle<int> (120, 120).withCentre (centre.translated (0, -8)));
    valueLabel.setBounds (juce::Rectangle<int> (120, 20).withCentre ({ centre.x, knob.getBottom() + 10 }));
}
