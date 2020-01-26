#include "openxrmenu.hpp"
#include "openxrmanagerimpl.hpp"
#include <openxr/openxr.h>

namespace MWVR
{

    OpenXRMenu::OpenXRMenu(osg::ref_ptr<OpenXRManager> XR, osg::ref_ptr<osg::State> state, const std::string& title, int width, int height, osg::Vec2 extent_meters)
        : OpenXRView(XR, title)
        , mTitle(title)
    {
        setWidth(width);
        setHeight(height);
        setSamples(1);
        if (!realize(state))
            throw std::runtime_error(std::string("Failed to create swapchain for menu \"") + title + "\"");
        mLayer.reset(new XrCompositionLayerQuad);
        mLayer->type = XR_TYPE_COMPOSITION_LAYER_QUAD;
        mLayer->next = nullptr;
        mLayer->layerFlags = 0;
        mLayer->space = XR->impl().mReferenceSpaceStage;
        mLayer->eyeVisibility = XR_EYE_VISIBILITY_BOTH;
        mLayer->subImage = swapchain().subImage();
        mLayer->size.width = extent_meters.x();
        mLayer->size.height = extent_meters.y();

        // Orientation needs a norm of 1 to be accepted by OpenXR, so we default it to 0,0,0,1
        mLayer->pose.orientation.w = 1.f;

        //updatePosition();
    }

    OpenXRMenu::~OpenXRMenu()
    {
    }

    const XrCompositionLayerBaseHeader* 
        OpenXRMenu::layer()
    {
        updatePosition();
        return reinterpret_cast<XrCompositionLayerBaseHeader*>(mLayer.get());
    }

    void OpenXRMenu::updatePosition()
    {        
        if (!mPositionNeedsUpdate)
            return;
        if (!mXR->sessionRunning())
            return;
        if (OpenXRFrameIndexer::instance().updateIndex() == 0)
            return;

        // Menus are position one meter in front of the player, facing the player.
        // Go via osg since OpenXR doesn't distribute a linear maths library
        auto pose = mXR->impl().getPredictedLimbPose(OpenXRFrameIndexer::instance().updateIndex(), TrackedLimb::HEAD, TrackedSpace::STAGE);
        pose.position += pose.orientation * osg::Vec3(0, 0, -1);
        mLayer->pose.position = osg::toXR(pose.position);
        mLayer->pose.orientation = osg::toXR(-pose.orientation);
        mPositionNeedsUpdate = false;

        Log(Debug::Verbose) << "Menu pose updated to: " << pose;
    }

    void OpenXRMenu::postrenderCallback(osg::RenderInfo& info)
    {
        OpenXRView::postrenderCallback(info);
    }
}
