#ifndef OPENXR_MENU_HPP
#define OPENXR_MENU_HPP

#include "openxrview.hpp"
#include "openxrlayer.hpp"

struct XrCompositionLayerQuad;
namespace MWVR
{
    class OpenXRMenu : public OpenXRView, public OpenXRLayer
    {

    public:
        OpenXRMenu(osg::ref_ptr<OpenXRManager> XR, OpenXRSwapchain::Config config, osg::ref_ptr<osg::State> state, const std::string& title, osg::Vec2 extent_meters);
        ~OpenXRMenu();
        const XrCompositionLayerBaseHeader* layer() override;
        const std::string& title() const { return mTitle; }
        void updatePosition();

        void swapBuffers(osg::GraphicsContext* gc) override;

    protected:
        bool mPositionNeedsUpdate{ true };
        std::unique_ptr<XrCompositionLayerQuad> mLayer = nullptr;
        std::string mTitle;
        Pose mPose{ {}, {0,0,0,1}, {} };
        osg::Vec2 mExtent;
    };
}

#endif
