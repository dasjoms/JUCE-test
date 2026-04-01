#include "SynthEngine.h"

#include <algorithm>
#include <cmath>

SynthEngine::SynthEngine (int maxVoices)
    : voices (static_cast<size_t> (juce::jmax (1, maxVoices)))
{
    setActiveVoiceCount (1);
}

void SynthEngine::prepare (double sampleRate, int blockSize) noexcept
{
    currentSampleRate = sampleRate;
    currentBlockSize = blockSize;

    for (auto& voice : voices)
    {
        voice.prepare (currentSampleRate);
        voice.setWaveform (currentWaveform);
        voice.setEnvelopeTimes (currentAttackSeconds, currentDecaySeconds, currentSustainLevel, currentReleaseSeconds);
        voice.setModulationParameters (currentModulationDepth, currentModulationRateHz, currentModulationDestination);
        voice.setVelocitySensitivity (currentVelocitySensitivity);
    }

    noteStartCounter = 0;
    nextVoiceGroupId = 1;
}

void SynthEngine::setWaveform (SynthVoice::Waveform waveformType) noexcept
{
    currentWaveform = waveformType;

    for (auto index = 0; index < activeVoiceCount; ++index)
        voices[static_cast<size_t> (index)].setWaveform (waveformType);
}

void SynthEngine::setActiveVoiceCount (int voiceCount) noexcept
{
    activeVoiceCount = juce::jlimit (1, static_cast<int> (voices.size()), voiceCount);

    for (auto index = activeVoiceCount; index < static_cast<int> (voices.size()); ++index)
        voices[static_cast<size_t> (index)].allNotesOff();
}

void SynthEngine::setVoiceStealPolicy (VoiceStealPolicy newPolicy) noexcept
{
    voiceStealPolicy = newPolicy;
}

void SynthEngine::setEnvelopeTimes (float attackSeconds, float decaySeconds, float sustainLevel, float releaseSeconds) noexcept
{
    currentAttackSeconds = juce::jmax (0.001f, attackSeconds);
    currentDecaySeconds = juce::jmax (0.001f, decaySeconds);
    currentSustainLevel = juce::jlimit (0.0f, 1.0f, sustainLevel);
    currentReleaseSeconds = juce::jmax (0.001f, releaseSeconds);

    for (auto index = 0; index < activeVoiceCount; ++index)
        voices[static_cast<size_t> (index)].setEnvelopeTimes (currentAttackSeconds, currentDecaySeconds, currentSustainLevel, currentReleaseSeconds);
}

void SynthEngine::setModulationParameters (float depth, float rateHz, SynthVoice::ModulationDestination destination) noexcept
{
    currentModulationDepth = juce::jlimit (0.0f, 1.0f, depth);
    currentModulationRateHz = juce::jmax (0.0f, rateHz);
    currentModulationDestination = destination;

    for (auto index = 0; index < activeVoiceCount; ++index)
        voices[static_cast<size_t> (index)].setModulationParameters (currentModulationDepth,
                                                                     currentModulationRateHz,
                                                                     currentModulationDestination);
}

void SynthEngine::setVelocitySensitivity (float sensitivity) noexcept
{
    currentVelocitySensitivity = juce::jlimit (0.0f, 1.0f, sensitivity);

    for (auto index = 0; index < activeVoiceCount; ++index)
        voices[static_cast<size_t> (index)].setVelocitySensitivity (currentVelocitySensitivity);
}

void SynthEngine::setUnisonVoices (int voicesPerNote) noexcept
{
    currentUnisonVoices = juce::jlimit (1, activeVoiceCount, voicesPerNote);
}

void SynthEngine::setUnisonDetuneCents (float cents) noexcept
{
    currentUnisonDetuneCents = juce::jlimit (0.0f, 120.0f, cents);
}

void SynthEngine::setOutputStage (OutputStage newOutputStage) noexcept
{
    outputStage = newOutputStage;
}

SynthEngine::VoiceStealPolicy SynthEngine::getVoiceStealPolicy() const noexcept
{
    return voiceStealPolicy;
}

