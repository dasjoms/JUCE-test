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
    enum class OutputStage
    {
        none = 0,
        normalizeVoiceSum,
        softLimit
    };

    void prepare (double sampleRate, int blockSize) noexcept;
    void setWaveform (SynthVoice::Waveform waveformType) noexcept;
    void setActiveVoiceCount (int voiceCount) noexcept;
    void setVoiceStealPolicy (VoiceStealPolicy newPolicy) noexcept;
    void setEnvelopeTimes (float attackSeconds, float decaySeconds, float sustainLevel, float releaseSeconds) noexcept;
    void setModulationParameters (float depth, float rateHz, SynthVoice::ModulationDestination destination) noexcept;
    void setVelocitySensitivity (float sensitivity) noexcept;
    void setUnisonVoices (int voicesPerNote) noexcept;
    void setUnisonDetuneCents (float cents) noexcept;
    void setOutputStage (OutputStage newOutputStage) noexcept;
    VoiceStealPolicy getVoiceStealPolicy() const noexcept;

    void renderBlock (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midiMessages) noexcept;
    SynthVoice::RuntimeMetadata getVoiceMetadata (int voiceIndex) const noexcept;

private:
    struct VoiceGroupCandidate
    {
        int groupId = 0;
        std::vector<int> voiceIndices;
        bool hasReleasingVoice = false;
        uint64_t oldestStartOrder = 0;
        float amplitudeEstimate = 0.0f;
    };

    void renderRange (juce::AudioBuffer<float>& buffer, int startSample, int endSample) noexcept;
    void handleMidiEvent (const juce::MidiMessage& midiMessage) noexcept;
    std::vector<int> pickVoicesForNoteOn() noexcept;
    std::vector<VoiceGroupCandidate> buildActiveGroupCandidates() const noexcept;
    void fillStealOrder (std::vector<VoiceGroupCandidate>& groups) const noexcept;
    float getDetuneOffsetForStackIndex (int stackIndex, int stackSize) const noexcept;

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
    float currentVelocitySensitivity = 0.0f;
    SynthVoice::ModulationDestination currentModulationDestination = SynthVoice::ModulationDestination::off;
    uint64_t noteStartCounter = 0;
    int nextVoiceGroupId = 1;
    int currentUnisonVoices = 1;
    float currentUnisonDetuneCents = 0.0f;
    OutputStage outputStage = OutputStage::normalizeVoiceSum;
};
