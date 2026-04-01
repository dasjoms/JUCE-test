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
    const auto* baseLayer = state.findLayerById (baseLayerId);
    ok = expect (baseLayer != nullptr, "base layer id must resolve") && ok;

    if (baseLayer != nullptr)
    {
        ok = expect (baseLayer->rootNoteAbsolute == 60, "default root note should be midi 60") && ok;
        ok = expect (! baseLayer->mute && ! baseLayer->solo, "base layer mute/solo defaults mismatch") && ok;
    }

    auto duplicateId = state.duplicateLayer (baseLayerId);
    ok = expect (duplicateId.has_value(), "duplicate layer should succeed under limit") && ok;
    ok = expect (! state.duplicateLayer (999999).has_value(), "duplicate should fail for unknown source id") && ok;

    if (duplicateId.has_value())
    {
        ok = expect (*duplicateId != baseLayerId, "duplicate layer should receive a new stable id") && ok;
        ok = expect (state.getLayerOrder().back() == *duplicateId, "duplicate should append to visual order") && ok;
    }

    ok = expect (state.moveLayerUp (1), "move up should succeed for index 1") && ok;
    ok = expect (state.getLayerOrder()[0] == duplicateId.value_or (0), "move up should swap visual order") && ok;
    ok = expect (! state.moveLayerUp (0), "move up should fail for first visual layer") && ok;
    ok = expect (state.moveLayerDown (0), "move down should succeed for index 0") && ok;
    ok = expect (state.getLayerOrder()[0] == baseLayerId, "move down should restore base-first order") && ok;
    ok = expect (! state.moveLayerDown (state.getLayerOrder().size() - 1), "move down should fail for last visual layer") && ok;

    while (state.getLayers().size() < InstrumentState::maxLayerCount)
        ok = expect (state.createDefaultLayer().has_value(), "create default layer should succeed before max") && ok;

    ok = expect (! state.createDefaultLayer().has_value(), "create default layer should fail at max") && ok;

    const auto removeTargetId = state.getLayerOrder()[2];
    ok = expect (state.removeLayer (removeTargetId), "remove layer should succeed with >1 layers") && ok;

    const auto resolvedIndex = InstrumentState::resolveSelectedLayerIndexAfterDelete (2, state.getLayerOrder().size());
    ok = expect (resolvedIndex == 1, "selection should fallback to previous visual index after delete") && ok;

    while (state.getLayers().size() > 1)
        ok = expect (state.removeLayer (state.getLayerOrder().back()), "remove should succeed until minimum layer") && ok;

    ok = expect (! state.removeLayer (state.getLayerOrder().front()), "remove should fail when only one layer remains") && ok;

    if (! ok)
        return 1;

    std::cout << "layered instrument state validation passed." << '\n';
    return 0;
}
