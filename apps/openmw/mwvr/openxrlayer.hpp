#ifndef OPENXR_LAYER_HPP
#define OPENXR_LAYER_HPP

#include <array>
#include "openxrmanager.hpp"
#include "vrtexture.hpp"

struct XrCompositionLayerBaseHeader;

namespace MWVR
{
    class OpenXRLayer
    {
    public:
        OpenXRLayer(void) = default;
        virtual ~OpenXRLayer(void) = default;

        virtual const XrCompositionLayerBaseHeader* layer() = 0;
        virtual void swapBuffers(osg::GraphicsContext* gc) = 0;

        bool isVisible() const { return mVisible; };
        void setVisible(bool visible) { mVisible = visible; };

    protected:

        bool mVisible{ false };
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
        using LayerHeaders = std::vector<const XrCompositionLayerBaseHeader*>;

        OpenXRLayerStack() = default;
        ~OpenXRLayerStack() = default;


        void setLayer(Layer layer, OpenXRLayer* layerObj);
        LayerHeaders layerHeaders();
        LayerObjectStack& layerObjects() { return mLayerObjects; };

    private:
        LayerObjectStack mLayerObjects;
    };
}

#endif
