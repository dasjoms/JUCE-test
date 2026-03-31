#include "../SynthVoice.h"

#include <cmath>
#include <iostream>

namespace
{
void renderSamples (SynthVoice& voice, int sampleCount)
{
    for (int sample = 0; sample < sampleCount; ++sample)
        (void) voice.renderSample();
}

bool validateAttackDecayAndSustainTransitions()
{
    SynthVoice voice;
    voice.prepare (1000.0);
    voice.setEnvelopeTimes (0.01f, 0.02f, 0.40f, 0.05f);
    voice.noteOn (60);

    renderSamples (voice, 10);
    const auto postAttack = voice.getRuntimeMetadata().amplitudeEstimate;

    if (postAttack < 0.98f)
    {
        std::cerr << "attack stage did not reach near-peak amplitude" << '\n';
        return false;
    }

    renderSamples (voice, 20);
    const auto postDecay = voice.getRuntimeMetadata().amplitudeEstimate;

    if (std::abs (postDecay - 0.40f) > 0.02f)
    {
        std::cerr << "decay stage did not converge to sustain level" << '\n';
        return false;
    }

    renderSamples (voice, 25);
    const auto sustainHeld = voice.getRuntimeMetadata().amplitudeEstimate;

    if (std::abs (sustainHeld - 0.40f) > 0.02f)
    {
        std::cerr << "sustain stage did not hold target level" << '\n';
        return false;
    }

    return true;
}

bool validateSustainHoldsUntilNoteOff()
{
    SynthVoice voice;
    voice.prepare (2000.0);
    voice.setEnvelopeTimes (0.005f, 0.01f, 0.55f, 0.05f);
    voice.noteOn (67);

    renderSamples (voice, 200);

    const auto heldBefore = voice.getRuntimeMetadata().amplitudeEstimate;
    renderSamples (voice, 100);
    const auto heldAfter = voice.getRuntimeMetadata().amplitudeEstimate;

    if (std::abs (heldBefore - heldAfter) > 0.01f)
    {
        std::cerr << "sustain level drifted while note remained held" << '\n';
        return false;
    }

    return true;
}

bool validateReleaseTransitionsToIdle()
{
    SynthVoice voice;
    voice.prepare (1000.0);
    voice.setEnvelopeTimes (0.01f, 0.02f, 0.50f, 0.04f);
    voice.noteOn (72);
    renderSamples (voice, 120);

    voice.noteOff (72);

    const auto releasingMetadata = voice.getRuntimeMetadata();

    if (! releasingMetadata.isReleasing)
    {
        std::cerr << "voice should enter release state after noteOff" << '\n';
        return false;
    }

    renderSamples (voice, 60);
    const auto idleMetadata = voice.getRuntimeMetadata();

    if (idleMetadata.isActive || idleMetadata.midiNote != -1)
    {
        std::cerr << "voice did not return to idle after release phase" << '\n';
        return false;
    }

    return true;
}

} // namespace

int main()
{
    if (! validateAttackDecayAndSustainTransitions())
        return 1;

    if (! validateSustainHoldsUntilNoteOff())
        return 1;

    if (! validateReleaseTransitionsToIdle())
        return 1;

    std::cout << "synth voice ADSR envelope validation passed." << '\n';
    return 0;
}
