#include "PluginEditor.h"
#include "dsp/Presets.h"

namespace
{
const juce::Colour panelTop (0xfff4f5f7), panelBottom (0xffe2e5ea), ink (0xff3b4250), inkSoft (0xff8a92a0);

constexpr int kWidth = 560, kHeight = 330;
constexpr float kSplashRadius = 86.0f;

// Brushed metal: brightness varies around the circle like light on a turned metal surface. JUCE has no
// conic gradient, so the ring is drawn as thin wedges.
void fillBrushedMetal (juce::Graphics& g, juce::Point<float> c, float outer, float inner, float light)
{
    constexpr int segments = 120;
    for (int i = 0; i < segments; ++i)
    {
        const float a0 = juce::MathConstants<float>::twoPi * (float) i / segments;
        const float a1 = juce::MathConstants<float>::twoPi * (float) (i + 1) / segments;
        const float mid = 0.5f * (a0 + a1);
        // Two bright bands (top-left and bottom-right) and darker sides, plus overall top-down light
        const float sheen = 0.5f + 0.5f * std::cos (2.0f * (mid + 0.6f));
        const float topLight = 0.5f - 0.5f * std::cos (mid); // 0 at top (angle 0 = 12 o'clock), 1 at bottom
        const float level = juce::jlimit (0.0f, 1.0f, light * (0.62f + 0.33f * sheen - 0.22f * topLight));
        juce::Path wedge;
        wedge.addPieSegment (juce::Rectangle<float> (outer * 2.0f, outer * 2.0f).withCentre (c), a0, a1 + 0.01f, inner / outer);
        g.setColour (juce::Colour::greyLevel (level));
        g.fillPath (wedge);
    }
}
} // namespace

void FreezeLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                          float startAngle, float endAngle, juce::Slider& slider)
{
    const bool onColour = slider.getProperties().getWithDefault ("onColour", false);
    // Square knob area at the top of the component; the space below holds the body and its shadow
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const float size = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const juce::Point<float> c (bounds.getCentreX(), bounds.getY() + size * 0.5f);
    const float ringRadius = size * 0.5f - 3.0f;          // value ring
    const float skirtRadius = ringRadius - size * 0.075f; // knurled metal edge
    const float capRadius = skirtRadius * 0.80f;          // domed top
    const float angle = startAngle + sliderPos * (endAngle - startAngle);
    const float ringWidth = juce::jmax (2.5f, size * 0.025f);

    // Value ring: recessed track, then the lit arc in the preset colour
    juce::Path track, value;
    track.addCentredArc (c.x, c.y, ringRadius, ringRadius, 0.0f, startAngle, endAngle, true);
    g.setColour (onColour ? juce::Colour (0x40000000) : juce::Colour (0xffc3c8d0));
    g.strokePath (track, juce::PathStrokeType (ringWidth + 1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (onColour ? juce::Colours::white.withAlpha (0.3f) : juce::Colour (0xffe9ecf0));
    g.strokePath (track, juce::PathStrokeType (ringWidth - 0.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (sliderPos > 0.0f)
    {
        value.addCentredArc (c.x, c.y, ringRadius, ringRadius, 0.0f, startAngle, angle, true);
        juce::DropShadow (accent.withAlpha (0.55f), (int) (ringWidth * 3.0f), {}).drawForPath (g, value);
        if (onColour)
            g.setColour (juce::Colours::white); // the big knob's ring sits on the coloured splash
        else
            g.setGradientFill (juce::ColourGradient (accent, c.x - ringRadius, c.y + ringRadius, accentLight,
                                                     c.x + ringRadius, c.y - ringRadius, false));
        g.strokePath (value, juce::PathStrokeType (ringWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Height of the knob body (taller for the small knobs), drawn as a metal cylinder wall
    const float bodyHeight = size * (float) slider.getProperties().getWithDefault ("height", 0.06);
    const auto top = juce::Rectangle<float> (skirtRadius * 2.0f, skirtRadius * 2.0f).withCentre (c);
    const auto bottom = top.translated (0.0f, bodyHeight);

    // Cast shadow under the body
    juce::Path footprint;
    footprint.addEllipse (bottom);
    juce::DropShadow (juce::Colour (0x70101828), (int) (size * 0.14f), { 0, (int) (size * 0.05f) }).drawForPath (g, footprint);
    juce::DropShadow (juce::Colour (0x40101828), (int) (size * 0.04f), { 0, (int) (size * 0.015f) }).drawForPath (g, footprint);

    // Cylinder wall: dark at the sides, bright down the middle, like a turned metal barrel
    juce::Path wall;
    wall.addRectangle (top.getX(), c.y, top.getWidth(), bodyHeight);
    wall.addEllipse (bottom);
    juce::ColourGradient barrel (juce::Colour (0xff5f656f), top.getX(), 0.0f, juce::Colour (0xff5f656f), top.getRight(), 0.0f, false);
    barrel.addColour (0.18, juce::Colour (0xffa9aeb6));
    barrel.addColour (0.42, juce::Colour (0xffeef0f3));
    barrel.addColour (0.55, juce::Colour (0xffc7cbd2));
    barrel.addColour (0.85, juce::Colour (0xff8a9099));
    g.setGradientFill (barrel);
    g.fillPath (wall);
    g.setColour (juce::Colour (0x50000000));
    g.drawEllipse (bottom, 0.8f);

    // Top face: smooth brushed-metal rim
    fillBrushedMetal (g, c, skirtRadius, capRadius * 1.06f, 1.0f);
    g.setColour (juce::Colour (0x40000000));
    g.drawEllipse (top, 0.8f);
    g.setColour (juce::Colours::white.withAlpha (0.6f));
    g.drawEllipse (top.reduced (1.0f), 0.6f);

    // Bevel between skirt and cap: dark below, light above
    const auto bevel = juce::Rectangle<float> (capRadius * 2.12f, capRadius * 2.12f).withCentre (c);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff9aa0a9), c.x, bevel.getY(), juce::Colour (0xfffcfdfe),
                                             c.x, bevel.getBottom(), false));
    g.fillEllipse (bevel);

    // Domed cap: off-centre radial gradient gives the curvature
    const auto capBounds = juce::Rectangle<float> (capRadius * 2.0f, capRadius * 2.0f).withCentre (c);
    juce::ColourGradient dome (juce::Colours::white, c.x - capRadius * 0.35f, c.y - capRadius * 0.45f,
                               juce::Colour (0xffc9ced6), c.x + capRadius * 0.7f, c.y + capRadius * 0.9f, true);
    dome.addColour (0.55, juce::Colour (0xffeef0f3));
    g.setGradientFill (dome);
    g.fillEllipse (capBounds);

    // Specular highlight on the upper half
    auto shine = capBounds.reduced (capRadius * 0.18f).withTrimmedBottom (capRadius * 0.85f).translated (0.0f, -capRadius * 0.05f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.85f), c.x, shine.getY(),
                                             juce::Colours::white.withAlpha (0.0f), c.x, shine.getBottom(), false));
    g.fillEllipse (shine);

    g.setColour (juce::Colour (0x30000000));
    g.drawEllipse (capBounds, 0.8f);

    // Pointer: a groove cut into the cap (dark line with a light lower edge)
    const auto inner = c.getPointOnCircumference (capRadius * 0.30f, angle);
    const auto outer = c.getPointOnCircumference (capRadius * 0.88f, angle);
    const float pointerWidth = juce::jmax (2.0f, size * 0.028f);
    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.drawLine ({ inner.translated (0.0f, 0.8f), outer.translated (0.0f, 0.8f) }, pointerWidth);
    g.setColour (juce::Colour (0xff2b303a));
    g.drawLine ({ inner.translated (0.0f, -0.6f), outer.translated (0.0f, -0.6f) }, pointerWidth);
}

FreezeEditor::FreezeEditor (FreezeProcessor& p)
    : AudioProcessorEditor (p),
      freezeProcessor (p)
{
    setupKnob (amount, "Amount", "freeze");
    setupKnob (grain, "Grain", "grain");
    setupKnob (fx, "FX", "fx");
    amount.slider.onValueChange = [this] { updateLabel (amount); repaint(); };
    amount.slider.getProperties().set ("onColour", true);
    amount.slider.getProperties().set ("height", 0.05);
    grain.slider.getProperties().set ("height", 0.20);
    fx.slider.getProperties().set ("height", 0.20);

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
    // Edge roughness: random values smoothed over neighbours, so the edge is chunky rather than hairy
    std::vector<float> raw (160);
    for (auto& v : raw)
        v = rng.nextFloat() * 2.0f - 1.0f;
    edgeNoise.resize (raw.size());
    for (size_t i = 0; i < raw.size(); ++i)
        edgeNoise[i] = 0.6f * raw[i] + 0.2f * raw[(i + 1) % raw.size()] + 0.2f * raw[(i + raw.size() - 1) % raw.size()];
    for (int i = 0; i < 40; ++i)
    {
        const float a = rng.nextFloat() * juce::MathConstants<float>::twoPi;
        const float reach = rng.nextFloat();
        const float dist = kSplashRadius * (1.05f + 0.4f * reach);
        const float size = 1.5f + 4.5f * rng.nextFloat() * (1.0f - 0.5f * reach);
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
    // Organic disc: a slow wobble plus fine ragged edges that get rougher as the amount rises.
    const float radius = kSplashRadius * (0.86f + 0.14f * level);
    const float rough = 0.03f + 0.06f * level;
    juce::Path blob;
    const auto n = edgeNoise.size();
    for (size_t i = 0; i <= n; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * (float) (i % n) / (float) n;
        const float wobble = 0.045f * std::sin (3.0f * a + 0.7f) + 0.03f * std::sin (5.0f * a + 2.1f)
                             + 0.02f * std::sin (9.0f * a + 4.0f);
        const float r = radius * (1.0f + wobble + rough * edgeNoise[i % n]);
        const auto pt = c.getPointOnCircumference (r, a);
        i == 0 ? blob.startNewSubPath (pt) : blob.lineTo (pt);
    }
    blob.closeSubPath();

    // Soft coloured glow under the disc
    juce::DropShadow (accent.withAlpha (0.35f + 0.3f * level), (int) (radius * 0.35f), { 0, 4 }).drawForPath (g, blob);

    // Diagonal colour gradient: light at the top left, deep at the bottom right
    juce::ColourGradient fill (accentLight, c.x - radius, c.y - radius, accentDeep, c.x + radius, c.y + radius, false);
    fill.addColour (0.5, accent);
    g.setGradientFill (fill);
    g.fillPath (blob);

    // Frosted centre light so the knob sits in a glow
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.45f), c.x, c.y - radius * 0.15f,
                                             juce::Colours::white.withAlpha (0.0f), c.x, c.y + radius, true));
    g.fillPath (blob);

    // Icy specks breaking off the edge as the amount rises
    for (const auto& s : shards)
    {
        const float visible = juce::jlimit (0.0f, 1.0f, (level * 1.3f - s.reach * 0.9f) * 2.0f);
        if (visible <= 0.0f)
            continue;
        auto shape = s.shape;
        shape.applyTransform (juce::AffineTransform::scale (1.0f + 0.1f * level * s.reach).translated (c));
        g.setColour (accent.interpolatedWith (accentLight, s.reach).withAlpha (s.alpha * visible * 0.8f));
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

    const auto amountArea = amount.slider.getBounds().toFloat();
    drawSplash (g, { amountArea.getCentreX(), amountArea.getY() + amountArea.getWidth() * 0.5f }, level);

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
    // Width sets the knob size; the extra height is room for the knob body and shadow
    amount.slider.setBounds (centreX - 68, 64, 136, 152);
    grain.slider.setBounds (centreX - 70 - 27, 236, 54, 72);
    fx.slider.setBounds (centreX + 70 - 27, 236, 54, 72);

    for (auto* knob : { &amount, &grain, &fx })
        knob->label.setBounds (juce::Rectangle<int> (110, 20).withCentre ({ knob->slider.getBounds().getCentreX(),
                                                                           knob->slider.getBottom() + 2 }));
}
