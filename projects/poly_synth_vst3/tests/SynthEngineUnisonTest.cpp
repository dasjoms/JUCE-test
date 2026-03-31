#include "../SynthEngine.h"

#include <algorithm>
#include <iostream>

namespace
{
void processMidi (SynthEngine& engine, std::initializer_list<juce::MidiMessage> messages)
{
    juce::AudioBuffer<float> buffer (2, 64);
    buffer.clear();

    juce::MidiBuffer midi;
    for (const auto& message : messages)
        midi.addEvent (message, 0);

    engine.renderBlock (buffer, midi);
}

int countActiveVoices (const SynthEngine& engine, int voiceCount)
{
    int active = 0;

    for (int voiceIndex = 0; voiceIndex < voiceCount; ++voiceIndex)
    {
        if (engine.getVoiceMetadata (voiceIndex).isActive)
            ++active;
    }

    return active;
}

bool validateUnisonAllocationRespectsVoiceLimit()
{
    SynthEngine engine (4);
    engine.prepare (48000.0, 64);
    engine.setActiveVoiceCount (4);
    engine.setUnisonVoices (3);
    engine.setUnisonDetuneCents (18.0f);

    processMidi (engine, { juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100) });
    processMidi (engine, { juce::MidiMessage::noteOn (1, 67, (juce::uint8) 100) });

    const auto activeVoices = countActiveVoices (engine, 4);
    if (activeVoices > 4)
    {
        std::cerr << "unison allocation exceeded max voices: " << activeVoices << " > 4" << '\n';
        return false;
    }

    return true;
}

bool validateDetuneOffsetsDeterministic()
{
    SynthEngine engine (6);
    engine.prepare (48000.0, 64);
    engine.setActiveVoiceCount (6);
    engine.setUnisonVoices (3);
    engine.setUnisonDetuneCents (15.0f);

    processMidi (engine, { juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100) });

    float detunes[3] = { 0.0f, 0.0f, 0.0f };
    int detuneIndex = 0;
    int voiceGroup = 0;

    for (int voiceIndex = 0; voiceIndex < 6; ++voiceIndex)
    {
        const auto metadata = engine.getVoiceMetadata (voiceIndex);
        if (! metadata.isActive)
            continue;

        if (voiceGroup == 0)
            voiceGroup = metadata.voiceGroupId;

        if (metadata.voiceGroupId == voiceGroup && detuneIndex < 3)
            detunes[detuneIndex++] = metadata.detuneCents;
    }

    if (detuneIndex != 3)
    {
        std::cerr << "expected three unison voices for detune test; got " << detuneIndex << '\n';
        return false;
    }

    std::sort (std::begin (detunes), std::end (detunes));

    if (! juce::approximatelyEqual (detunes[0], -15.0f)
        || ! juce::approximatelyEqual (detunes[1], 0.0f)
        || ! juce::approximatelyEqual (detunes[2], 15.0f))
    {
        std::cerr << "detune offsets were not deterministic/symmetric: ["
                  << detunes[0] << ", " << detunes[1] << ", " << detunes[2] << "]" << '\n';
        return false;
    }

    return true;
}

bool validateNoteOffReleasesEntireGroup()
{
    SynthEngine engine (6);
    engine.prepare (48000.0, 64);
    engine.setActiveVoiceCount (6);
    engine.setUnisonVoices (3);
    engine.setUnisonDetuneCents (12.0f);

    processMidi (engine, { juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100) });

    int targetGroupId = 0;
    for (int voiceIndex = 0; voiceIndex < 6; ++voiceIndex)
    {
        const auto metadata = engine.getVoiceMetadata (voiceIndex);
        if (metadata.isActive)
        {
            targetGroupId = metadata.voiceGroupId;
            break;
        }
    }

    processMidi (engine, { juce::MidiMessage::noteOff (1, 60) });

    int releasingVoices = 0;
    for (int voiceIndex = 0; voiceIndex < 6; ++voiceIndex)
    {
        const auto metadata = engine.getVoiceMetadata (voiceIndex);
        if (metadata.voiceGroupId == targetGroupId && metadata.isReleasing)
            ++releasingVoices;
    }

    if (releasingVoices != 3)
    {
        std::cerr << "expected full unison group to release; got " << releasingVoices << " voices" << '\n';
        return false;
    }

    return true;
}

} // namespace

int main()
{
    if (! validateUnisonAllocationRespectsVoiceLimit())
        return 1;

    if (! validateDetuneOffsetsDeterministic())
        return 1;

    if (! validateNoteOffReleasesEntireGroup())
        return 1;

    std::cout << "synth engine unison behavior validation passed." << '\n';
    return 0;
}
