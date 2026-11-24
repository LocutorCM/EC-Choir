#include "PluginProcessor.h"
#include "PluginEditor.h"

static const int   kDefDeg[6]  = { 2, 4, -2, -4, 7, -7 };
static const bool  kDefOn[6]   = { true, true, true, true, false, false };
static const float kDefPan[6]  = { -0.6f, 0.6f, -0.3f, 0.3f, -0.9f, 0.9f };
static const float kDefLvl[6]  = { -6.0f, -6.0f, -7.0f, -7.0f, -9.0f, -9.0f };
static const float kExtraMs[6] = { 8.0f, 15.0f, 23.0f, 11.0f, 19.0f, 27.0f };
static const float kLfoHz[6]   = { 0.37f, 0.53f, 0.71f, 0.43f, 0.61f, 0.47f };

juce::AudioProcessorValueTreeState::ParameterLayout ChoirProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto dbText = [] (float v, int) { return v <= -59.9f ? String ("-inf") : String (v, 1) + " dB"; };
    auto pctText = [] (float v, int) { return String ((int) std::round (v)) + " %"; };

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "key", 1 }, "Key", choir::keyNames(), 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "scale", 1 }, "Scale", choir::scaleNames(), 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "source", 1 }, "Source",
                 StringArray { "Live Input", "Audio File" }, 0));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "play", 1 }, "Play", false));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "loop", 1 }, "Loop", true));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "gate", 1 }, "Gate",
                 NormalisableRange<float> (-80.0f, -20.0f, 0.1f), -50.0f,
                 AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 0) + " dB"; })));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "lead", 1 }, "Lead",
                 NormalisableRange<float> (-60.0f, 6.0f, 0.1f), 0.0f,
                 AudioParameterFloatAttributes().withStringFromValueFunction (dbText)));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "choir", 1 }, "Choir",
                 NormalisableRange<float> (-60.0f, 6.0f, 0.1f), 0.0f,
                 AudioParameterFloatAttributes().withStringFromValueFunction (dbText)));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "human", 1 }, "Humanize",
                 NormalisableRange<float> (0.0f, 100.0f, 1.0f), 40.0f,
                 AudioParameterFloatAttributes().withStringFromValueFunction (pctText)));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "space", 1 }, "Space",
                 NormalisableRange<float> (0.0f, 100.0f, 1.0f), 30.0f,
                 AudioParameterFloatAttributes().withStringFromValueFunction (pctText)));

    for (int v = 0; v < choir::numVoices; ++v)
    {
        const String nm = "Voice " + String (v + 1) + " ";
        p.push_back (std::make_unique<AudioParameterBool> (ParameterID { choir::pid (v, "on"), 1 }, nm + "On", kDefOn[v]));
        p.push_back (std::make_unique<AudioParameterInt> (ParameterID { choir::pid (v, "deg"), 1 }, nm + "Interval",
                     -14, 14, kDefDeg[v],
                     AudioParameterIntAttributes().withStringFromValueFunction ([] (int d, int) { return choir::degreeText (d); })));
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { choir::pid (v, "lvl"), 1 }, nm + "Level",
                     NormalisableRange<float> (-60.0f, 6.0f, 0.1f), kDefLvl[v],
                     AudioParameterFloatAttributes().withStringFromValueFunction (dbText)));
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { choir::pid (v, "pan"), 1 }, nm + "Pan",
                     NormalisableRange<float> (-1.0f, 1.0f, 0.01f), kDefPan[v],
                     AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int) { return choir::panText (x); })));
    }

    return { std::make_move_iterator (p.begin()), std::make_move_iterator (p.end()) };
}

ChoirProcessor::ChoirProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    keyP    = apvts.getRawParameterValue ("key");
    scaleP  = apvts.getRawParameterValue ("scale");
    sourceP = apvts.getRawParameterValue ("source");
    playP   = apvts.getRawParameterValue ("play");
    loopP   = apvts.getRawParameterValue ("loop");
    gateP   = apvts.getRawParameterValue ("gate");
    leadP   = apvts.getRawParameterValue ("lead");
    choirP  = apvts.getRawParameterValue ("choir");
    humanP  = apvts.getRawParameterValue ("human");
    spaceP  = apvts.getRawParameterValue ("space");
    for (int v = 0; v < choir::numVoices; ++v)
    {
        vp[v].on  = apvts.getRawParameterValue (choir::pid (v, "on"));
        vp[v].deg = apvts.getRawParameterValue (choir::pid (v, "deg"));
        vp[v].lvl = apvts.getRawParameterValue (choir::pid (v, "lvl"));
        vp[v].pan = apvts.getRawParameterValue (choir::pid (v, "pan"));
    }
    formats.registerBasicFormats();
}

bool ChoirProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
           && (l.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
               || l.getMainInputChannelSet() == juce::AudioChannelSet::mono());
}

void ChoirProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    winLen = 0.04f * (float) sr;
    dryDelay = (int) std::lround (winLen * 0.5f) + 1;
    setLatencySamples (dryDelay);

    const int shSize = (int) (winLen + 0.03f * (float) sr + 16.0f);
    for (auto& s : shifters) s.prepare (shSize);

    dryL.assign ((size_t) dryDelay + 2, 0.0f);
    dryR.assign ((size_t) dryDelay + 2, 0.0f);
    dryW = 0;

    decim = juce::jmax (1, (int) std::lround (sr / 12000.0));
    decCount = hopCount = 0;
    decAcc = 0.0f;
    yin.reset();

    hasNote = false;
    voicedTarget = voicedGain = 0.0f;
    unvoicedCount = 0;
    for (int v = 0; v < choir::numVoices; ++v)
    {
        targetShift[v] = curShift[v] = 0.0f;
        lvlSm[v] = 0.0f; gLs[v] = gRs[v] = 0.0f;
    }
    leadSm = choirSm = 0.0f;

    choirBuf.setSize (2, juce::jmax (samplesPerBlock, 512));
    reverb.setSampleRate (sr);
    reverb.reset();

    if (lastFile.existsAsFile() && std::abs (fileSr - sr) > 1.0)
        loadFile (lastFile);
}

int ChoirProcessor::midiOfIndex (int n) const
{
    const int* iv = choir::scaleIntervals (curScale);
    const int o = choir::floorDiv7 (n);
    return 12 * o + curKey + iv[n - 7 * o];
}

int ChoirProcessor::indexNearest (float m) const
{
    const int* iv = choir::scaleIntervals (curScale);
    const int o0 = (int) std::floor ((m - (float) curKey) / 12.0f);
    int best = 0;
    float bestD = 1.0e9f;
    for (int o = o0 - 1; o <= o0 + 1; ++o)
        for (int d = 0; d < 7; ++d)
        {
            const float diff = std::abs ((float) (12 * o + curKey + iv[d]) - m);
            if (diff < bestD) { bestD = diff; best = 7 * o + d; }
        }
    return best;
}

void ChoirProcessor::analyse (double fsD, float gateLin)
{
    const float f = yin.detect (fsD, gateLin);

    if (f > 70.0f && f < 1100.0f)
    {
        const float m = 69.0f + 12.0f * std::log2 (f / 440.0f);
        const int cand = indexNearest (m);
        if (! hasNote) { curIdx = cand; hasNote = true; }
        else if (cand != curIdx)
        {
            const float dc   = std::abs (m - (float) midiOfIndex (cand));
            const float dcur = std::abs (m - (float) midiOfIndex (curIdx));
            if (dc < dcur - 0.25f) curIdx = cand;
        }
        voicedTarget = 1.0f;
        unvoicedCount = 0;
        uiHz = f;
    }
    else if (++unvoicedCount > 4)
        voicedTarget = 0.0f;

    if (hasNote)
    {
        const int base = midiOfIndex (curIdx);
        for (int v = 0; v < choir::numVoices; ++v)
        {
            targetShift[v] = (float) (midiOfIndex (curIdx + voiceDeg[v]) - base);
            if (voicedGain < 0.05f) curShift[v] = targetShift[v];
        }
    }
    uiNoteMidi = (hasNote && voicedTarget > 0.5f) ? midiOfIndex (curIdx) : -1;
}

void ChoirProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2) return;
    if (getTotalNumInputChannels() == 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, n);

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);

    // ---- parámetros ----
    curKey   = (int) keyP->load();
    curScale = (int) scaleP->load();
    const int  source  = (int) sourceP->load();
    const bool playing = playP->load() > 0.5f;
    const bool loop    = loopP->load() > 0.5f;
    const float gateLin = juce::Decibels::decibelsToGain (gateP->load());
    auto dbToLin = [] (float db) { return db <= -59.9f ? 0.0f : juce::Decibels::decibelsToGain (db); };
    const float leadT  = dbToLin (leadP->load());
    const float choirT = dbToLin (choirP->load());
    const float human  = humanP->load() * 0.01f;
    const float space  = spaceP->load() * 0.01f;

    bool vOn[choir::numVoices];
    float lvlT[choir::numVoices], gLT[choir::numVoices], gRT[choir::numVoices], extra[choir::numVoices];
    for (int v = 0; v < choir::numVoices; ++v)
    {
        vOn[v] = vp[v].on->load() > 0.5f;
        voiceDeg[v] = (int) std::lround (vp[v].deg->load());
        lvlT[v] = dbToLin (vp[v].lvl->load());
        const float pan = juce::jlimit (-1.0f, 1.0f, vp[v].pan->load());
        const float ang = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        gLT[v] = std::cos (ang);
        gRT[v] = std::sin (ang);
        extra[v] = human * kExtraMs[v] * 0.001f * (float) sr;
    }

    if (choirBuf.getNumSamples() < n) choirBuf.setSize (2, n, false, false, true);
    float* cL = choirBuf.getWritePointer (0);
    float* cR = choirBuf.getWritePointer (1);

    juce::SpinLock::ScopedTryLockType fl (fileLock);
    const bool haveFile = fl.isLocked() && fileLen > 0 && source == 1;
    const float* fileData = haveFile ? fileBuf.getReadPointer (0) : nullptr;
    if (source == 1 && ! playing) filePos = 0;

    const float cLevel = 1.0f - std::exp (-1.0f / (0.01f * (float) sr));
    const float cGlide = 1.0f - std::exp (-1.0f / (0.02f * (float) sr));
    const float cVoice = 1.0f - std::exp (-1.0f / (0.008f * (float) sr));
    const double fsD = sr / (double) decim;
    const int dryBufSize = (int) dryL.size();

    for (int i = 0; i < n; ++i)
    {
        float inL = L[i], inR = R[i];
        if (source == 1)
        {
            float s = 0.0f;
            if (playing && haveFile && filePos < fileLen)
            {
                s = fileData[filePos++];
                if (filePos >= fileLen && loop) filePos = 0;
            }
            inL = inR = s;
        }
        const float mono = 0.5f * (inL + inR);

        // detección de altura
        decAcc += mono;
        if (++decCount >= decim)
        {
            yin.push (decAcc / (float) decim);
            decAcc = 0.0f;
            decCount = 0;
            if (++hopCount >= 128)
            {
                hopCount = 0;
                analyse (fsD, gateLin);
            }
        }

        // voz principal retardada (alineada con el coro)
        dryL[(size_t) dryW] = inL;
        dryR[(size_t) dryW] = inR;
        int rp = dryW - dryDelay;
        if (rp < 0) rp += dryBufSize;
        const float dL = dryL[(size_t) rp], dR = dryR[(size_t) rp];
        if (++dryW >= dryBufSize) dryW = 0;

        voicedGain += (voicedTarget - voicedGain) * cVoice;
        leadSm  += (leadT  - leadSm)  * cLevel;
        choirSm += (choirT - choirSm) * cLevel;

        float bl = 0.0f, br = 0.0f;
        for (int v = 0; v < choir::numVoices; ++v)
        {
            lvlSm[v] += ((vOn[v] ? lvlT[v] : 0.0f) - lvlSm[v]) * cLevel;
            gLs[v]   += (gLT[v] - gLs[v]) * cLevel;
            gRs[v]   += (gRT[v] - gRs[v]) * cLevel;
            curShift[v] += (targetShift[v] - curShift[v]) * cGlide;

            if (! vOn[v] && lvlSm[v] < 1.0e-4f) continue;

            lfoPh[v] += kLfoHz[v] / (float) sr;
            if (lfoPh[v] > 1.0f) lfoPh[v] -= 1.0f;
            const float drift = human * 0.15f * std::sin (juce::MathConstants<float>::twoPi * lfoPh[v]);
            const float ratio = std::exp2 ((curShift[v] + drift) / 12.0f);

            const float y = shifters[v].process (mono, ratio, winLen, extra[v]) * lvlSm[v] * voicedGain;
            bl += y * gLs[v];
            br += y * gRs[v];
        }

        cL[i] = bl * choirSm;
        cR[i] = br * choirSm;
        L[i] = dL * leadSm;
        R[i] = dR * leadSm;
    }

    juce::Reverb::Parameters rp;
    rp.roomSize = 0.78f;
    rp.damping = 0.45f;
    rp.wetLevel = space * 0.3f;
    rp.dryLevel = 1.0f;
    rp.width = 1.0f;
    rp.freezeMode = 0.0f;
    reverb.setParameters (rp);
    reverb.processStereo (cL, cR, n);

    buffer.addFrom (0, 0, choirBuf, 0, 0, n);
    buffer.addFrom (1, 0, choirBuf, 1, 0, n);
}

void ChoirProcessor::loadFile (const juce::File& f)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (f));
    if (reader == nullptr || reader->lengthInSamples <= 0) return;
    if (reader->lengthInSamples > (juce::int64) (reader->sampleRate * 60.0 * 10.0)) return;   // máx. 10 min

    const int len = (int) reader->lengthInSamples;
    const int chs = (int) reader->numChannels;
    juce::AudioBuffer<float> tmp (chs, len);
    reader->read (&tmp, 0, len, 0, true, true);

    juce::AudioBuffer<float> mono (1, len);
    mono.clear();
    for (int c = 0; c < chs; ++c)
        mono.addFrom (0, 0, tmp, c, 0, len, 1.0f / (float) chs);

    const double ratio = reader->sampleRate / sr;
    const int outLen = juce::jmax (1, (int) ((double) len / ratio));
    juce::AudioBuffer<float> res (1, outLen);
    juce::LagrangeInterpolator interp;
    interp.process (ratio, mono.getReadPointer (0), res.getWritePointer (0), outLen);

    {
        const juce::SpinLock::ScopedLockType sl (fileLock);
        fileBuf = std::move (res);
        fileLen = outLen;
        filePos = 0;
    }
    fileSr = sr;
    lastFile = f;
    fileName = f.getFileName();
    apvts.state.setProperty ("filePath", f.getFullPathName(), nullptr);
}

juce::AudioProcessorEditor* ChoirProcessor::createEditor()
{
    return new ChoirEditor (*this);
}

void ChoirProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void ChoirProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            const juce::File f (apvts.state.getProperty ("filePath").toString());
            if (f.existsAsFile()) loadFile (f);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChoirProcessor();
}
