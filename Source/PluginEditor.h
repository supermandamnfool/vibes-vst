#pragma once

#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"

//==============================================================================
// Vibes look: white printed-panel face, heavy capitals, grey arcs, dotted patch
// lines and three colours that always mean the same thing.
namespace vibes
{
namespace col
{
    inline juce::Colour ink()      { return juce::Colour (0xff231f20); }
    inline juce::Colour graphite() { return juce::Colour (0xff656263); }
    inline juce::Colour silver()   { return juce::Colour (0xffbdbcbc); }
    inline juce::Colour mist()     { return juce::Colour (0xffdeddde); }
    inline juce::Colour panel()    { return juce::Colour (0xffffffff); }
    inline juce::Colour teal()     { return juce::Colour (0xff03a3ac); }   // feedback
    inline juce::Colour amber()    { return juce::Colour (0xffe6a012); }   // dry / wet, "on"
    inline juce::Colour magenta()  { return juce::Colour (0xffdb285d); }   // reverb
    inline juce::Colour sky()      { return juce::Colour (0xff35a1c9); }   // media bars
}

struct Fonts
{
    static juce::Font barlow (float size, int weight = 500, float kerning = 0.0f);   // weight 500 / 600 / 700
    static juce::Font condensed (float size, float kerning = 0.0f);                  // Barlow Condensed Bold
    static juce::Font mono (float size);                                             // IBM Plex Mono Medium
    static float textWidth (const juce::Font&, const juce::String&);
};

enum class Icon { Random, Cv, Feedback, DryWet, Reverb };
void drawIcon (juce::Graphics&, Icon, juce::Rectangle<float> area, bool onColourFill = false);

enum KnobKind { KnobLarge = 0, KnobMix = 1, KnobSmall = 2, KnobDark = 3 };
}

class VibesLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VibesLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int bx, int by, int bw, int bh, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    void drawLabel (juce::Graphics&, juce::Label&) override;
};

// Four round buttons (off / feedback / dry-wet / reverb) bound to the assignable CV destination.
class DestinationButtons : public juce::Component
{
public:
    DestinationButtons (juce::AudioProcessorValueTreeState&, const juce::String& paramId);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    int indexAt (juce::Point<int>) const;
    juce::ParameterAttachment attachment;
    int selected = 0;
};

// Four bars (one per recording medium); the one in use is lit.
class MediaBars : public juce::Component
{
public:
    MediaBars (juce::AudioProcessorValueTreeState&, const juce::String& paramId);
    void paint (juce::Graphics&) override;

private:
    juce::ParameterAttachment attachment;
    int selected = 1;
};

class BeadsAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit BeadsAudioProcessorEditor (BeadsAudioProcessor&);
    ~BeadsAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct KnobCtl
    {
        juce::Slider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    struct PillCtl
    {
        juce::ToggleButton button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
    };
    struct ComboCtl
    {
        juce::ComboBox box;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
    };

    void setupKnob (KnobCtl&, const char* id, int kind, juce::Colour accent = vibes::col::ink(),
                    bool bipolar = false, bool notch = false);
    void setupPill (PillCtl&, const char* id, const char* text, bool cvPill = false);
    void setupCombo (ComboCtl&, const char* id);

    BeadsAudioProcessor& processor;
    VibesLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltipWindow { this, 700 };
    juce::Image logo;

    KnobCtl time, size, shape, pitch, density;
    KnobCtl timeAmt, sizeAmt, shapeAmt, pitchAmt;
    KnobCtl timeCv, sizeCv, shapeCv, pitchCv, densityCv;
    KnobCtl feedback, dryWet, reverb, assignCv, inputGain, outputGain;

    PillCtl timeCvOn, sizeCvOn, shapeCvOn, pitchCvOn, densityCvOn;
    PillCtl freeze, trim, mono, gateOut, midiPitch, synthMode;
    ComboCtl media, seedMode, seedSource, clockRate;

    DestinationButtons assignDest;
    MediaBars mediaBars;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BeadsAudioProcessorEditor)
};
