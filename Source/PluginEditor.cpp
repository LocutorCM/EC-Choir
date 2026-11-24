#include "PluginEditor.h"
using namespace juce;

static const Colour kInk   (0xff1b3a66);
static const Colour kMute  (0xff6c86ad);
static const Colour kBlue  (0xff2f7bff);
static const Colour kCyan  (0xff00b4ff);

static const Colour kVoice[choir::numVoices] = {
    Colour (0xff2f7bff), Colour (0xff00b4ff), Colour (0xff6c63ff),
    Colour (0xff00c9c0), Colour (0xff4f9dff), Colour (0xff8a7dff)
};

struct Preset { const char* name; int deg[6]; int on[6]; float pan[6]; float lvl[6]; };
static const Preset kPresets[] = {
    { "Gospel Triad", { 2, 4, -2, -4, 7, -7 }, { 1, 1, 1, 1, 0, 0 },
      { -0.6f, 0.6f, -0.3f, 0.3f, -0.9f, 0.9f }, { -6, -6, -7, -7, -9, -9 } },
    { "Full Choir",   { 2, 4, -2, -4, 7, -7 }, { 1, 1, 1, 1, 1, 1 },
      { -0.8f, 0.8f, -0.4f, 0.4f, -0.15f, 0.15f }, { -5, -5, -6, -6, -8, -8 } },
    { "Tight Duo",    { 2, -2, 4, -4, 7, -7 }, { 1, 1, 0, 0, 0, 0 },
      { -0.5f, 0.5f, -0.3f, 0.3f, 0.0f, 0.0f }, { -5, -5, -6, -6, -8, -8 } },
};

// ------------------------------------------------------------ Look & Feel
ChoirLNF::ChoirLNF()
{
    setColour (ComboBox::backgroundColourId, Colour (0xeeffffff));
    setColour (ComboBox::textColourId, kInk);
    setColour (ComboBox::outlineColourId, Colour (0xff9cc4f5));
    setColour (ComboBox::arrowColourId, kBlue);
    setColour (PopupMenu::backgroundColourId, Colour (0xfff4f9ff));
    setColour (PopupMenu::textColourId, kInk);
    setColour (PopupMenu::highlightedBackgroundColourId, kBlue);
    setColour (PopupMenu::highlightedTextColourId, Colours::white);
    setColour (Slider::textBoxTextColourId, kInk);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Slider::trackColourId, kBlue);
    setColour (Slider::backgroundColourId, Colour (0x33406a9a));
    setColour (Slider::thumbColourId, kBlue);
    setColour (Label::textColourId, kInk);
}

