#pragma once

#include "SynthEngine.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

struct LayerState
{
    uint64_t layerId = 0;

    SynthVoice::Waveform waveform = SynthVoice::Waveform::sine;
    int voiceCount = 1;
    SynthEngine::VoiceStealPolicy stealPolicy = SynthEngine::VoiceStealPolicy::releasedFirst;

    float attackSeconds = 0.005f;
    float decaySeconds = 0.08f;
    float sustainLevel = 0.8f;
    float releaseSeconds = 0.03f;

    float modulationDepth = 0.0f;
    float modulationRateHz = 2.0f;
    SynthVoice::ModulationDestination modulationDestination = SynthVoice::ModulationDestination::off;

    int unisonVoices = 1;
    float unisonDetuneCents = 0.0f;

    SynthEngine::OutputStage outputStage = SynthEngine::OutputStage::normalizeVoiceSum;
    float velocitySensitivity = 0.0f;

    int rootNoteAbsolute = 60; // Canonical absolute MIDI root note. Default C4 (MIDI 60).
    bool mute = false;
    bool solo = false;
    float layerVolume = 1.0f;
};

class InstrumentState final
{
public:
    static constexpr std::size_t maxLayerCount = 8;

    InstrumentState();

    const std::vector<LayerState>& getLayers() const noexcept;
    const std::vector<uint64_t>& getLayerOrder() const noexcept;

    LayerState* findLayerById (uint64_t layerId) noexcept;
    const LayerState* findLayerById (uint64_t layerId) const noexcept;

    std::optional<uint64_t> createDefaultLayer();
    std::optional<uint64_t> duplicateLayer (uint64_t sourceLayerId);

    bool removeLayer (uint64_t layerId);

    bool moveLayerUp (std::size_t visualIndex);
    bool moveLayerDown (std::size_t visualIndex);
    bool restoreFromSerializedState (std::vector<LayerState> restoredLayers,
                                     std::vector<uint64_t> restoredOrder,
                                     uint64_t restoredNextLayerId) noexcept;

    static std::size_t resolveSelectedLayerIndexAfterDelete (std::size_t removedVisualIndex,
                                                             std::size_t remainingLayerCount) noexcept;

private:
    LayerState makeDefaultLayer() const noexcept;
    bool appendLayer (LayerState layer);
    std::vector<LayerState> layers;
    std::vector<uint64_t> layerOrder;
    uint64_t nextLayerId = 1;
};
