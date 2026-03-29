#include "MonoSynthAudioProcessor.h"
#include "MonoSynthAudioProcessorEditor.h"
#include <cmath>

namespace
{
constexpr float outputLevel = 0.12f;
constexpr double twoPi = juce::MathConstants<double>::twoPi;
constexpr double inverseTwoPi = 1.0 / twoPi;
constexpr float attackTimeSeconds = 0.005f;
constexpr float releaseTimeSeconds = 0.03f;
constexpr float noteTransitionRampTimeSeconds = 0.002f;
constexpr float minimumEnvelopeValue = 1.0e-5f;
constexpr auto waveformParameterId = "waveform";
} // namespace

//==============================================================================
MonoSynthAudioProcessor::MonoSynthAudioProcessor()
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                      #endif
                       )
     , parameters (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    waveformParameter = parameters.getRawParameterValue (waveformParameterId);
    updateWaveformFromParameter();
}

MonoSynthAudioProcessor::~MonoSynthAudioProcessor()
{
}

//==============================================================================
const juce::String MonoSynthAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool MonoSynthAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool MonoSynthAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool MonoSynthAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double MonoSynthAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int MonoSynthAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int MonoSynthAudioProcessor::getCurrentProgram()
{
    return 0;
}

void MonoSynthAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String MonoSynthAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void MonoSynthAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void MonoSynthAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);

    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    phase = 0.0;
    gateOpen = false;
    currentMidiNote = -1;
    pendingMidiNote = -1;
    currentFrequencyHz = 440.0;
    pendingFrequencyHz = 440.0;
    currentAmplitude = 0.0f;
    noteTransitionState = NoteTransitionState::none;
    shouldResetPhaseOnNoteChange = false;
    attackStep = 1.0f / static_cast<float> (juce::jmax (1.0, currentSampleRate * attackTimeSeconds));
    releaseStep = 1.0f / static_cast<float> (juce::jmax (1.0, currentSampleRate * releaseTimeSeconds));
    noteTransitionStep = 1.0f / static_cast<float> (juce::jmax (1.0, currentSampleRate * noteTransitionRampTimeSeconds));
    updatePhaseIncrement();
}

void MonoSynthAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

bool MonoSynthAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}

void MonoSynthAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    const auto numSamples = buffer.getNumSamples();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    updateWaveformFromParameter();

    const auto renderRange = [this, &buffer, totalNumOutputChannels] (int startSample, int endSample)
    {
        for (int sample = startSample; sample < endSample; ++sample)
        {
            float sampleValue = 0.0f;
            const auto currentWaveform = waveform.load (std::memory_order_relaxed);
            const auto shouldRunTransitionRamp = (noteTransitionState == NoteTransitionState::rampDownForNoteChange)
                                              || (noteTransitionState == NoteTransitionState::rampUpAfterNoteChange);
            const auto targetAmplitude = shouldRunTransitionRamp ? (noteTransitionState == NoteTransitionState::rampDownForNoteChange ? 0.0f : 1.0f)
                                                                 : (gateOpen ? 1.0f : 0.0f);
            const auto envelopeStep = shouldRunTransitionRamp ? noteTransitionStep
                                                              : (gateOpen ? attackStep : releaseStep);

            if (currentAmplitude < targetAmplitude)
            {
                currentAmplitude = juce::jmin (targetAmplitude, currentAmplitude + envelopeStep);
            }
            else if (currentAmplitude > targetAmplitude)
            {
                currentAmplitude = juce::jmax (targetAmplitude, currentAmplitude - envelopeStep);
            }

            if (noteTransitionState == NoteTransitionState::rampDownForNoteChange
                && currentAmplitude <= minimumEnvelopeValue)
            {
                currentAmplitude = 0.0f;
                currentMidiNote = pendingMidiNote;
                currentFrequencyHz = pendingFrequencyHz;

                if (shouldResetPhaseOnNoteChange)
                    phase = 0.0;

                updatePhaseIncrement();
                gateOpen = true;
                noteTransitionState = NoteTransitionState::rampUpAfterNoteChange;
            }
            else if (noteTransitionState == NoteTransitionState::rampUpAfterNoteChange
                     && currentAmplitude >= (1.0f - minimumEnvelopeValue))
            {
                noteTransitionState = NoteTransitionState::none;
            }

            if (gateOpen || currentAmplitude > minimumEnvelopeValue)
            {
                sampleValue = outputLevel * currentAmplitude * getOscillatorSample (phase, currentWaveform);
                phase += phaseIncrement;

                if (phase >= twoPi)
                    phase -= twoPi;
            }
            else
            {
                currentAmplitude = 0.0f;
            }

            for (int channel = 0; channel < totalNumOutputChannels; ++channel)
                buffer.setSample (channel, sample, sampleValue);
        }
    };

    int renderedSamples = 0;

    for (const auto midiMetadata : midiMessages)
    {
        const auto eventOffset = juce::jlimit (0, numSamples, midiMetadata.samplePosition);

        if (eventOffset > renderedSamples)
            renderRange (renderedSamples, eventOffset);

        handleMidiEvent (midiMetadata.getMessage());
        renderedSamples = eventOffset;
    }

    if (renderedSamples < numSamples)
        renderRange (renderedSamples, numSamples);

    midiMessages.clear();
}

