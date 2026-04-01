#include "../LayeredInstrumentState.h"

#include <iostream>

static bool expect (bool condition, const char* message)
{
    if (! condition)
        std::cerr << message << '\n';

    return condition;
}

int main()
{
    bool ok = true;

    InstrumentState state;

    ok = expect (state.getLayers().size() == 1, "default state must contain one base layer") && ok;
    ok = expect (state.getLayerOrder().size() == 1, "default layer order size mismatch") && ok;

    const auto baseLayerId = state.getLayerOrder().front();
    auto* baseLayer = state.findLayerById (baseLayerId);
    ok = expect (baseLayer != nullptr, "base layer id must resolve") && ok;

    if (baseLayer != nullptr)
    {
        baseLayer->waveform = SynthVoice::Waveform::triangle;
        baseLayer->voiceCount = 6;
        baseLayer->rootNoteAbsolute = 52;
        baseLayer->mute = true;
        baseLayer->solo = true;
        baseLayer->layerVolume = 0.35f;
    }

    auto duplicateId = state.duplicateLayer (baseLayerId);
    ok = expect (duplicateId.has_value(), "duplicate layer should succeed under limit") && ok;
    ok = expect (! state.duplicateLayer (999999).has_value(), "duplicate should fail for unknown source id") && ok;

    if (duplicateId.has_value())
    {
        ok = expect (*duplicateId != baseLayerId, "duplicate layer should receive a new stable id") && ok;
        ok = expect (state.getLayerOrder().back() == *duplicateId, "duplicate should append to visual order") && ok;

        const auto* duplicatedLayer = state.findLayerById (*duplicateId);
        const auto* refreshedBaseLayer = state.findLayerById (baseLayerId);
        ok = expect (duplicatedLayer != nullptr, "duplicate layer id must resolve") && ok;

        if (duplicatedLayer != nullptr && refreshedBaseLayer != nullptr)
        {
            ok = expect (duplicatedLayer->waveform == refreshedBaseLayer->waveform, "duplicate should copy waveform from selected source") && ok;
            ok = expect (duplicatedLayer->voiceCount == refreshedBaseLayer->voiceCount, "duplicate should copy voice count from selected source") && ok;
            ok = expect (duplicatedLayer->rootNoteAbsolute == refreshedBaseLayer->rootNoteAbsolute, "duplicate should copy root note from selected source") && ok;
            ok = expect (duplicatedLayer->mute == refreshedBaseLayer->mute && duplicatedLayer->solo == refreshedBaseLayer->solo,
                         "duplicate should copy mute/solo flags from selected source")
                 && ok;
            ok = expect (juce::approximatelyEqual (duplicatedLayer->layerVolume, refreshedBaseLayer->layerVolume),
                         "duplicate should copy layer volume from selected source")
                 && ok;
        }
    }

    const auto orderBeforeMoves = state.getLayerOrder();
    ok = expect (state.moveLayerUp (1), "move up should succeed for index 1") && ok;
    ok = expect (! state.moveLayerUp (0), "move up should fail for first visual layer") && ok;
    ok = expect (state.moveLayerDown (0), "move down should succeed for index 0") && ok;
    ok = expect (! state.moveLayerDown (state.getLayerOrder().size() - 1), "move down should fail for last visual layer") && ok;
    ok = expect (state.getLayerOrder() == orderBeforeMoves, "up/down should preserve the same ids and restore order") && ok;

    while (state.getLayers().size() < InstrumentState::maxLayerCount)
        ok = expect (state.createDefaultLayer().has_value(), "create default layer should succeed before max") && ok;

    ok = expect (state.getLayers().size() == InstrumentState::maxLayerCount, "layer count should clamp at 8") && ok;
    ok = expect (! state.createDefaultLayer().has_value(), "create default layer should fail at max") && ok;
    ok = expect (! state.duplicateLayer (state.getLayerOrder().front()).has_value(), "duplicate should fail at max") && ok;

    while (state.getLayers().size() > 1)
        ok = expect (state.removeLayer (state.getLayerOrder().back()), "remove should succeed until minimum layer") && ok;

    ok = expect (! state.removeLayer (state.getLayerOrder().front()), "remove should fail when only one layer remains") && ok;

    if (! ok)
        return 1;

    std::cout << "layered instrument state validation passed." << '\n';
    return 0;
}
