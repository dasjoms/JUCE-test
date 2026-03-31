#include "../SynthVoice.h"

#include <cmath>
#include <iostream>

namespace
{
float renderPeakMagnitude (float noteVelocity, float sensitivity)
{
    SynthVoice voice;
    voice.prepare (48000.0);
    voice.setWaveform (SynthVoice::Waveform::sine);
    voice.setEnvelopeTimes (0.001f, 0.01f, 1.0f, 0.05f);
    voice.setVelocitySensitivity (sensitivity);
    voice.noteOn (60, noteVelocity);

    auto peakMagnitude = 0.0f;

    for (int sample = 0; sample < 2048; ++sample)
        peakMagnitude = juce::jmax (peakMagnitude, std::abs (voice.renderSample()));

    return peakMagnitude;
}

bool validateVelocitySensitivityScalesAmplitude()
{
    const auto lowVelocityPeak = renderPeakMagnitude (0.2f, 1.0f);
    const auto highVelocityPeak = renderPeakMagnitude (1.0f, 1.0f);

    if (highVelocityPeak <= lowVelocityPeak)
    {
        std::cerr << "higher velocity should render higher peak when sensitivity is enabled" << '\n';
        return false;
    }

    return true;
}

bool validateZeroSensitivityIgnoresVelocity()
{
    const auto lowVelocityPeak = renderPeakMagnitude (0.2f, 0.0f);
    const auto highVelocityPeak = renderPeakMagnitude (1.0f, 0.0f);

    if (std::abs (highVelocityPeak - lowVelocityPeak) > 1.0e-4f)
    {
        std::cerr << "zero sensitivity should ignore velocity in output scaling" << '\n';
        return false;
    }

    return true;
}

} // namespace

int main()
{
    if (! validateVelocitySensitivityScalesAmplitude())
        return 1;

    if (! validateZeroSensitivityIgnoresVelocity())
        return 1;

    std::cout << "synth voice velocity sensitivity validation passed." << '\n';
    return 0;
}