void MonoSynthAudioProcessor::handleMidiEvent (const juce::MidiMessage& midiMessage)
{
    if (midiMessage.isNoteOn())
    {
        const auto incomingNote = midiMessage.getNoteNumber();
        const auto incomingFrequencyHz = juce::MidiMessage::getMidiNoteInHertz (incomingNote);

        // If a note is already sounding, perform a short ramp-to-zero,
        // then switch note/frequency near silence, then ramp up.
        if (gateOpen && currentAmplitude > minimumEnvelopeValue)
        {
            pendingMidiNote = incomingNote;
            pendingFrequencyHz = incomingFrequencyHz;
            shouldResetPhaseOnNoteChange = true;
            gateOpen = false;
            noteTransitionState = NoteTransitionState::rampDownForNoteChange;
            return;
        }

        const auto shouldResetPhase = (! gateOpen) || (currentAmplitude <= minimumEnvelopeValue);
        currentMidiNote = incomingNote;
        currentFrequencyHz = incomingFrequencyHz;

        if (shouldResetPhase)
            phase = 0.0;

        gateOpen = true;
        noteTransitionState = NoteTransitionState::none;
        pendingMidiNote = -1;
        pendingFrequencyHz = currentFrequencyHz;
        shouldResetPhaseOnNoteChange = false;
        updatePhaseIncrement();
        return;
    }

    if (midiMessage.isNoteOff())
    {
        if (midiMessage.getNoteNumber() == currentMidiNote)
        {
            gateOpen = false;

            if (noteTransitionState == NoteTransitionState::rampUpAfterNoteChange)
                noteTransitionState = NoteTransitionState::none;
        }

        if (midiMessage.getNoteNumber() == pendingMidiNote)
        {
            pendingMidiNote = -1;
            pendingFrequencyHz = currentFrequencyHz;

            if (noteTransitionState == NoteTransitionState::rampDownForNoteChange)
                noteTransitionState = NoteTransitionState::none;
        }

        return;
    }

    if (midiMessage.isAllNotesOff() || midiMessage.isAllSoundOff())
    {
        gateOpen = false;
        noteTransitionState = NoteTransitionState::none;
    }
}

MonoSynthAudioProcessor::Waveform MonoSynthAudioProcessor::getWaveform() const noexcept
{
    return waveform.load (std::memory_order_relaxed);
}

juce::AudioProcessorValueTreeState::ParameterLayout MonoSynthAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> layout;
    layout.push_back (std::make_unique<juce::AudioParameterChoice> (waveformParameterId,
                                                                     "Waveform",
                                                                     getWaveformChoices(),
                                                                     waveformToChoiceIndex (Waveform::sine)));
    return { layout.begin(), layout.end() };
}

void MonoSynthAudioProcessor::updateWaveformFromParameter() noexcept
{
    if (waveformParameter == nullptr)
        return;

    waveform.store (waveformFromParameterValue (waveformParameter->load (std::memory_order_relaxed)),
                    std::memory_order_relaxed);
}

MonoSynthAudioProcessor::Waveform MonoSynthAudioProcessor::waveformFromParameterValue (float parameterValue) noexcept
{
    return waveformFromChoiceIndex (juce::roundToInt (parameterValue));
}

int MonoSynthAudioProcessor::waveformToChoiceIndex (Waveform waveformType) noexcept
{
    return static_cast<int> (waveformType);
}

MonoSynthAudioProcessor::Waveform MonoSynthAudioProcessor::waveformFromChoiceIndex (int choiceIndex) noexcept
{
    switch (choiceIndex)
    {
        case 0: return Waveform::sine;
        case 1: return Waveform::saw;
        case 2: return Waveform::square;
        case 3: return Waveform::triangle;
        default: break;
    }

    return Waveform::sine;
}

const juce::StringArray& MonoSynthAudioProcessor::getWaveformChoices() noexcept
{
    static const juce::StringArray choices { "Sine", "Saw", "Square", "Triangle" };
    return choices;
}

float MonoSynthAudioProcessor::getOscillatorSample (double phaseInRadians, Waveform waveformType) const noexcept
{
    switch (waveformType)
    {
        case Waveform::sine:
            return std::sin (phaseInRadians);
        case Waveform::saw:
            return static_cast<float> ((phaseInRadians * inverseTwoPi * 2.0) - 1.0);
        case Waveform::square:
            return phaseInRadians < juce::MathConstants<double>::pi ? 1.0f : -1.0f;
        case Waveform::triangle:
        {
            const auto normalizedPhase = phaseInRadians * inverseTwoPi;
            return static_cast<float> (1.0 - 4.0 * std::abs (normalizedPhase - 0.5));
        }
    }

    jassertfalse;
    return 0.0f;
}

void MonoSynthAudioProcessor::updatePhaseIncrement() noexcept
{
    phaseIncrement = twoPi * currentFrequencyHz / currentSampleRate;
}

//==============================================================================
bool MonoSynthAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* MonoSynthAudioProcessor::createEditor()
{
    return new MonoSynthAudioProcessorEditor (*this);
}

//==============================================================================
void MonoSynthAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (const auto xml = parameters.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void MonoSynthAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (const auto xml = getXmlFromBinary (data, sizeInBytes); xml != nullptr)
    {
        if (xml->hasTagName (parameters.state.getType()))
            parameters.replaceState (juce::ValueTree::fromXml (*xml));
    }

    updateWaveformFromParameter();
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MonoSynthAudioProcessor();
}
