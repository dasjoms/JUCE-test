#include "SynthEngine.h"

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
        voice.setEnvelopeTimes (currentAttackSeconds, currentReleaseSeconds);
        voice.setModulationParameters (currentModulationDepth, currentModulationRateHz);
    }

    noteStartCounter = 0;
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

void SynthEngine::setEnvelopeTimes (float attackSeconds, float releaseSeconds) noexcept
{
    currentAttackSeconds = juce::jmax (0.001f, attackSeconds);
    currentReleaseSeconds = juce::jmax (0.001f, releaseSeconds);

    for (auto index = 0; index < activeVoiceCount; ++index)
        voices[static_cast<size_t> (index)].setEnvelopeTimes (currentAttackSeconds, currentReleaseSeconds);
}

void SynthEngine::setModulationParameters (float depth, float rateHz) noexcept
{
    currentModulationDepth = juce::jlimit (0.0f, 1.0f, depth);
    currentModulationRateHz = juce::jmax (0.0f, rateHz);

    for (auto index = 0; index < activeVoiceCount; ++index)
        voices[static_cast<size_t> (index)].setModulationParameters (currentModulationDepth, currentModulationRateHz);
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

    for (int sample = startSample; sample < endSample; ++sample)
    {
        float sampleValue = 0.0f;

        for (auto voiceIndex = 0; voiceIndex < activeVoiceCount; ++voiceIndex)
            sampleValue += voices[static_cast<size_t> (voiceIndex)].renderSample();

        for (int channel = 0; channel < numChannels; ++channel)
            buffer.setSample (channel, sample, sampleValue);
    }
}

void SynthEngine::handleMidiEvent (const juce::MidiMessage& midiMessage) noexcept
{
    if (midiMessage.isNoteOn())
    {
        const auto voiceIndex = pickVoiceForNoteOn();
        voices[static_cast<size_t> (voiceIndex)].setStartOrder (noteStartCounter++);
        voices[static_cast<size_t> (voiceIndex)].noteOn (midiMessage.getNoteNumber());
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

int SynthEngine::pickVoiceForNoteOn() noexcept
{
    const auto idleVoiceIndex = findIdleVoice();

    if (idleVoiceIndex >= 0)
        return idleVoiceIndex;

    return selectVoiceByPolicy();
}

SynthVoice::RuntimeMetadata SynthEngine::getVoiceMetadata (int voiceIndex) const noexcept
{
    const auto boundedIndex = juce::jlimit (0, activeVoiceCount - 1, voiceIndex);
    return voices[static_cast<size_t> (boundedIndex)].getRuntimeMetadata();
}

int SynthEngine::selectVoiceByPolicy() const noexcept
{
    switch (voiceStealPolicy)
    {
        case VoiceStealPolicy::releasedFirst:
        {
            for (auto voiceIndex = 0; voiceIndex < activeVoiceCount; ++voiceIndex)
            {
                if (voices[static_cast<size_t> (voiceIndex)].getRuntimeMetadata().isReleasing)
                    return voiceIndex;
            }

            return selectOldestVoice();
        }

        case VoiceStealPolicy::oldest:
            return selectOldestVoice();

        case VoiceStealPolicy::quietest:
            return selectQuietestVoice();
    }

    jassertfalse;
    return 0;
}

int SynthEngine::selectOldestVoice() const noexcept
{
    auto selectedIndex = 0;
    auto selectedOrder = voices[0].getRuntimeMetadata().startOrder;

    for (auto voiceIndex = 1; voiceIndex < activeVoiceCount; ++voiceIndex)
    {
        const auto voiceOrder = voices[static_cast<size_t> (voiceIndex)].getRuntimeMetadata().startOrder;

        if (voiceOrder < selectedOrder)
        {
            selectedOrder = voiceOrder;
            selectedIndex = voiceIndex;
        }
    }

    return selectedIndex;
}

int SynthEngine::selectQuietestVoice() const noexcept
{
    auto selectedIndex = 0;
    auto selectedMetadata = voices[0].getRuntimeMetadata();

    for (auto voiceIndex = 1; voiceIndex < activeVoiceCount; ++voiceIndex)
    {
        const auto metadata = voices[static_cast<size_t> (voiceIndex)].getRuntimeMetadata();
        const auto lowerAmplitude = metadata.amplitudeEstimate < selectedMetadata.amplitudeEstimate;
        const auto amplitudesAreEqual = std::abs (metadata.amplitudeEstimate - selectedMetadata.amplitudeEstimate) < 1.0e-6f;
        const auto tieBreakByAge = amplitudesAreEqual
                                && metadata.startOrder < selectedMetadata.startOrder;

        if (lowerAmplitude || tieBreakByAge)
        {
            selectedMetadata = metadata;
            selectedIndex = voiceIndex;
        }
    }

    return selectedIndex;
}

int SynthEngine::findIdleVoice() const noexcept
{
    for (auto voiceIndex = 0; voiceIndex < activeVoiceCount; ++voiceIndex)
    {
        if (! voices[static_cast<size_t> (voiceIndex)].getRuntimeMetadata().isActive)
            return voiceIndex;
    }

    return -1;
}
