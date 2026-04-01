#include "LayeredInstrumentState.h"

#include <algorithm>

InstrumentState::InstrumentState()
{
    static_cast<void> (createDefaultLayer());
}

const std::vector<LayerState>& InstrumentState::getLayers() const noexcept
{
    return layers;
}

const std::vector<uint64_t>& InstrumentState::getLayerOrder() const noexcept
{
    return layerOrder;
}

LayerState* InstrumentState::findLayerById (uint64_t layerId) noexcept
{
    const auto it = std::find_if (layers.begin(),
                                  layers.end(),
                                  [layerId] (const LayerState& layer)
                                  {
                                      return layer.layerId == layerId;
                                  });

    if (it == layers.end())
        return nullptr;

    return std::addressof (*it);
}

const LayerState* InstrumentState::findLayerById (uint64_t layerId) const noexcept
{
    const auto it = std::find_if (layers.begin(),
                                  layers.end(),
                                  [layerId] (const LayerState& layer)
                                  {
                                      return layer.layerId == layerId;
                                  });

    if (it == layers.end())
        return nullptr;

    return std::addressof (*it);
}

std::optional<uint64_t> InstrumentState::createDefaultLayer()
{
    auto layer = makeDefaultLayer();

    if (! appendLayer (std::move (layer)))
        return std::nullopt;

    return layerOrder.back();
}

std::optional<uint64_t> InstrumentState::duplicateLayer (uint64_t sourceLayerId)
{
    const auto* sourceLayer = findLayerById (sourceLayerId);

    if (sourceLayer == nullptr || layers.size() >= maxLayerCount)
        return std::nullopt;

    auto duplicatedLayer = *sourceLayer;
    duplicatedLayer.layerId = nextLayerId++;

    if (! appendLayer (std::move (duplicatedLayer)))
        return std::nullopt;

    return layerOrder.back();
}

bool InstrumentState::removeLayer (uint64_t layerId)
{
    if (layers.size() <= 1)
        return false;

    const auto layerIt = std::find_if (layers.begin(),
                                       layers.end(),
                                       [layerId] (const LayerState& layer)
                                       {
                                           return layer.layerId == layerId;
                                       });

    if (layerIt == layers.end())
        return false;

    layers.erase (layerIt);

    const auto orderIt = std::find (layerOrder.begin(), layerOrder.end(), layerId);

    if (orderIt != layerOrder.end())
        layerOrder.erase (orderIt);

    return true;
}

bool InstrumentState::moveLayerUp (std::size_t visualIndex)
{
    if (visualIndex == 0 || visualIndex >= layerOrder.size())
        return false;

    std::swap (layerOrder[visualIndex], layerOrder[visualIndex - 1]);
    return true;
}

bool InstrumentState::moveLayerDown (std::size_t visualIndex)
{
    if (visualIndex + 1 >= layerOrder.size())
        return false;

    std::swap (layerOrder[visualIndex], layerOrder[visualIndex + 1]);
    return true;
}

bool InstrumentState::restoreFromSerializedState (std::vector<LayerState> restoredLayers,
                                                  std::vector<uint64_t> restoredOrder,
                                                  uint64_t restoredNextLayerId) noexcept
{
    if (restoredLayers.empty() || restoredOrder.empty() || restoredLayers.size() > maxLayerCount)
        return false;

    layers = std::move (restoredLayers);
    layerOrder = std::move (restoredOrder);

    uint64_t maxLayerId = 0;
    for (const auto& layer : layers)
        maxLayerId = std::max (maxLayerId, layer.layerId);

    nextLayerId = std::max (maxLayerId + 1, restoredNextLayerId);
    return true;
}

std::size_t InstrumentState::resolveSelectedLayerIndexAfterDelete (std::size_t removedVisualIndex,
                                                                   std::size_t remainingLayerCount) noexcept
{
    if (remainingLayerCount == 0)
        return 0;

    if (removedVisualIndex == 0)
        return 0;

    return std::min (removedVisualIndex - 1, remainingLayerCount - 1);
}

LayerState InstrumentState::makeDefaultLayer() const noexcept
{
    LayerState layer;
    layer.layerId = nextLayerId;
    return layer;
}

bool InstrumentState::appendLayer (LayerState layer)
{
    if (layers.size() >= maxLayerCount)
        return false;

    // Stable IDs decouple logical identity from visual ordering.
    // This is required for deterministic recall of automation, host state, and preset routing when
    // layers are reordered, duplicated, or removed in later phases.
    const auto newLayerId = layer.layerId;
    layers.push_back (std::move (layer));
    layerOrder.push_back (newLayerId);
    nextLayerId = std::max (nextLayerId, newLayerId + 1);
    return true;
}
