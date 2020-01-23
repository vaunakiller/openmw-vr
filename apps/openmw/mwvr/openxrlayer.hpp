#ifndef OPENXR_LAYER_HPP
#define OPENXR_LAYER_HPP

#include <array>
#include "openxrmanager.hpp"
#include "openxrtexture.hpp"

struct XrCompositionLayerBaseHeader;

namespace MWVR
{
    class OpenXRLayer
    {
    public:
        OpenXRLayer(void) = default;
        virtual ~OpenXRLayer(void) = default;

        virtual const XrCompositionLayerBaseHeader* layer() = 0;
    };

    class OpenXRLayerStack
    {
    public:
        enum Layer {
            WORLD_VIEW_LAYER = 0,
            MENU_VIEW_LAYER = 1,
            LAYER_MAX = MENU_VIEW_LAYER //!< Used to size layer arrays. Not a valid input.
        };
        using LayerObjectStack = std::array< OpenXRLayer*, LAYER_MAX + 1>;
        using LayerStack = std::array< const XrCompositionLayerBaseHeader*, LAYER_MAX + 1>;

        OpenXRLayerStack() = default;
        ~OpenXRLayerStack() = default;

        void setLayer(Layer layer, OpenXRLayer* layerObj);
        int layerCount();
        const XrCompositionLayerBaseHeader** layerHeaders();

    private:
        LayerObjectStack mLayerObjects;
        LayerStack mLayers;
    };
}

#endif
