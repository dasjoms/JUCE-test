#include "SynthEngine.h"

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
    }

    nextVoiceIndex = 0;
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
    nextVoiceIndex = 0;

    for (auto index = activeVoiceCount; index < static_cast<int> (voices.size()); ++index)
        voices[static_cast<size_t> (index)].allNotesOff();
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
    if (activeVoiceCount <= 1)
        return 0;

    const auto selectedVoice = nextVoiceIndex;
    nextVoiceIndex = (nextVoiceIndex + 1) % activeVoiceCount;
    return selectedVoice;
}
