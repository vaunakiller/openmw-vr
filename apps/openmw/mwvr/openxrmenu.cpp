#include "openxrmenu.hpp"
#include "openxrmanagerimpl.hpp"
#include <openxr/openxr.h>

namespace MWVR
{

    OpenXRMenu::OpenXRMenu(
        osg::ref_ptr<OpenXRManager> XR,
        OpenXRSwapchain::Config config, 
        osg::ref_ptr<osg::State> state, 
        const std::string& title,
        osg::Vec2 extent_meters)
        : OpenXRView(XR, title, config, state)
        , mTitle(title)
        , mExtent(extent_meters)
    {

        //updatePosition();
    }

    OpenXRMenu::~OpenXRMenu()
    {
    }

    const XrCompositionLayerBaseHeader* 
        OpenXRMenu::layer()
    {

        if (mPositionNeedsUpdate)
        {
            // I position menus half a meter in front of the player, facing the player.
            mPose = predictedPose();
            mPose.position += mPose.orientation * osg::Vec3(0, 0.5, 0);
            mPose.orientation = -mPose.orientation;

            Log(Debug::Verbose) << "Menu pose updated to: " << mPose;
            mPositionNeedsUpdate = false;
        }
        
        if (mLayer)
        {
            mLayer->pose.position = osg::toXR(mPose.position);
            mLayer->pose.orientation = osg::toXR(mPose.orientation);
        }

        if(mVisible)
            return reinterpret_cast<XrCompositionLayerBaseHeader*>(mLayer.get());
        return nullptr;
    }

    void OpenXRMenu::updatePosition()
    {        
        mPositionNeedsUpdate = true;
    }

    void OpenXRMenu::swapBuffers(
        osg::GraphicsContext* gc)
    {
        OpenXRView::swapBuffers(gc);

        if (!mLayer)
        {
            mLayer.reset(new XrCompositionLayerQuad);
            mLayer->type = XR_TYPE_COMPOSITION_LAYER_QUAD;
            mLayer->next = nullptr;
            mLayer->layerFlags = 0;
            mLayer->space = mXR->impl().mReferenceSpaceStage;
            mLayer->eyeVisibility = XR_EYE_VISIBILITY_BOTH;
            mLayer->subImage = swapchain().subImage();
            mLayer->size.width = mExtent.x();
            mLayer->size.height = mExtent.y();

            // Orientation needs a norm of 1 to be accepted by OpenXR, so we default it to 0,0,0,1
            mLayer->pose.orientation.w = 1.f;
        }
    }
}
