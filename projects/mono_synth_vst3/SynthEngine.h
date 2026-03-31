#pragma once

#include "SynthVoice.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

class SynthEngine final
{
public:
    explicit SynthEngine (int maxVoices = 8);

    void prepare (double sampleRate, int blockSize) noexcept;
    void setWaveform (SynthVoice::Waveform waveformType) noexcept;
    void setActiveVoiceCount (int voiceCount) noexcept;

    void renderBlock (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midiMessages) noexcept;

private:
    void renderRange (juce::AudioBuffer<float>& buffer, int startSample, int endSample) noexcept;
    void handleMidiEvent (const juce::MidiMessage& midiMessage) noexcept;
    int pickVoiceForNoteOn() noexcept;

    std::vector<SynthVoice> voices;
    int activeVoiceCount = 1;
    int nextVoiceIndex = 0;
    double currentSampleRate = 44100.0;
    int currentBlockSize = 0;
    SynthVoice::Waveform currentWaveform = SynthVoice::Waveform::sine;
};
