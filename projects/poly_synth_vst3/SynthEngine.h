#pragma once

#include "SynthVoice.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

class SynthEngine final
{
public:
    enum class VoiceStealPolicy
    {
        releasedFirst = 0,
        oldest,
        quietest
    };

    explicit SynthEngine (int maxVoices = 8);

    void prepare (double sampleRate, int blockSize) noexcept;
    void setWaveform (SynthVoice::Waveform waveformType) noexcept;
    void setActiveVoiceCount (int voiceCount) noexcept;
    void setVoiceStealPolicy (VoiceStealPolicy newPolicy) noexcept;
    void setEnvelopeTimes (float attackSeconds, float decaySeconds, float sustainLevel, float releaseSeconds) noexcept;
    void setModulationParameters (float depth, float rateHz) noexcept;
    VoiceStealPolicy getVoiceStealPolicy() const noexcept;

    void renderBlock (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midiMessages) noexcept;
    SynthVoice::RuntimeMetadata getVoiceMetadata (int voiceIndex) const noexcept;

private:
    void renderRange (juce::AudioBuffer<float>& buffer, int startSample, int endSample) noexcept;
    void handleMidiEvent (const juce::MidiMessage& midiMessage) noexcept;
    int pickVoiceForNoteOn() noexcept;
    int selectVoiceByPolicy() const noexcept;
    int selectOldestVoice() const noexcept;
    int selectQuietestVoice() const noexcept;
    int findIdleVoice() const noexcept;

    std::vector<SynthVoice> voices;
    int activeVoiceCount = 1;
    double currentSampleRate = 44100.0;
    int currentBlockSize = 0;
    SynthVoice::Waveform currentWaveform = SynthVoice::Waveform::sine;
    VoiceStealPolicy voiceStealPolicy = VoiceStealPolicy::releasedFirst;
    float currentAttackSeconds = 0.005f;
    float currentDecaySeconds = 0.08f;
    float currentSustainLevel = 0.8f;
    float currentReleaseSeconds = 0.03f;
    float currentModulationDepth = 0.0f;
    float currentModulationRateHz = 0.0f;
    uint64_t noteStartCounter = 0;
};
