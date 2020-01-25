#include "openxrlayer.hpp"


namespace MWVR {

    void OpenXRLayerStack::setLayer(Layer layer, OpenXRLayer* layerObj)
    {
        mLayers[layer] = nullptr;
        mLayerObjects[layer] = layerObj;
    }

    int OpenXRLayerStack::layerCount()
    {
        int count = 0;
        for (auto* layer : mLayers)
            if (layer)
                count++;
        return count;
    }

    const XrCompositionLayerBaseHeader** OpenXRLayerStack::layerHeaders()
    {
        // If i had more than two layers i'd make a general solution using a vector member instead.

        for (unsigned i = 0; i < mLayerObjects.size(); i++)
            if (mLayerObjects[i])
                mLayers[i] = mLayerObjects[i]->layer();
            else
                mLayers[i] = nullptr;

        if (mLayers[0])
            return mLayers.data();

        if (!mLayers[0])
            if (mLayers[1])
                return mLayers.data() + 1;

        if (!mLayers[0])
            if (!mLayers[1])
                return nullptr;

        return nullptr;
    }
}
