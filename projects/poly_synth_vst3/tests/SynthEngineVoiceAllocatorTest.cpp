#include "../SynthEngine.h"

#include <iostream>

namespace
{
void processMidi (SynthEngine& engine, std::initializer_list<juce::MidiMessage> messages)
{
    constexpr int numChannels = 2;
    constexpr int numSamples = 32;

    juce::AudioBuffer<float> buffer (numChannels, numSamples);
    buffer.clear();

    juce::MidiBuffer midi;

    for (const auto& message : messages)
        midi.addEvent (message, 0);

    engine.renderBlock (buffer, midi);
}

int findVoiceForNote (const SynthEngine& engine, int activeVoiceCount, int midiNote)
{
    for (int voiceIndex = 0; voiceIndex < activeVoiceCount; ++voiceIndex)
    {
        if (engine.getVoiceMetadata (voiceIndex).midiNote == midiNote)
            return voiceIndex;
    }

    return -1;
}

bool validateIdleVoiceChosenBeforeSteal()
{
    SynthEngine engine (3);
    engine.prepare (48000.0, 64);
    engine.setActiveVoiceCount (3);

    processMidi (engine, { juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100) });
    const auto firstNoteVoice = findVoiceForNote (engine, 3, 60);

    processMidi (engine, { juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100) });
    const auto secondNoteVoice = findVoiceForNote (engine, 3, 64);

    if (firstNoteVoice < 0 || secondNoteVoice < 0)
    {
        std::cerr << "idle-voice test expected both notes to be active" << '\n';
        return false;
    }

    if (firstNoteVoice == secondNoteVoice)
    {
        std::cerr << "idle-voice test expected a different voice for second note" << '\n';
        return false;
    }

    return true;
}

bool validateReleasedFirstStealsReleasingVoice()
{
    SynthEngine engine (2);
    engine.prepare (48000.0, 64);
    engine.setActiveVoiceCount (2);
    engine.setVoiceStealPolicy (SynthEngine::VoiceStealPolicy::releasedFirst);

    processMidi (engine, {
                           juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100),
                           juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100)
                       });

    const auto note60Voice = findVoiceForNote (engine, 2, 60);
    const auto note64Voice = findVoiceForNote (engine, 2, 64);

    if (note60Voice < 0 || note64Voice < 0)
    {
        std::cerr << "released-first setup failed to allocate initial notes" << '\n';
        return false;
    }

    processMidi (engine, { juce::MidiMessage::noteOff (1, 60) });

    if (! engine.getVoiceMetadata (note60Voice).isReleasing)
    {
        std::cerr << "released-first expected note-60 voice to enter releasing state" << '\n';
        return false;
    }

    processMidi (engine, { juce::MidiMessage::noteOn (1, 67, (juce::uint8) 100) });

    if (findVoiceForNote (engine, 2, 67) != note60Voice)
    {
        std::cerr << "released-first should steal releasing voice first" << '\n';
        return false;
    }

    if (findVoiceForNote (engine, 2, 64) != note64Voice)
    {
        std::cerr << "released-first should keep sustained note voice untouched" << '\n';
        return false;
    }

    return true;
}

bool validateOldestStealsOldestActiveVoice()
{
    SynthEngine engine (2);
    engine.prepare (48000.0, 64);
    engine.setActiveVoiceCount (2);
    engine.setVoiceStealPolicy (SynthEngine::VoiceStealPolicy::oldest);

    processMidi (engine, { juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100) });
    processMidi (engine, { juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100) });

    const auto oldestVoice = findVoiceForNote (engine, 2, 60);

    processMidi (engine, { juce::MidiMessage::noteOn (1, 67, (juce::uint8) 100) });

    if (findVoiceForNote (engine, 2, 67) != oldestVoice)
    {
        std::cerr << "oldest policy did not steal the oldest voice" << '\n';
        return false;
    }

    if (findVoiceForNote (engine, 2, 64) < 0)
    {
        std::cerr << "oldest policy should keep newer sustained note active" << '\n';
        return false;
    }

    return true;
}

bool validateQuietestPrefersLowerAmplitudeVoice()
{
    SynthEngine engine (2);
    engine.prepare (48000.0, 64);
    engine.setActiveVoiceCount (2);
    engine.setVoiceStealPolicy (SynthEngine::VoiceStealPolicy::quietest);

    processMidi (engine, {
                           juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100),
                           juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100)
                       });
    const auto note60Voice = findVoiceForNote (engine, 2, 60);
    const auto note64Voice = findVoiceForNote (engine, 2, 64);

    if (note60Voice < 0 || note64Voice < 0)
    {
        std::cerr << "quietest setup failed to allocate both sustained notes" << '\n';
        return false;
    }

    processMidi (engine, { juce::MidiMessage::noteOff (1, 60) });

    const auto note60Amplitude = engine.getVoiceMetadata (note60Voice).amplitudeEstimate;
    const auto note64Amplitude = engine.getVoiceMetadata (note64Voice).amplitudeEstimate;

    if (note60Amplitude >= note64Amplitude)
    {
        std::cerr << "quietest setup expected note-60 voice to be lower amplitude" << '\n';
        return false;
    }

    processMidi (engine, { juce::MidiMessage::noteOn (1, 67, (juce::uint8) 100) });

    if (findVoiceForNote (engine, 2, 67) != note60Voice)
    {
        std::cerr << "quietest policy should steal lower-amplitude voice" << '\n';
        return false;
    }

    return true;
}

} // namespace

int main()
{
    if (! validateIdleVoiceChosenBeforeSteal())
        return 1;

    if (! validateReleasedFirstStealsReleasingVoice())
        return 1;

    if (! validateOldestStealsOldestActiveVoice())
        return 1;

    if (! validateQuietestPrefersLowerAmplitudeVoice())
        return 1;

    std::cout << "synth engine voice allocator policy validation passed." << '\n';
    return 0;
}