void SynthEngine::renderBlock (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midiMessages) noexcept
{
    const auto numSamples = buffer.getNumSamples();
    int renderedSamples = 0;

    for (const auto midiMetadata : midiMessages)
    {
        const auto eventOffset = juce::jlimit (0, numSamples, midiMetadata.samplePosition);

        if (eventOffset > renderedSamples)
            renderRange (buffer, renderedSamples, eventOffset);

        handleMidiEvent (midiMetadata.getMessage());
        renderedSamples = eventOffset;
    }

    if (renderedSamples < numSamples)
        renderRange (buffer, renderedSamples, numSamples);
}

void SynthEngine::renderRange (juce::AudioBuffer<float>& buffer, int startSample, int endSample) noexcept
{
    const auto numChannels = buffer.getNumChannels();
    const auto unisonGainCompensation = 1.0f / std::sqrt (static_cast<float> (juce::jmax (1, currentUnisonVoices)));
    const auto voiceCountNormalization = 1.0f / std::sqrt (static_cast<float> (juce::jmax (1, activeVoiceCount)));
    constexpr auto softLimitDrive = 1.35f;
    const auto softLimitNormalization = 1.0f / std::tanh (softLimitDrive);

    for (int sample = startSample; sample < endSample; ++sample)
    {
        float sampleValue = 0.0f;

        for (auto voiceIndex = 0; voiceIndex < activeVoiceCount; ++voiceIndex)
            sampleValue += voices[static_cast<size_t> (voiceIndex)].renderSample();

        sampleValue *= unisonGainCompensation;

        if (outputStage == OutputStage::normalizeVoiceSum)
        {
            sampleValue *= voiceCountNormalization;
        }
        else if (outputStage == OutputStage::softLimit)
        {
            sampleValue = std::tanh (sampleValue * softLimitDrive) * softLimitNormalization;
        }

        for (int channel = 0; channel < numChannels; ++channel)
            buffer.setSample (channel, sample, sampleValue);
    }
}

void SynthEngine::handleMidiEvent (const juce::MidiMessage& midiMessage) noexcept
{
    if (midiMessage.isNoteOn())
    {
        const auto selectedVoices = pickVoicesForNoteOn();
        const auto groupId = nextVoiceGroupId++;

        for (auto stackIndex = 0; stackIndex < static_cast<int> (selectedVoices.size()); ++stackIndex)
        {
            const auto voiceIndex = selectedVoices[static_cast<size_t> (stackIndex)];
            auto& voice = voices[static_cast<size_t> (voiceIndex)];
            voice.setStartOrder (noteStartCounter++);
            voice.setVoiceGroupId (groupId);
            voice.setDetuneCents (getDetuneOffsetForStackIndex (stackIndex, static_cast<int> (selectedVoices.size())));
            voice.noteOn (midiMessage.getNoteNumber(), midiMessage.getFloatVelocity());
        }
        return;
    }

    if (midiMessage.isNoteOff())
    {
        for (auto voiceIndex = 0; voiceIndex < activeVoiceCount; ++voiceIndex)
            voices[static_cast<size_t> (voiceIndex)].noteOff (midiMessage.getNoteNumber());

        return;
    }

    if (midiMessage.isAllNotesOff() || midiMessage.isAllSoundOff())
    {
        for (auto voiceIndex = 0; voiceIndex < activeVoiceCount; ++voiceIndex)
            voices[static_cast<size_t> (voiceIndex)].allNotesOff();
    }
}

std::vector<int> SynthEngine::pickVoicesForNoteOn() noexcept
{
    const auto requiredVoices = juce::jlimit (1, activeVoiceCount, currentUnisonVoices);
    std::vector<int> selected;
    selected.reserve (static_cast<size_t> (requiredVoices));

    for (auto voiceIndex = 0; voiceIndex < activeVoiceCount && static_cast<int> (selected.size()) < requiredVoices; ++voiceIndex)
    {
        const auto metadata = voices[static_cast<size_t> (voiceIndex)].getRuntimeMetadata();
        if (! metadata.isActive)
            selected.push_back (voiceIndex);
    }

    if (static_cast<int> (selected.size()) >= requiredVoices)
        return selected;

    auto groups = buildActiveGroupCandidates();
    fillStealOrder (groups);

    for (const auto& group : groups)
    {
        if (static_cast<int> (selected.size()) >= requiredVoices)
            break;

        for (const auto voiceIndex : group.voiceIndices)
            selected.push_back (voiceIndex);
    }

    if (static_cast<int> (selected.size()) > requiredVoices)
        selected.resize (static_cast<size_t> (requiredVoices));

    return selected;
}

