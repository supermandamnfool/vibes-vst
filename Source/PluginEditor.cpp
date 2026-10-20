#include "PluginEditor.h"

#include <BinaryData.h>

using namespace vibes;

//==============================================================================
// Fonts (embedded, SIL Open Font License)
//==============================================================================
namespace
{
juce::Typeface::Ptr loadFace (const void* data, size_t size)
{
    return juce::Typeface::createSystemTypefaceFor (data, size);
}

juce::Font makeFont (const juce::Typeface::Ptr& face, float size, float kerning)
{
    auto options = juce::FontOptions (face).withPointHeight (size);
    if (kerning != 0.0f)
        options = options.withKerningFactor (kerning);
    return juce::Font (options);
}
}

juce::Font Fonts::barlow (float size, int weight, float kerning)
{
    static const auto medium   = loadFace (BinaryData::BarlowMedium_ttf,   BinaryData::BarlowMedium_ttfSize);
    static const auto semibold = loadFace (BinaryData::BarlowSemiBold_ttf, BinaryData::BarlowSemiBold_ttfSize);
    static const auto bold     = loadFace (BinaryData::BarlowBold_ttf,     BinaryData::BarlowBold_ttfSize);
    return makeFont (weight >= 700 ? bold : (weight >= 600 ? semibold : medium), size, kerning);
}

juce::Font Fonts::condensed (float size, float kerning)
{
    static const auto face = loadFace (BinaryData::BarlowCondensedBold_ttf, BinaryData::BarlowCondensedBold_ttfSize);
    return makeFont (face, size, kerning);
}

juce::Font Fonts::mono (float size)
{
    static const auto face = loadFace (BinaryData::IBMPlexMonoMedium_ttf, BinaryData::IBMPlexMonoMedium_ttfSize);
    return makeFont (face, size, 0.0f);
}

float Fonts::textWidth (const juce::Font& font, const juce::String& text)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (font, text, 0.0f, 0.0f);
    return ga.getBoundingBox (0, -1, true).getWidth();
}

//==============================================================================
// Icons, drawn from small vector paths (14 x 14 or 16 x 12 design grids)
//==============================================================================
void vibes::drawIcon (juce::Graphics& g, Icon icon, juce::Rectangle<float> area, bool onColourFill)
{
    const bool wide = icon == Icon::Random || icon == Icon::Cv;
    const float vbW = wide ? 16.0f : 14.0f, vbH = wide ? 12.0f : 14.0f;
    const float s = juce::jmin (area.getWidth() / vbW, area.getHeight() / vbH);
    const auto t = juce::AffineTransform::scale (s).translated (area.getCentreX() - vbW * s * 0.5f,
                                                                 area.getCentreY() - vbH * s * 0.5f);

    switch (icon)
    {
        case Icon::Random:
        {
            juce::Path p;
            const float pts[][2] = { { 1, 9 }, { 3, 3 }, { 5, 10 }, { 7, 2 }, { 9, 9 }, { 11, 4 }, { 13, 8 }, { 15, 6 } };
            for (int i = 0; i < 8; ++i)
                (i == 0 ? p.startNewSubPath (pts[i][0], pts[i][1]) : p.lineTo (pts[i][0], pts[i][1]));
            g.setColour (col::ink());
            g.strokePath (p, juce::PathStrokeType (1.5f * s, juce::PathStrokeType::curved, juce::PathStrokeType::butt), t);
            break;
        }
        case Icon::Cv:
        {
            juce::Path p;
            p.addTriangle (1, 11, 15, 11, 15, 2);
            g.setColour (col::ink());
            g.fillPath (p, t);
            break;
        }
        case Icon::Feedback:
        {
            const auto c = onColourFill ? col::ink() : col::teal();
            auto arc = juce::Drawable::parseSVGPath ("M11.2 7a4.2 4.2 0 1 1-1.3-3");
            juce::Path head;
            head.addTriangle (7.6f, 0.8f, 11.6f, 2.6f, 8.4f, 5.6f);
            g.setColour (c);
            g.strokePath (arc, juce::PathStrokeType (1.8f * s, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), t);
            g.fillPath (head, t);
            break;
        }
        case Icon::DryWet:
        {
            auto cloud = juce::Drawable::parseSVGPath ("M3.8 11.5a3 3 0 0 1 .3-6 4 4 0 0 1 7.4 1.2 2.4 2.4 0 0 1-.4 4.8Z");
            g.setColour (onColourFill ? col::ink() : col::amber());
            g.fillPath (cloud, t);
            break;
        }
        case Icon::Reverb:
        {
            auto cube = juce::Drawable::parseSVGPath ("M7 1 12.5 4v6L7 13 1.5 10V4Z");
            auto lines = juce::Drawable::parseSVGPath ("M7 7v6M1.5 4 7 7l5.5-3");
            g.setColour (onColourFill ? juce::Colours::white : col::magenta());
            g.fillPath (cube, t);
            g.setColour (onColourFill ? col::magenta() : juce::Colours::white);
            g.strokePath (lines, juce::PathStrokeType (1.0f * s), t);
            break;
        }
    }
}

