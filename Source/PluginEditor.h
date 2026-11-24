#pragma once
#include "PluginProcessor.h"

class ChoirLNF : public juce::LookAndFeel_V4
{
public:
    ChoirLNF();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool over, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
};

class ChoirEditor : public juce::AudioProcessorEditor,
                    public juce::FileDragAndDropTarget,
                    private juce::Timer
{
public:
    explicit ChoirEditor (ChoirProcessor&);
    ~ChoirEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;

private:
    struct VoiceState { bool on; int deg; float pan; };

    void timerCallback() override;
    VoiceState readVoice (int v) const;
    void setParam (const juce::String& id, float value);
    void beginGesture (const juce::String& id);
    void endGesture (const juce::String& id);
    juce::Point<float> dotPos (int v) const;
    int hitDot (juce::Point<float>) const;
    void applyPreset (int index);
    void loadAudio (const juce::File&);
    void updateFileLabel();

    ChoirProcessor& proc;
    ChoirLNF laf;

    juce::Rectangle<float> srcCard, stageCard, globalCard, plot;
    juce::Rectangle<float> strips[choir::numVoices];
    float pulse = 0.0f;
    int dragVoice = -1;

    juce::ComboBox keyBox, scaleBox, presetBox, sourceBox;
    juce::TextButton loadButton { "LOAD AUDIO" };
    juce::ToggleButton playButton { "PLAY" }, loopButton { "LOOP" };
    juce::Label fileLabel;
    juce::Slider leadS, choirS, humanS, spaceS, gateS;
    juce::ToggleButton onB[choir::numVoices];
    juce::Slider degS[choir::numVoices], lvlS[choir::numVoices], panS[choir::numVoices];

    std::unique_ptr<juce::FileChooser> chooser;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::unique_ptr<CA> keyA, scaleA, sourceA;
    std::unique_ptr<BA> playA, loopA;
    std::unique_ptr<SA> leadA, choirA, humanA, spaceA, gateA;
    std::unique_ptr<BA> onA[choir::numVoices];
    std::unique_ptr<SA> degA[choir::numVoices], lvlA[choir::numVoices], panA[choir::numVoices];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChoirEditor)
};