void ChoirLNF::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                 float startA, float endA, Slider&)
{
    auto area = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float d = jmin (area.getWidth(), area.getHeight());
    auto b = area.withSizeKeepingCentre (d, d);
    const auto c = b.getCentre();
    const float r = d * 0.5f;

    Path disc;
    disc.addEllipse (b.reduced (d * 0.1f));
    DropShadow (Colour (0x442f5fa8), (int) (d * 0.16f), { 0, (int) (d * 0.06f) }).drawForPath (g, disc);
    g.setGradientFill (ColourGradient (Colours::white, b.getX(), b.getY(), Colour (0xffdbe9fb), b.getRight(), b.getBottom(), false));
    g.fillPath (disc);

    const float ar = r - 3.0f;
    const float ang = startA + pos * (endA - startA);

    Path track;
    track.addCentredArc (c.x, c.y, ar, ar, 0.0f, startA, endA, true);
    g.setColour (Colour (0x332f5fa8));
    g.strokePath (track, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));

    if (pos > 0.003f)
    {
        Path val;
        val.addCentredArc (c.x, c.y, ar, ar, 0.0f, startA, ang, true);
        for (int i = 3; i >= 1; --i)
        {
            g.setColour (kCyan.withAlpha (0.08f * (float) (4 - i)));
            g.strokePath (val, PathStrokeType (3.0f + (float) i * 3.0f, PathStrokeType::curved, PathStrokeType::rounded));
        }
        g.setGradientFill (ColourGradient (kBlue, b.getX(), b.getBottom(), kCyan, b.getRight(), b.getY(), false));
        g.strokePath (val, PathStrokeType (3.2f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    const Point<float> dir (std::sin (ang), -std::cos (ang));
    g.setColour (kBlue);
    g.drawLine (Line<float> (c + dir * (ar * 0.30f), c + dir * (ar * 0.62f)), 2.4f);
}

void ChoirLNF::drawToggleButton (Graphics& g, ToggleButton& b, bool, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (2.0f);
    const bool on = b.getToggleState();
    Path p;
    p.addRoundedRectangle (r, r.getHeight() * 0.5f);

    if (on)
    {
        DropShadow (Colour (0x882f9bff), 10, { 0, 0 }).drawForPath (g, p);
        g.setGradientFill (ColourGradient (Colour (0xff4aa8ff), r.getX(), r.getY(), Colour (0xff2f6bff), r.getX(), r.getBottom(), false));
    }
    else
        g.setColour (Colour (0xccffffff));
    g.fillPath (p);

    g.setColour (on ? Colours::white.withAlpha (0.7f) : Colour (0xff9cc4f5));
    g.strokePath (p, PathStrokeType (1.2f));
    g.setColour (on ? Colours::white : Colour (0xff4a6a9a));
    g.setFont (12.5f);
    g.drawText (b.getButtonText(), r, Justification::centred);
}

void ChoirLNF::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.5f);
    Path p;
    p.addRoundedRectangle (r, 9.0f);
    g.setGradientFill (ColourGradient (down ? Colour (0xffd5e6ff) : Colour (0xfff7fbff), r.getX(), r.getY(),
                                       over ? Colour (0xffc5dcff) : Colour (0xffdbe9fb), r.getX(), r.getBottom(), false));
    g.fillPath (p);
    g.setColour (Colour (0xff7fb0f5));
    g.strokePath (p, PathStrokeType (1.2f));
}

// ------------------------------------------------------------ Editor
ChoirEditor::ChoirEditor (ChoirProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setSize (960, 610);
    setLookAndFeel (&laf);

    keyBox.addItemList (choir::keyNames(), 1);
    scaleBox.addItemList (choir::scaleNames(), 1);
    sourceBox.addItemList (StringArray { "Live Input", "Audio File" }, 1);
    for (size_t i = 0; i < std::size (kPresets); ++i)
        presetBox.addItem (kPresets[i].name, (int) i + 1);
    presetBox.setTextWhenNothingSelected ("Choose...");
    presetBox.onChange = [this] { applyPreset (presetBox.getSelectedId() - 1); };

    for (auto* c : { &keyBox, &scaleBox, &presetBox, &sourceBox })
        addAndMakeVisible (*c);
    addAndMakeVisible (loadButton);
    addAndMakeVisible (playButton);
    addAndMakeVisible (loopButton);
    addAndMakeVisible (fileLabel);

    fileLabel.setJustificationType (Justification::centredLeft);
    fileLabel.setFont (Font (FontOptions (12.0f)));
    fileLabel.setColour (Label::textColourId, kMute);
    loadButton.setColour (TextButton::textColourOffId, kBlue);
    loadButton.onClick = [this]
    {
        chooser = std::make_unique<FileChooser> ("Select an audio file", File(), "*.wav;*.aif;*.aiff;*.mp3;*.flac;*.m4a;*.caf");
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                              [this] (const FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f.existsAsFile()) loadAudio (f);
                              });
    };

    auto setupKnob = [this] (Slider& s, double def)
    {
        s.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (Slider::TextBoxBelow, false, 80, 16);
        s.setDoubleClickReturnValue (true, def);
        addAndMakeVisible (s);
    };
    setupKnob (leadS, 0.0);
    setupKnob (choirS, 0.0);
    setupKnob (humanS, 40.0);
    setupKnob (spaceS, 30.0);
    setupKnob (gateS, -50.0);

    static const float defPan[6] = { -0.6f, 0.6f, -0.3f, 0.3f, -0.9f, 0.9f };
    static const int defDeg[6] = { 2, 4, -2, -4, 7, -7 };
    for (int v = 0; v < choir::numVoices; ++v)
    {
        onB[v].setButtonText ("ON");
        addAndMakeVisible (onB[v]);

        degS[v].setSliderStyle (Slider::LinearHorizontal);
        degS[v].setTextBoxStyle (Slider::TextBoxBelow, false, 110, 18);
        degS[v].setDoubleClickReturnValue (true, (double) defDeg[v]);
        degS[v].setColour (Slider::trackColourId, kVoice[v]);
        degS[v].setColour (Slider::thumbColourId, kVoice[v]);
        addAndMakeVisible (degS[v]);

        setupKnob (lvlS[v], -6.0);
        setupKnob (panS[v], (double) defPan[v]);
        panS[v].setDoubleClickReturnValue (true, 0.0);
    }

    auto& a = proc.apvts;
    keyA    = std::make_unique<CA> (a, "key", keyBox);
    scaleA  = std::make_unique<CA> (a, "scale", scaleBox);
    sourceA = std::make_unique<CA> (a, "source", sourceBox);
    playA   = std::make_unique<BA> (a, "play", playButton);
    loopA   = std::make_unique<BA> (a, "loop", loopButton);
    leadA   = std::make_unique<SA> (a, "lead", leadS);
    choirA  = std::make_unique<SA> (a, "choir", choirS);
    humanA  = std::make_unique<SA> (a, "human", humanS);
    spaceA  = std::make_unique<SA> (a, "space", spaceS);
    gateA   = std::make_unique<SA> (a, "gate", gateS);
    for (int v = 0; v < choir::numVoices; ++v)
    {
        onA[v]  = std::make_unique<BA> (a, choir::pid (v, "on"), onB[v]);
        degA[v] = std::make_unique<SA> (a, choir::pid (v, "deg"), degS[v]);
        lvlA[v] = std::make_unique<SA> (a, choir::pid (v, "lvl"), lvlS[v]);
        panA[v] = std::make_unique<SA> (a, choir::pid (v, "pan"), panS[v]);
    }

    updateFileLabel();
    startTimerHz (30);
}

