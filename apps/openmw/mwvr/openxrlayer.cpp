#include "openxrlayer.hpp"


namespace MWVR {

    void OpenXRLayerStack::setLayer(Layer layer, OpenXRLayer* layerObj)
    {
        mLayerObjects[layer] = layerObj;
    }

    OpenXRLayerStack::LayerHeaders OpenXRLayerStack::layerHeaders()
    {
        LayerHeaders headers{};
        for (auto* layerObject: mLayerObjects)
        {
            auto* header = layerObject->layer();
            if (header)
                headers.push_back(header);
        }

        return headers;
    }
}
