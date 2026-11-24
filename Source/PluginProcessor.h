#pragma once
#include <JuceHeader.h>
#include "ChoirCore.h"

class ChoirProcessor : public juce::AudioProcessor
{
public:
    ChoirProcessor();
    ~ChoirProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "EC Choir"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 2.5; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Carga un archivo de audio en memoria (hilo de interfaz)
    void loadFile (const juce::File&);
    juce::String getFileName() const { return fileName; }

    juce::AudioProcessorValueTreeState apvts;

    // Datos para la interfaz
    std::atomic<int>   uiNoteMidi { -1 };
    std::atomic<float> uiHz { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // ---- Detector de altura (YIN) ----
    struct Yin
    {
        static constexpr int W = 512, MAXLAG = 200, N = W + MAXLAG;
        std::array<float, N> ring {};
        std::array<float, N> x {};
        std::array<float, MAXLAG + 2> cm {};
        int wr = 0;

        void reset() { ring.fill (0.0f); wr = 0; }
        void push (float s) { ring[(size_t) wr] = s; wr = (wr + 1) % N; }

        float detect (double fsD, float gateLin)
        {
            for (int i = 0; i < N; ++i) x[(size_t) i] = ring[(size_t) ((wr + i) % N)];

            float e = 0.0f;
            for (int i = 0; i < W; ++i) e += x[(size_t) i] * x[(size_t) i];
            if (std::sqrt (e / (float) W) < gateLin) return 0.0f;

            const int tauMax = juce::jmin (MAXLAG, (int) (fsD / 70.0));
            const int tauMin = juce::jmax (2, (int) (fsD / 1100.0));
            if (tauMax <= tauMin + 2) return 0.0f;

            float run = 0.0f;
            cm[0] = 1.0f;
            for (int tau = 1; tau <= tauMax; ++tau)
            {
                float d = 0.0f;
                for (int j = 0; j < W; ++j)
                {
                    const float df = x[(size_t) j] - x[(size_t) (j + tau)];
                    d += df * df;
                }
                run += d;
                cm[(size_t) tau] = run > 1.0e-12f ? d * (float) tau / run : 1.0f;
            }

            int tau = -1;
            for (int t = tauMin; t <= tauMax; ++t)
            {
                if (cm[(size_t) t] < 0.15f)
                {
                    while (t + 1 <= tauMax && cm[(size_t) (t + 1)] < cm[(size_t) t]) ++t;
                    tau = t;
                    break;
                }
            }
            if (tau < 0)
            {
                float mn = 1.0e9f; int mi = -1;
                for (int t = tauMin; t <= tauMax; ++t)
                    if (cm[(size_t) t] < mn) { mn = cm[(size_t) t]; mi = t; }
                if (mn < 0.3f) tau = mi; else return 0.0f;
            }

            float t0 = (float) tau;
            if (tau > 1 && tau < tauMax)
            {
                const float a = cm[(size_t) (tau - 1)], b = cm[(size_t) tau], c = cm[(size_t) (tau + 1)];
                const float den = a - 2.0f * b + c;
                if (std::abs (den) > 1.0e-9f) t0 += 0.5f * (a - c) / den;
            }
            return (float) (fsD / (double) t0);
        }
    };

    // ---- Transpositor de altura (línea de retardo con dos lecturas) ----
    struct Shifter
    {
        std::vector<float> buf;
        int size = 0, w = 0;
        float phase = 0.0f;

        void prepare (int sz) { size = sz; buf.assign ((size_t) sz, 0.0f); w = 0; phase = 0.0f; }

        float read (float delay) const
        {
            float rp = (float) w - delay;
            while (rp < 0.0f) rp += (float) size;
            const int i0 = (int) rp;
            const float fr = rp - (float) i0;
            const int i1 = (i0 + 1 >= size) ? 0 : i0 + 1;
            return buf[(size_t) i0] * (1.0f - fr) + buf[(size_t) i1] * fr;
        }

        float process (float in, float ratio, float winLen, float extra)
        {
            buf[(size_t) w] = in;
            phase += (1.0f - ratio) / winLen;
            phase -= std::floor (phase);
            float p2 = phase + 0.5f;
            p2 -= std::floor (p2);

            const float d1 = phase * winLen + extra + 1.0f;
            const float d2 = p2 * winLen + extra + 1.0f;
            const float g1 = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * phase);
            const float g2 = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * p2);
            const float out = read (d1) * g1 + read (d2) * g2;

            if (++w >= size) w = 0;
            return out;
        }
    };

    int midiOfIndex (int n) const;
    int indexNearest (float midi) const;
    void analyse (double fsD, float gateLin);

    struct VoicePtrs { std::atomic<float> *on = nullptr, *deg = nullptr, *lvl = nullptr, *pan = nullptr; };
    VoicePtrs vp[choir::numVoices];
    std::atomic<float> *keyP = nullptr, *scaleP = nullptr, *sourceP = nullptr, *playP = nullptr, *loopP = nullptr,
                       *gateP = nullptr, *leadP = nullptr, *choirP = nullptr, *humanP = nullptr, *spaceP = nullptr;

    double sr = 44100.0;
    float winLen = 1764.0f;
    int dryDelay = 0;
    std::vector<float> dryL, dryR;
    int dryW = 0;

    Shifter shifters[choir::numVoices];
    Yin yin;
    int decim = 4, decCount = 0, hopCount = 0;
    float decAcc = 0.0f;

    // seguimiento de nota
    int curKey = 0, curScale = 0;
    int voiceDeg[choir::numVoices] = {};
    bool hasNote = false;
    int curIdx = 0;
    int unvoicedCount = 0;
    float voicedTarget = 0.0f, voicedGain = 0.0f;
    float targetShift[choir::numVoices] = {};
    float curShift[choir::numVoices] = {};
    float lfoPh[choir::numVoices] = {};
    float lvlSm[choir::numVoices] = {};
    float gLs[choir::numVoices] = {};
    float gRs[choir::numVoices] = {};
    float leadSm = 1.0f, choirSm = 1.0f;

    juce::AudioBuffer<float> choirBuf;
    juce::Reverb reverb;

    // archivo
    juce::AudioFormatManager formats;
    juce::SpinLock fileLock;
    juce::AudioBuffer<float> fileBuf;
    int fileLen = 0, filePos = 0;
    juce::String fileName;
    juce::File lastFile;
    double fileSr = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChoirProcessor)
};
