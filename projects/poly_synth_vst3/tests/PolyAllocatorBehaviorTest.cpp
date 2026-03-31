#include "../SynthEngine.h"

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

int countActiveVoices (const SynthEngine& engine, int activeVoiceCount)
{
    int active = 0;

    for (int voiceIndex = 0; voiceIndex < activeVoiceCount; ++voiceIndex)
        if (engine.getVoiceMetadata (voiceIndex).isActive)
            ++active;

    return active;
}

bool validateActiveVoiceLimitRespected()
{
    SynthEngine engine (8);
    engine.prepare (48000.0, 64);
    engine.setActiveVoiceCount (2);

    processMidi (engine, {
                           juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100),
                           juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100),
                           juce::MidiMessage::noteOn (1, 67, (juce::uint8) 100)
                       });

    const auto active = countActiveVoices (engine, 2);

    if (active > 2)
    {
        std::cerr << "active voice count exceeded configured limit: " << active << " > 2" << '\n';
        return false;
    }

    return true;
}

bool validateVoiceLimitIncreaseAllowsAdditionalNotes()
{
    SynthEngine engine (8);
    engine.prepare (48000.0, 64);
    engine.setActiveVoiceCount (1);

    processMidi (engine, {
                           juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100),
                           juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100)
                       });

    engine.setActiveVoiceCount (3);

    processMidi (engine, {
                           juce::MidiMessage::noteOn (1, 67, (juce::uint8) 100),
                           juce::MidiMessage::noteOn (1, 71, (juce::uint8) 100)
                       });

    const auto active = countActiveVoices (engine, 3);

    if (active < 3)
    {
        std::cerr << "expected at least three active voices after increasing max voices; got " << active << '\n';
        return false;
    }

    return true;
}

} // namespace

int main()
{
    if (! validateActiveVoiceLimitRespected())
        return 1;

    if (! validateVoiceLimitIncreaseAllowsAdditionalNotes())
        return 1;

    std::cout << "poly allocator behaviour validation passed." << '\n';
    return 0;
}