//==============================================================================
// Look and feel
//==============================================================================
VibesLookAndFeel::VibesLookAndFeel()
{
    setColour (juce::Label::textColourId, col::ink());
    setColour (juce::Slider::textBoxTextColourId, col::ink());
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::backgroundColourId, col::panel());
    setColour (juce::ComboBox::textColourId, col::ink());
    setColour (juce::ComboBox::outlineColourId, col::ink());
    setColour (juce::ComboBox::arrowColourId, col::ink());
    setColour (juce::PopupMenu::backgroundColourId, col::panel());
    setColour (juce::PopupMenu::textColourId, col::ink());
    setColour (juce::PopupMenu::headerTextColourId, col::graphite());
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::amber());
    setColour (juce::PopupMenu::highlightedTextColourId, col::ink());
    setColour (juce::TooltipWindow::backgroundColourId, col::ink());
    setColour (juce::TooltipWindow::textColourId, juce::Colours::white);
    setColour (juce::TooltipWindow::outlineColourId, col::ink());
}

juce::Label* VibesLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (slider);
    const float size = (float) (double) slider.getProperties().getWithDefault ("valueSize", 12.0);
    l->setFont (Fonts::mono (size));
    l->setJustificationType (juce::Justification::centred);
    l->setBorderSize ({ 0, 0, 0, 0 });
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::textColourId, slider.findColour (juce::Slider::textBoxTextColourId));
    return l;
}

void VibesLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    // Knob read-outs are plain numbers: no box, no outline (unless being edited).
    if (dynamic_cast<juce::Slider*> (label.getParentComponent()) != nullptr && ! label.isBeingEdited())
    {
        g.setColour (label.findColour (juce::Label::textColourId).withMultipliedAlpha (label.isEnabled() ? 1.0f : 0.5f));
        g.setFont (label.getFont());
        g.drawText (label.getText(), label.getLocalBounds(), label.getJustificationType(), false);
        return;
    }
    LookAndFeel_V4::drawLabel (g, label);
}

void VibesLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                                         float startAngle, float endAngle, juce::Slider& slider)
{
    const auto& props = slider.getProperties();
    const int kind = (int) props.getWithDefault ("kind", (int) KnobLarge);
    const bool bipolar = (bool) props.getWithDefault ("bipolar", false);
    const bool notch = (bool) props.getWithDefault ("notch", false);
    const auto accent = juce::Colour::fromString (props.getWithDefault ("accent", col::ink().toString()).toString());

    const bool small = kind == KnobSmall || kind == KnobDark;
    const float d = (float) juce::jmin (width, height);
    const auto c = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).getCentre();

    const float trackR  = d * (small ? 23.0f / 56.0f : 45.0f / 108.0f);
    const float trackW  = d * (small ? 4.0f / 56.0f : 5.0f / 108.0f);
    const float valueW  = kind == KnobMix ? d * 6.0f / 108.0f : trackW;
    const float capR    = d * (small ? 16.0f / 56.0f : 32.0f / 108.0f);
    const float angle   = startAngle + pos * (endAngle - startAngle);
    const float mid     = 0.5f * (startAngle + endAngle);
    const bool enabled  = slider.isEnabled();

    const juce::PathStrokeType trackStroke (trackW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    const juce::PathStrokeType valueStroke (valueW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    // track
    juce::Path track;
    track.addCentredArc (c.x, c.y, trackR, trackR, 0.0f, startAngle, endAngle, true);
    g.setColour (kind == KnobDark ? col::silver().withAlpha (0.55f) : col::silver());
    g.strokePath (track, trackStroke);

    // value arc
    const float from = bipolar ? juce::jmin (mid, angle) : startAngle;
    const float to   = bipolar ? juce::jmax (mid, angle) : angle;
    if (to - from > 0.02f)
    {
        juce::Path value;
        value.addCentredArc (c.x, c.y, trackR, trackR, 0.0f, from, to, true);
        g.setColour (enabled ? (kind == KnobDark ? juce::Colours::white : accent) : accent.withAlpha (0.35f));
        g.strokePath (value, valueStroke);
    }

    // centre notch on bipolar large knobs (pitch)
    if (notch)
    {
        g.setColour (col::graphite());
        g.drawLine (c.x, c.y - d * 0.472f, c.x, c.y - d * 0.398f, juce::jmax (1.5f, d * 2.0f / 108.0f));
    }

    // cap
    g.setColour (col::ink());
    g.fillEllipse (juce::Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (c));
    if (! small)
    {
        const float ringR = d * 27.0f / 108.0f;
        g.setColour (col::graphite());
        g.drawEllipse (juce::Rectangle<float> (ringR * 2.0f, ringR * 2.0f).withCentre (c), 1.0f);
    }

    // pointer
    const float rOut = d * (small ? 14.0f / 56.0f : 26.0f / 108.0f);
    const float rIn  = d * (small ? 6.0f / 56.0f : 14.0f / 108.0f);
    const float w    = juce::jmax (2.0f, d * (small ? 2.5f / 56.0f : 3.0f / 108.0f));
    g.setColour (juce::Colours::white);
    g.drawLine (c.x + rIn * std::sin (angle), c.y - rIn * std::cos (angle),
                c.x + rOut * std::sin (angle), c.y - rOut * std::cos (angle), w);
    g.fillEllipse (juce::Rectangle<float> (w, w).withCentre ({ c.x + rIn * std::sin (angle), c.y - rIn * std::cos (angle) }));
    g.fillEllipse (juce::Rectangle<float> (w, w).withCentre ({ c.x + rOut * std::sin (angle), c.y - rOut * std::cos (angle) }));
}

void VibesLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.75f);
    const bool on = b.getToggleState();
    const bool cvPill = (bool) b.getProperties().getWithDefault ("cvPill", false);

    juce::Colour fill = col::panel(), text = col::ink();
    if (on) { fill = cvPill ? col::ink() : col::amber(); text = cvPill ? juce::Colours::white : col::ink(); }
    else if (highlighted) fill = col::mist().withAlpha (0.55f);

    g.setColour (fill);
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (col::ink());
    g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.5f);

    g.setColour (text);
    g.setFont (Fonts::barlow (10.5f, 700, 0.1f));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, false);
}

void VibesLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.75f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (col::ink());
    g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.5f);

    juce::Path caret;
    const float cx = (float) width - 16.5f, cy = (float) height * 0.5f;
    caret.addTriangle (cx - 4.5f, cy - 3.0f, cx + 4.5f, cy - 3.0f, cx, cy + 3.0f);
    g.setColour (col::ink());
    g.fillPath (caret);
}

void VibesLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (13, 1, box.getWidth() - 13 - 28, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centredLeft);
}

juce::Font VibesLookAndFeel::getComboBoxFont (juce::ComboBox&) { return Fonts::barlow (13.0f, 600); }
juce::Font VibesLookAndFeel::getPopupMenuFont()                 { return Fonts::barlow (14.0f, 600); }

//==============================================================================
// Destination buttons and media bars
//==============================================================================
DestinationButtons::DestinationButtons (juce::AudioProcessorValueTreeState& state, const juce::String& id)
    : attachment (*state.getParameter (id), [this] (float v) { selected = juce::roundToInt (v); repaint(); })
{
    attachment.sendInitialUpdate();
    setSize (84, 84);
}

int DestinationButtons::indexAt (juce::Point<int> p) const
{
    for (int i = 0; i < 4; ++i)
        if (juce::Rectangle<float> (4.0f + (float) (i % 2) * 42.0f, 4.0f + (float) (i / 2) * 42.0f, 34.0f, 34.0f)
                .expanded (4.0f).contains (p.toFloat()))
            return i;
    return -1;
}

void DestinationButtons::mouseDown (const juce::MouseEvent& e)
{
    const int i = indexAt (e.getPosition());
    if (i >= 0)
        attachment.setValueAsCompleteGesture ((float) i);
}

void DestinationButtons::paint (juce::Graphics& g)
{
    const juce::Colour dest[4] = { col::ink(), col::teal(), col::amber(), col::magenta() };
    const Icon icons[4] = { Icon::Random, Icon::Feedback, Icon::DryWet, Icon::Reverb };
    const char* names[4] = { "off", "feedback", "dry/wet", "reverb" };

    for (int i = 0; i < 4; ++i)
    {
        auto r = juce::Rectangle<float> (4.0f + (float) (i % 2) * 42.0f, 4.0f + (float) (i / 2) * 42.0f, 34.0f, 34.0f);
        const bool sel = i == selected;

        if (sel)
        {
            g.setColour (dest[i]);
            g.drawEllipse (r.expanded (2.75f), 1.5f);
            g.fillEllipse (r.reduced (1.0f));
            g.setColour (col::ink());
            g.drawEllipse (r.reduced (1.0f), 2.0f);
        }
        else
        {
            g.setColour (col::panel());
            g.fillEllipse (r.reduced (1.0f));
            g.setColour (dest[i]);
            g.drawEllipse (r.reduced (1.0f), 2.0f);
        }

        if (i == 0)
        {
            g.setColour (sel ? juce::Colours::white : col::ink());
            g.setFont (Fonts::barlow (10.0f, 700, 0.06f));
            g.drawText ("OFF", r.toNearestInt(), juce::Justification::centred, false);
        }
        else
        {
            drawIcon (g, icons[i], r.withSizeKeepingCentre (16.0f, 16.0f), sel);
        }
        juce::ignoreUnused (names);
    }
}

MediaBars::MediaBars (juce::AudioProcessorValueTreeState& state, const juce::String& id)
    : attachment (*state.getParameter (id), [this] (float v) { selected = juce::roundToInt (v); repaint(); })
{
    attachment.sendInitialUpdate();
    setInterceptsMouseClicks (false, false);
    setSize (26, 14);
}

void MediaBars::paint (juce::Graphics& g)
{
    const juce::Colour colours[4] = { col::silver(), col::sky(), col::amber(), col::magenta() };
    const float widths[4] = { 8.0f, 14.0f, 20.0f, 26.0f };
    for (int i = 0; i < 4; ++i)
    {
        g.setColour (colours[i].withAlpha (i == selected ? 1.0f : 0.45f));
        g.fillRect (0.0f, (float) i * 3.8f, widths[i], 2.5f);
    }
}

