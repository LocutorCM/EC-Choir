#pragma once
#include <JuceHeader.h>

namespace choir
{
constexpr int numVoices = 6;

inline juce::String pid (int v, const juce::String& n) { return "v" + juce::String (v) + "_" + n; }

inline const juce::StringArray& keyNames()
{
    static const juce::StringArray s { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return s;
}
inline const juce::StringArray& scaleNames()
{
    static const juce::StringArray s { "Major", "Natural Minor", "Dorian", "Mixolydian", "Harmonic Minor" };
    return s;
}
inline const int* scaleIntervals (int s)
{
    static const int t[5][7] = { { 0, 2, 4, 5, 7, 9, 11 }, { 0, 2, 3, 5, 7, 8, 10 }, { 0, 2, 3, 5, 7, 9, 10 },
                                 { 0, 2, 4, 5, 7, 9, 10 }, { 0, 2, 3, 5, 7, 8, 11 } };
    return t[juce::jlimit (0, 4, s)];
}
inline int floorDiv7 (int n) { return n >= 0 ? n / 7 : -((-n + 6) / 7); }

inline juce::String degreeText (int v)
{
    if (v == 0) return "Unison";
    const int k = std::abs (v) + 1;   // intervalo: 2 = segunda, 3 = tercera...
    juce::String name = (k == 8) ? juce::String ("Octave")
                                 : juce::String (k) + (k == 2 ? "nd" : k == 3 ? "rd" : "th");
    return name + (v > 0 ? " up" : " down");
}
inline juce::String panText (float p)
{
    if (p < -0.01f) return "L " + juce::String ((int) std::round (-p * 100.0f));
    if (p >  0.01f) return "R " + juce::String ((int) std::round ( p * 100.0f));
    return "C";
}
inline juce::String noteName (int midi)
{
    return keyNames()[((midi % 12) + 12) % 12] + juce::String (midi / 12 - 1);
}
} // namespace choir