ChoirEditor::~ChoirEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void ChoirEditor::updateFileLabel()
{
    const auto n = proc.getFileName();
    fileLabel.setText (n.isEmpty() ? "No file loaded (or drop one here)" : n, dontSendNotification);
}

void ChoirEditor::loadAudio (const File& f)
{
    proc.loadFile (f);
    setParam ("source", 1.0f);
    updateFileLabel();
}

bool ChoirEditor::isInterestedInFileDrag (const StringArray& files)
{
    if (files.isEmpty()) return false;
    const auto ext = File (files[0]).getFileExtension().toLowerCase();
    return ext == ".wav" || ext == ".aif" || ext == ".aiff" || ext == ".mp3" || ext == ".flac" || ext == ".m4a" || ext == ".caf";
}

void ChoirEditor::filesDropped (const StringArray& files, int, int)
{
    if (files.size() > 0) loadAudio (File (files[0]));
}

ChoirEditor::VoiceState ChoirEditor::readVoice (int v) const
{
    VoiceState s;
    s.on  = proc.apvts.getRawParameterValue (choir::pid (v, "on"))->load() > 0.5f;
    s.deg = (int) std::lround (proc.apvts.getRawParameterValue (choir::pid (v, "deg"))->load());
    s.pan = proc.apvts.getRawParameterValue (choir::pid (v, "pan"))->load();
    return s;
}