//==============================================================================
// Editor
//==============================================================================
namespace
{
constexpr int kW = 924, kH = 628;
constexpr int kMargin = 24;
inline int columnCentre (int i) { return kMargin + 82 + 178 * i; }   // five 164 px columns, 14 px apart
const char* const kColumnNames[5] = { "TIME", "SIZE", "SHAPE", "PITCH", "DENSITY" };
constexpr int kMixCentres[3] = { 80, 198, 316 };
constexpr int kControlX[4] = { 24, 134, 244, 354 };
}

BeadsAudioProcessorEditor::BeadsAudioProcessorEditor (BeadsAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p),
      assignDest (p.apvts, ParamIDs::assignDest), mediaBars (p.apvts, ParamIDs::media)
{
    setLookAndFeel (&lookAndFeel);
    logo = juce::ImageCache::getFromMemory (BinaryData::vibes_logo_png, BinaryData::vibes_logo_pngSize);

    setupKnob (time,    ParamIDs::time,    KnobLarge);
    setupKnob (size,    ParamIDs::size,    KnobLarge);
    setupKnob (shape,   ParamIDs::shape,   KnobLarge);
    setupKnob (pitch,   ParamIDs::pitch,   KnobLarge, col::ink(), true, true);
    setupKnob (density, ParamIDs::density, KnobLarge);

    setupKnob (timeAmt,  ParamIDs::timeAmt,  KnobSmall, col::ink(), true);
    setupKnob (sizeAmt,  ParamIDs::sizeAmt,  KnobSmall, col::ink(), true);
    setupKnob (shapeAmt, ParamIDs::shapeAmt, KnobSmall, col::ink(), true);
    setupKnob (pitchAmt, ParamIDs::pitchAmt, KnobSmall, col::ink(), true);

    setupKnob (timeCv,    ParamIDs::timeCv,    KnobSmall, col::ink(), true);
    setupKnob (sizeCv,    ParamIDs::sizeCv,    KnobSmall, col::ink(), true);
    setupKnob (shapeCv,   ParamIDs::shapeCv,   KnobSmall, col::ink(), true);
    setupKnob (pitchCv,   ParamIDs::pitchCv,   KnobSmall, col::ink(), true);
    setupKnob (densityCv, ParamIDs::densityCv, KnobSmall, col::ink(), true);

    setupPill (timeCvOn,    ParamIDs::timeCvOn,    "CV IN", true);
    setupPill (sizeCvOn,    ParamIDs::sizeCvOn,    "CV IN", true);
    setupPill (shapeCvOn,   ParamIDs::shapeCvOn,   "CV IN", true);
    setupPill (pitchCvOn,   ParamIDs::pitchCvOn,   "V/OCT", true);
    setupPill (densityCvOn, ParamIDs::densityCvOn, "CV IN", true);

    setupKnob (feedback, ParamIDs::feedback, KnobMix, col::teal());
    setupKnob (dryWet,   ParamIDs::dryWet,   KnobMix, col::amber());
    setupKnob (reverb,   ParamIDs::reverb,   KnobMix, col::magenta());
    setupKnob (assignCv, ParamIDs::assignCv, KnobSmall, col::ink(), true);
    setupKnob (inputGain,  ParamIDs::inputGain,  KnobDark, col::ink(), true);
    setupKnob (outputGain, ParamIDs::outputGain, KnobDark, col::ink(), true);

    setupCombo (media,      ParamIDs::media);
    setupCombo (seedMode,   ParamIDs::seedMode);
    setupCombo (seedSource, ParamIDs::seedSource);
    setupCombo (clockRate,  ParamIDs::clockRate);

    setupPill (freeze,    ParamIDs::freeze,    "FREEZE");
    setupPill (trim,      ParamIDs::trim,      "TRIM");
    setupPill (mono,      ParamIDs::mono,      "MONO IN");
    setupPill (gateOut,   ParamIDs::gateOut,   "GATE OUT");
    setupPill (midiPitch, ParamIDs::midiPitch, "MIDI PITCH");
    setupPill (synthMode, ParamIDs::synthMode, "SYNTH");

    addAndMakeVisible (assignDest);
    addAndMakeVisible (mediaBars);

    freeze.button.setTooltip ("Freeze the buffer so grains keep playing it");
    trim.button.setTooltip ("Trim the frozen buffer (turn on, then off again)");
    timeCvOn.button.setTooltip ("Off: the Amount knob sets internal randomness. On: Amount attenuverts the CV knob.");
    midiPitch.button.setTooltip ("Use the last MIDI note played as the pitch (middle C = no shift)");
    synthMode.button.setTooltip ("Wavetable synth: audio input is ignored. Feedback picks the bank, Dry/Wet blends oscillator and grains, Media sets bit depth.");
    synthMode.button.onStateChange = [this] { repaint(); };   // the Feedback label becomes BANK

    setSize (kW, kH);
}