std::vector<SynthEngine::VoiceGroupCandidate> SynthEngine::buildActiveGroupCandidates() const noexcept
{
    std::vector<VoiceGroupCandidate> groups;

    for (auto voiceIndex = 0; voiceIndex < activeVoiceCount; ++voiceIndex)
    {
        const auto metadata = voices[static_cast<size_t> (voiceIndex)].getRuntimeMetadata();
        if (! metadata.isActive)
            continue;

        const auto existing = std::find_if (groups.begin(), groups.end(), [metadata] (const VoiceGroupCandidate& candidate)
        {
            return candidate.groupId == metadata.voiceGroupId;
        });

        if (existing == groups.end())
        {
            VoiceGroupCandidate candidate;
            candidate.groupId = metadata.voiceGroupId;
            candidate.oldestStartOrder = metadata.startOrder;
            candidate.amplitudeEstimate = metadata.amplitudeEstimate;
            candidate.hasReleasingVoice = metadata.isReleasing;
            candidate.voiceIndices.push_back (voiceIndex);
            groups.push_back (std::move (candidate));
        }
        else
        {
            existing->voiceIndices.push_back (voiceIndex);
            existing->hasReleasingVoice = existing->hasReleasingVoice || metadata.isReleasing;
            existing->oldestStartOrder = juce::jmin (existing->oldestStartOrder, metadata.startOrder);
            existing->amplitudeEstimate = juce::jmin (existing->amplitudeEstimate, metadata.amplitudeEstimate);
        }
    }

    return groups;
}

void SynthEngine::fillStealOrder (std::vector<VoiceGroupCandidate>& groups) const noexcept
{
    switch (voiceStealPolicy)
    {
        case VoiceStealPolicy::releasedFirst:
            std::sort (groups.begin(), groups.end(), [] (const VoiceGroupCandidate& left, const VoiceGroupCandidate& right)
            {
                if (left.hasReleasingVoice != right.hasReleasingVoice)
                    return left.hasReleasingVoice;
                return left.oldestStartOrder < right.oldestStartOrder;
            });
            break;

        case VoiceStealPolicy::oldest:
            std::sort (groups.begin(), groups.end(), [] (const VoiceGroupCandidate& left, const VoiceGroupCandidate& right)
            {
                return left.oldestStartOrder < right.oldestStartOrder;
            });
            break;

        case VoiceStealPolicy::quietest:
            std::sort (groups.begin(), groups.end(), [] (const VoiceGroupCandidate& left, const VoiceGroupCandidate& right)
            {
                const auto amplitudeDelta = std::abs (left.amplitudeEstimate - right.amplitudeEstimate);
                if (amplitudeDelta > 1.0e-6f)
                    return left.amplitudeEstimate < right.amplitudeEstimate;

                return left.oldestStartOrder < right.oldestStartOrder;
            });
            break;
    }
}

float SynthEngine::getDetuneOffsetForStackIndex (int stackIndex, int stackSize) const noexcept
{
    if (stackSize <= 1)
        return 0.0f;

    const auto center = 0.5f * static_cast<float> (stackSize - 1);
    const auto normalizedPosition = (static_cast<float> (stackIndex) - center) / juce::jmax (0.5f, center);
    return normalizedPosition * currentUnisonDetuneCents;
}

SynthVoice::RuntimeMetadata SynthEngine::getVoiceMetadata (int voiceIndex) const noexcept
{
    const auto boundedIndex = juce::jlimit (0, activeVoiceCount - 1, voiceIndex);
    return voices[static_cast<size_t> (boundedIndex)].getRuntimeMetadata();
}