void ChoirEditor::setParam (const String& id, float value)
{
    if (auto* p = proc.apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}
void ChoirEditor::beginGesture (const String& id)
{
    if (auto* p = proc.apvts.getParameter (id)) p->beginChangeGesture();
}
void ChoirEditor::endGesture (const String& id)
{
    if (auto* p = proc.apvts.getParameter (id)) p->endChangeGesture();
}

void ChoirEditor::applyPreset (int idx)
{
    if (idx < 0 || idx >= (int) std::size (kPresets)) return;
    const auto& pr = kPresets[idx];
    for (int v = 0; v < choir::numVoices; ++v)
    {
        setParam (choir::pid (v, "on"),  (float) pr.on[v]);
        setParam (choir::pid (v, "deg"), (float) pr.deg[v]);
        setParam (choir::pid (v, "pan"), pr.pan[v]);
        setParam (choir::pid (v, "lvl"), pr.lvl[v]);
    }
}

Point<float> ChoirEditor::dotPos (int v) const
{
    const auto s = readVoice (v);
    return { plot.getX() + (s.pan + 1.0f) * 0.5f * plot.getWidth(),
             plot.getCentreY() - (float) s.deg * (plot.getHeight() * 0.5f / 14.0f) };
}

int ChoirEditor::hitDot (Point<float> pt) const
{
    int best = -1;
    float bestD = 18.0f;
    for (int v = 0; v < choir::numVoices; ++v)
    {
        if (! readVoice (v).on) continue;
        const float d = dotPos (v).getDistanceFrom (pt);
        if (d < bestD) { bestD = d; best = v; }
    }
    return best;
}

void ChoirEditor::resized()
{
    srcCard    = { 20.0f, 76.0f, 230.0f, 262.0f };
    stageCard  = { 266.0f, 76.0f, 440.0f, 262.0f };
    globalCard = { 722.0f, 76.0f, 218.0f, 262.0f };
    plot = stageCard.withTrimmedTop (62.0f).withTrimmedBottom (26.0f).reduced (26.0f, 0.0f);

    keyBox.setBounds    (470, 24, 110, 28);
    scaleBox.setBounds  (590, 24, 160, 28);
    presetBox.setBounds (760, 24, 180, 28);

    const int sx = (int) srcCard.getX() + 14, sw = (int) srcCard.getWidth() - 28, sy = (int) srcCard.getY();
    sourceBox.setBounds  (sx, sy + 38, sw, 28);
    loadButton.setBounds (sx, sy + 76, sw, 30);
    fileLabel.setBounds  (sx, sy + 110, sw, 18);
    playButton.setBounds (sx, sy + 140, 96, 32);
    loopButton.setBounds (sx + 108, sy + 140, 84, 32);

    Slider* gl[5] = { &leadS, &choirS, &humanS, &spaceS, &gateS };
    for (int i = 0; i < 5; ++i)
    {
        const int col = i % 2, row = i / 2;
        gl[i]->setBounds (730 + col * 104, 108 + row * 76, 100, 66);
    }

    for (int v = 0; v < choir::numVoices; ++v)
    {
        strips[v] = { 20.0f + (float) v * 153.0f, 354.0f, 147.0f, 236.0f };
        const int x = (int) strips[v].getX(), y = (int) strips[v].getY();
        onB[v].setBounds (x + 85, y + 8, 52, 22);
        degS[v].setBounds (x + 10, y + 46, 127, 48);
        lvlS[v].setBounds (x + 6, y + 112, 68, 112);
        panS[v].setBounds (x + 73, y + 112, 68, 112);
    }
}

void ChoirEditor::timerCallback()
{
    pulse += 0.12f;
    repaint (stageCard.toNearestInt());
}

void ChoirEditor::paint (Graphics& g)
{
    const float W = (float) getWidth(), H = (float) getHeight();

    g.setGradientFill (ColourGradient (Colour (0xfff7fbff), 0, 0, Colour (0xffd9e8fa), 0, H, false));
    g.fillAll();
    g.setGradientFill (ColourGradient (Colour (0x4400c8ff), 80.0f, 60.0f, Colour (0x0000c8ff), 520.0f, 60.0f, true));
    g.fillRect (0.0f, 0.0f, W, H);
    g.setGradientFill (ColourGradient (Colour (0x442f7bff), W - 60.0f, H - 40.0f, Colour (0x002f7bff), W - 560.0f, H - 40.0f, true));
    g.fillRect (0.0f, 0.0f, W, H);

    // cabecera
    g.setColour (kInk);
    g.setFont (30.0f);
    g.drawText ("E C   C H O I R", 28, 12, 420, 40, Justification::centredLeft);
    g.setColour (kMute);
    g.setFont (11.0f);
    g.drawText ("GOSPEL VOCAL HARMONIZER", 30, 50, 300, 14, Justification::centredLeft);
    g.drawText ("KEY", 472, 8, 60, 14, Justification::centredLeft);
    g.drawText ("SCALE", 592, 8, 60, 14, Justification::centredLeft);
    g.drawText ("PRESET", 762, 8, 60, 14, Justification::centredLeft);
    g.setColour (kBlue.withAlpha (0.35f));
    g.drawHorizontalLine (68, 28.0f, W - 20.0f);

    auto card = [&g] (Rectangle<float> r)
    {
        Path p;
        p.addRoundedRectangle (r, 16.0f);
        DropShadow (Colour (0x2a3a6aa0), 22, { 0, 8 }).drawForPath (g, p);
        g.setGradientFill (ColourGradient (Colour (0xddffffff), r.getX(), r.getY(), Colour (0xaaeaf3ff), r.getX(), r.getBottom(), false));
        g.fillPath (p);
        g.setColour (Colours::white.withAlpha (0.95f));
        g.strokePath (p, PathStrokeType (1.2f));
    };
    card (srcCard);
    card (stageCard);
    card (globalCard);
    for (auto& s : strips) card (s);

    g.setFont (11.0f);
    g.setColour (kMute);
    g.drawText ("SOURCE", (int) srcCard.getX() + 16, (int) srcCard.getY() + 12, 120, 16, Justification::centredLeft);
    g.drawText ("MIX", (int) globalCard.getX() + 16, (int) globalCard.getY() + 12, 120, 16, Justification::centredLeft);
    g.drawText ("Drop an audio file here", (int) srcCard.getX() + 16, (int) srcCard.getBottom() - 56, 200, 16, Justification::centredLeft);
    g.drawText ("or use the track input as a", (int) srcCard.getX() + 16, (int) srcCard.getBottom() - 40, 200, 16, Justification::centredLeft);
    g.drawText ("vocal effect (insert).", (int) srcCard.getX() + 16, (int) srcCard.getBottom() - 24, 200, 16, Justification::centredLeft);

    const char* gn[5] = { "LEAD", "CHOIR", "HUMANIZE", "SPACE", "GATE" };
    for (int i = 0; i < 5; ++i)
        g.drawText (gn[i], 730 + (i % 2) * 104, 96 + (i / 2) * 76, 100, 14, Justification::centred);

    // escenario
    {
        g.setColour (kMute);
        g.drawText ("CHOIR STAGE", (int) stageCard.getX() + 16, (int) stageCard.getY() + 12, 140, 16, Justification::centredLeft);

        const int note = proc.uiNoteMidi.load();
        g.setColour (kInk);
        g.setFont (28.0f);
        g.drawText (note >= 0 ? choir::noteName (note) : String ("--"), stageCard.withTrimmedTop (8.0f).withHeight (44.0f).toNearestInt(),
                    Justification::centred);

        const auto cx = plot.getCentreX(), cy = plot.getCentreY();
        g.setColour (Colour (0x182f7bff));
        for (float k : { 0.25f, 0.5f, 0.75f, 1.0f })
            g.drawEllipse (cx - plot.getWidth() * 0.5f * k, cy - plot.getHeight() * 0.5f * k,
                           plot.getWidth() * k, plot.getHeight() * k, 1.0f);
        g.setColour (Colour (0x222f7bff));
        g.drawHorizontalLine ((int) cy, plot.getX(), plot.getRight());
        g.drawVerticalLine ((int) cx, plot.getY(), plot.getBottom());
        g.setColour (kMute);
        g.setFont (11.0f);
        g.drawText ("L", (int) plot.getX() - 18, (int) cy - 7, 14, 14, Justification::centred);
        g.drawText ("R", (int) plot.getRight() + 4, (int) cy - 7, 14, 14, Justification::centred);

        // voz principal
        const float act = note >= 0 ? 1.0f : 0.0f;
        g.setColour (Colours::white);
        g.fillEllipse (cx - 9.0f, cy - 9.0f, 18.0f, 18.0f);
        g.setColour (kInk.withAlpha (0.6f + 0.4f * act));
        g.drawEllipse (cx - 9.0f, cy - 9.0f, 18.0f, 18.0f, 2.0f);
        g.setFont (9.0f);
        g.drawText ("LEAD", Rectangle<float> (cx - 20.0f, cy + 10.0f, 40.0f, 12.0f), Justification::centred);

        for (int v = 0; v < choir::numVoices; ++v)
        {
            const auto s = readVoice (v);
            const auto c = dotPos (v);
            if (! s.on)
            {
                g.setColour (Colour (0x446c86ad));
                g.drawEllipse (c.x - 6.0f, c.y - 6.0f, 12.0f, 12.0f, 1.0f);
                continue;
            }
            const float rad = 11.0f + act * 2.0f * std::sin (pulse + (float) v);
            g.setGradientFill (ColourGradient (kVoice[v].withAlpha (0.5f), c.x, c.y, kVoice[v].withAlpha (0.0f), c.x + 30.0f, c.y, true));
            g.fillEllipse (c.x - 30.0f, c.y - 30.0f, 60.0f, 60.0f);
            g.setColour (kVoice[v]);
            g.fillEllipse (c.x - rad, c.y - rad, rad * 2.0f, rad * 2.0f);
            g.setColour (Colours::white);
            g.drawEllipse (c.x - rad, c.y - rad, rad * 2.0f, rad * 2.0f, 1.6f);
            g.setFont (11.0f);
            g.drawText (String (v + 1), Rectangle<float> (c.x - rad, c.y - rad, rad * 2.0f, rad * 2.0f), Justification::centred);
        }

        g.setColour (kMute);
        g.setFont (10.5f);
        g.drawText ("Drag: pan (left/right) + interval (up/down)   |   Double-click: add / remove voice",
                    (int) stageCard.getX() + 12, (int) stageCard.getBottom() - 20, (int) stageCard.getWidth() - 24, 14,
                    Justification::centred);
    }

    // tiras de voces
    for (int v = 0; v < choir::numVoices; ++v)
    {
        const auto& s = strips[v];
        g.setColour (kVoice[v]);
        g.fillEllipse (s.getX() + 14.0f, s.getY() + 14.0f, 10.0f, 10.0f);
        g.setColour (kInk);
        g.setFont (13.0f);
        g.drawText ("VOICE " + String (v + 1), (int) s.getX() + 30, (int) s.getY() + 8, 70, 22, Justification::centredLeft);
        g.setColour (kMute);
        g.setFont (10.5f);
        g.drawText ("INTERVAL", (int) s.getX() + 14, (int) s.getY() + 32, 100, 14, Justification::centredLeft);
        g.drawText ("LEVEL", (int) s.getX() + 6, (int) s.getY() + 98, 68, 14, Justification::centred);
        g.drawText ("PAN", (int) s.getX() + 73, (int) s.getY() + 98, 68, 14, Justification::centred);
    }
}

void ChoirEditor::mouseDown (const MouseEvent& e)
{
    dragVoice = -1;
    if (! stageCard.contains (e.position)) return;
    const int hit = hitDot (e.position);
    if (hit < 0) return;
    dragVoice = hit;
    beginGesture (choir::pid (hit, "pan"));
    beginGesture (choir::pid (hit, "deg"));
}

void ChoirEditor::mouseDrag (const MouseEvent& e)
{
    if (dragVoice < 0) return;
    const float pan = jlimit (-1.0f, 1.0f, (e.position.x - plot.getX()) / plot.getWidth() * 2.0f - 1.0f);
    const float deg = jlimit (-14.0f, 14.0f, std::round ((plot.getCentreY() - e.position.y) / (plot.getHeight() * 0.5f / 14.0f)));
    setParam (choir::pid (dragVoice, "pan"), std::abs (pan) < 0.04f ? 0.0f : pan);
    setParam (choir::pid (dragVoice, "deg"), deg);
}

void ChoirEditor::mouseUp (const MouseEvent&)
{
    if (dragVoice >= 0)
    {
        endGesture (choir::pid (dragVoice, "pan"));
        endGesture (choir::pid (dragVoice, "deg"));
    }
    dragVoice = -1;
}

void ChoirEditor::mouseDoubleClick (const MouseEvent& e)
{
    if (! plot.expanded (10.0f).contains (e.position)) return;

    const int hit = hitDot (e.position);
    if (hit >= 0)
    {
        setParam (choir::pid (hit, "on"), 0.0f);
        return;
    }
    for (int v = 0; v < choir::numVoices; ++v)
    {
        if (! readVoice (v).on)
        {
            const float pan = jlimit (-1.0f, 1.0f, (e.position.x - plot.getX()) / plot.getWidth() * 2.0f - 1.0f);
            const float deg = jlimit (-14.0f, 14.0f, std::round ((plot.getCentreY() - e.position.y) / (plot.getHeight() * 0.5f / 14.0f)));
            setParam (choir::pid (v, "pan"), pan);
            setParam (choir::pid (v, "deg"), deg);
            setParam (choir::pid (v, "on"), 1.0f);
            return;
        }
    }
}