BeadsAudioProcessorEditor::~BeadsAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void BeadsAudioProcessorEditor::setupKnob (KnobCtl& k, const char* id, int kind, juce::Colour accent, bool bipolar, bool notch)
{
    auto& s = k.slider;
    auto& props = s.getProperties();
    props.set ("kind", kind);
    props.set ("bipolar", bipolar);
    props.set ("notch", notch);
    props.set ("accent", accent.toString());
    props.set ("valueSize", kind == KnobLarge || kind == KnobMix ? 12.0 : 11.0);

    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, kind == KnobLarge || kind == KnobMix ? 84 : 56,
                       kind == KnobLarge ? 16 : 14);
    s.setColour (juce::Slider::textBoxTextColourId, kind == KnobDark ? juce::Colours::white : col::ink());
    s.setMouseDragSensitivity (220);
    addAndMakeVisible (s);

    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, id, s);
    if (auto* param = processor.apvts.getParameter (id))
        s.setDoubleClickReturnValue (true, (double) param->convertFrom0to1 (param->getDefaultValue()));
}

void BeadsAudioProcessorEditor::setupPill (PillCtl& p, const char* id, const char* text, bool cvPill)
{
    p.button.setButtonText (text);
    p.button.getProperties().set ("cvPill", cvPill);
    addAndMakeVisible (p.button);
    p.attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, id, p.button);
}

void BeadsAudioProcessorEditor::setupCombo (ComboCtl& c, const char* id)
{
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter (id)))
        c.box.addItemList (choice->choices, 1);
    addAndMakeVisible (c.box);
    c.attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processor.apvts, id, c.box);
}

//==============================================================================
static void dottedLine (juce::Graphics& g, float x, float y0, float y1)
{
    g.setColour (col::ink());
    for (float y = y0; y < y1; y += 4.0f)
        g.fillRect (x - 1.0f, y, 2.0f, 2.0f);
}

void BeadsAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::panel());

    auto text = [&] (const juce::String& s, const juce::Font& f, juce::Colour c, juce::Rectangle<int> r,
                     juce::Justification j = juce::Justification::centred)
    {
        g.setColour (c);
        g.setFont (f);
        g.drawText (s, r, j, false);
    };

    // ---- header: logo, name, tagline, ornament band
    if (logo.isValid())
    {
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (logo, juce::Rectangle<float> (24.0f, 19.5f, 27.0f, 27.0f), juce::RectanglePlacement::centred);
    }
    const auto titleFont = Fonts::condensed (38.0f, 0.08f);
    text ("VIBES", titleFont, col::ink(), { 65, 14, 200, 38 }, juce::Justification::centredLeft);
    const int taglineX = 65 + juce::roundToInt (Fonts::textWidth (titleFont, "VIBES")) + 16;
    text ("GRANULAR TEXTURE PROCESSOR", Fonts::barlow (11.0f, 600, 0.18f), col::graphite(),
          { taglineX, 14, 400, 38 }, juce::Justification::centredLeft);

    g.setColour (col::silver());
    for (float x = 0.0f; x < (float) kW; x += 14.0f)
    {
        juce::Path d;
        d.addQuadrilateral (x + 7.0f, 59.5f, x + 12.5f, 65.0f, x + 7.0f, 70.5f, x + 1.5f, 65.0f);
        g.strokePath (d, juce::PathStrokeType (1.0f));
        g.fillEllipse (x + 5.5f, 63.5f, 3.0f, 3.0f);
    }

    // ---- five main columns
    for (int i = 0; i < 5; ++i)
    {
        const int cx = columnCentre (i);
        text (kColumnNames[i], Fonts::condensed (20.0f, 0.06f), col::ink(), { cx - 82, 84, 164, 22 });
        dottedLine (g, (float) cx, 238.0f, 248.0f);

        if (i < 4)
        {
            vibes::drawIcon (g, Icon::Random, juce::Rectangle<float> ((float) cx - 44.0f, 248.0f, 16.0f, 12.0f));
            vibes::drawIcon (g, Icon::Cv,     juce::Rectangle<float> ((float) cx + 28.0f, 248.0f, 16.0f, 12.0f));
        }
        else
        {
            vibes::drawIcon (g, Icon::Cv, juce::Rectangle<float> ((float) cx - 8.0f, 248.0f, 16.0f, 12.0f));
        }
    }

    // grain-shape hints on SHAPE and the endless mark on SIZE
    {
        const float cxShape = (float) columnCentre (2), cxSize = (float) columnCentre (1);
        g.setColour (col::graphite());
        juce::Path rect, ramp;
        rect.startNewSubPath (cxShape - 49.0f, 212.0f); rect.lineTo (cxShape - 49.0f, 205.0f); rect.lineTo (cxShape - 40.0f, 205.0f); rect.lineTo (cxShape - 40.0f, 212.0f);
        ramp.startNewSubPath (cxShape + 39.0f, 212.0f); ramp.lineTo (cxShape + 48.0f, 204.0f); ramp.lineTo (cxShape + 48.0f, 212.0f);
        g.strokePath (rect, juce::PathStrokeType (1.5f));
        g.strokePath (ramp, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved));
        text (juce::String::fromUTF8 ("\xe2\x88\x9e"), Fonts::condensed (18.0f), col::graphite(),
              { (int) cxSize + 30, 198, 24, 20 });
    }

    // ---- mix / assign / levels
    g.setColour (col::mist());
    g.fillRect (kMargin, 371, kW - 2 * kMargin, 1);

    const bool bankMode = synthMode.button.getToggleState();
    const struct { Icon icon; const char* label; } mix[3] = {
        { Icon::Feedback, bankMode ? "BANK" : "FEEDBACK" }, { Icon::DryWet, "DRY / WET" }, { Icon::Reverb, "REVERB" } };
    for (int i = 0; i < 3; ++i)
    {
        const auto f = Fonts::condensed (15.0f, 0.08f);
        const float tw = Fonts::textWidth (f, mix[i].label);
        const float total = 14.0f + 6.0f + tw;
        const float x0 = (float) kMixCentres[i] - total * 0.5f;
        vibes::drawIcon (g, mix[i].icon, { x0, 388.0f, 14.0f, 14.0f });
        text (mix[i].label, f, col::ink(), { juce::roundToInt (x0 + 20.0f), 387, juce::roundToInt (tw) + 4, 17 }, juce::Justification::centredLeft);
    }

    text ("ASSIGNABLE CV", Fonts::barlow (10.0f, 600, 0.14f), col::graphite(), { 478, 398, 150, 12 });

    g.setColour (col::graphite());
    g.fillRoundedRectangle (734.0f, 394.0f, 166.0f, 104.0f, 12.0f);
    text ("IN",  Fonts::condensed (15.0f, 0.1f), juce::Colours::white, { 780 - 30, 403, 60, 17 });
    text ("OUT", Fonts::condensed (15.0f, 0.1f), juce::Colours::white, { 854 - 30, 403, 60, 17 });

    // ---- control row
    g.setColour (col::mist());
    g.fillRect (kMargin, 522, kW - 2 * kMargin, 1);

    const auto capFont = Fonts::barlow (10.0f, 600, 0.14f);
    text ("MEDIA",       capFont, col::graphite(), { kControlX[0], 534, 70, 12 }, juce::Justification::centredLeft);
    text ("SEED BUTTON", capFont, col::graphite(), { kControlX[1], 538, 100, 12 }, juce::Justification::centredLeft);
    text ("SEED INPUT",  capFont, col::graphite(), { kControlX[2], 538, 100, 12 }, juce::Justification::centredLeft);
    text ("CLOCK RATE",  capFont, col::graphite(), { kControlX[3], 538, 100, 12 }, juce::Justification::centredLeft);

    text (juce::String::fromUTF8 ("DSP: Beads firmware \xc2\xa9 \xc3\x89milie Gillet, MIT licence. Unofficial, not affiliated with Mutable Instruments."),
          Fonts::barlow (10.5f, 500), col::graphite(), { 300, 603, kW - kMargin - 300, 14 }, juce::Justification::centredRight);
}

