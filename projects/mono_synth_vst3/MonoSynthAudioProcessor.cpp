#include "MonoSynthAudioProcessor.h"
#include "MonoSynthAudioProcessorEditor.h"

namespace
{
constexpr float outputLevel = 0.15f;
constexpr double twoPi = juce::MathConstants<double>::twoPi;
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
{
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
    currentFrequencyHz = 440.0;
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

    for (const auto midiMetadata : midiMessages)
        handleMidiEvent (midiMetadata.getMessage());

    midiMessages.clear();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float sampleValue = 0.0f;

        if (gateOpen)
        {
            sampleValue = outputLevel * std::sin (phase);
            phase += phaseIncrement;

            if (phase >= twoPi)
                phase -= twoPi;
        }

        for (int channel = 0; channel < totalNumOutputChannels; ++channel)
            buffer.setSample (channel, sample, sampleValue);
    }
}

void MonoSynthAudioProcessor::handleMidiEvent (const juce::MidiMessage& midiMessage)
{
    if (midiMessage.isNoteOn())
    {
        // Monophonic retrigger policy: latest note-on always becomes active and
        // restarts the oscillator phase.
        currentMidiNote = midiMessage.getNoteNumber();
        currentFrequencyHz = juce::MidiMessage::getMidiNoteInHertz (currentMidiNote);
        phase = 0.0;
        gateOpen = true;
        updatePhaseIncrement();
        return;
    }

    if (midiMessage.isNoteOff())
    {
        if (midiMessage.getNoteNumber() == currentMidiNote)
            gateOpen = false;

        return;
    }

    if (midiMessage.isAllNotesOff() || midiMessage.isAllSoundOff())
        gateOpen = false;
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
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    juce::ignoreUnused (destData);
}

void MonoSynthAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    juce::ignoreUnused (data, sizeInBytes);
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MonoSynthAudioProcessor();
}