void BeadsAudioProcessorEditor::resized()
{
    KnobCtl* big[5] = { &time, &size, &shape, &pitch, &density };
    KnobCtl* amt[4] = { &timeAmt, &sizeAmt, &shapeAmt, &pitchAmt };
    KnobCtl* cv[5]  = { &timeCv, &sizeCv, &shapeCv, &pitchCv, &densityCv };
    PillCtl* on[5]  = { &timeCvOn, &sizeCvOn, &shapeCvOn, &pitchCvOn, &densityCvOn };

    for (int i = 0; i < 5; ++i)
    {
        const int cx = columnCentre (i);
        big[i]->slider.setBounds (cx - 54, 108, 108, 124);
        if (i < 4)
        {
            amt[i]->slider.setBounds (cx - 64, 260, 56, 70);
            cv[i]->slider.setBounds (cx + 8, 260, 56, 70);
        }
        else
        {
            cv[i]->slider.setBounds (cx - 28, 260, 56, 70);
        }
        on[i]->button.setBounds (cx - 33, 335, 66, 22);
    }

    KnobCtl* mixKnobs[3] = { &feedback, &dryWet, &reverb };
    for (int i = 0; i < 3; ++i)
        mixKnobs[i]->slider.setBounds (kMixCentres[i] - 42, 405, 84, 98);

    assignCv.slider.setBounds (478, 416, 60, 74);
    assignDest.setBounds (548, 412, 84, 84);
    inputGain.slider.setBounds (752, 422, 56, 70);
    outputGain.slider.setBounds (826, 422, 56, 70);

    ComboCtl* combos[4] = { &media, &seedMode, &seedSource, &clockRate };
    for (int i = 0; i < 4; ++i)
        combos[i]->box.setBounds (kControlX[i], 552, 100, 30);
    mediaBars.setBounds (kControlX[0] + 100 - 26, 533, 26, 14);

    struct Item { PillCtl* p; int w; };
    const Item pills[6] = { { &freeze, 60 }, { &trim, 46 }, { &mono, 62 }, { &gateOut, 66 }, { &midiPitch, 84 }, { &synthMode, 56 } };
    int x = kW - kMargin - (60 + 46 + 62 + 66 + 84 + 56 + 5 * 6);
    for (auto& it : pills)
    {
        it.p->button.setBounds (x, 552, it.w, 30);
        x += it.w + 6;
    }
}
