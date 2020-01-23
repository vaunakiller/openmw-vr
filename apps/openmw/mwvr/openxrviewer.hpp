#ifndef MWVR_OPENRXVIEWER_H
#define MWVR_OPENRXVIEWER_H

#include <memory>
#include <array>
#include <osg/Group>
#include <osg/Camera>
#include <osgViewer/Viewer>

#include "openxrmanager.hpp"
#include "openxrlayer.hpp"
#include "openxrworldview.hpp"
#include "openxrmenu.hpp"
#include "openxrinputmanager.hpp"

struct XrCompositionLayerProjection;
struct XrCompositionLayerProjectionView;
namespace MWVR
{
    class OpenXRViewer : public osg::Group, public OpenXRLayer
    {
    public:
        class RealizeOperation : public OpenXRManager::RealizeOperation
        {
        public:
            RealizeOperation(osg::ref_ptr<OpenXRManager> XR, osg::ref_ptr<OpenXRViewer> viewer) : OpenXRManager::RealizeOperation(XR), mViewer(viewer) {};
            void operator()(osg::GraphicsContext* gc) override;
            bool realized() override;

        private:
            osg::ref_ptr<OpenXRViewer> mViewer;
        };

        class UpdateSlaveCallback : public osg::View::Slave::UpdateSlaveCallback
        {
        public:
            UpdateSlaveCallback(osg::ref_ptr<OpenXRManager> XR, osg::ref_ptr<OpenXRWorldView> view, osg::GraphicsContext* gc)
                : mXR(XR), mView(view), mGC(gc)
            {}

            void updateSlave(osg::View& view, osg::View::Slave& slave) override;

        private:
            osg::ref_ptr<OpenXRManager> mXR;
            osg::ref_ptr<OpenXRWorldView> mView;
            osg::ref_ptr<osg::GraphicsContext> mGC;
        };
        class SwapBuffersCallback : public osg::GraphicsContext::SwapCallback
        {
        public:
            SwapBuffersCallback(OpenXRViewer* viewer) : mViewer(viewer) {};
            void swapBuffersImplementation(osg::GraphicsContext* gc) override;

        private:
            OpenXRViewer* mViewer;
        };

    public:
        OpenXRViewer(
            osg::ref_ptr<OpenXRManager> XR,
            osg::ref_ptr<osgViewer::Viewer> viewer,
            float metersPerUnit = 1.f);

        ~OpenXRViewer(void);

        virtual void traverse(osg::NodeVisitor& visitor) override;

        const XrCompositionLayerBaseHeader* layer() override;

        void traversals();
        void blitEyesToMirrorTexture(osg::GraphicsContext* gc) const;
        void swapBuffers(osg::GraphicsContext* gc) ;
        void realize(osg::GraphicsContext* gc);

        bool realized() { return mConfigured; }

    protected:

        std::unique_ptr<XrCompositionLayerProjection> mLayer = nullptr;
        std::vector<XrCompositionLayerProjectionView> mCompositionLayerProjectionViews;
        osg::observer_ptr<OpenXRManager> mXR = nullptr;
        osg::ref_ptr<OpenXRManager::RealizeOperation> mRealizeOperation = nullptr;
        osg::observer_ptr<osgViewer::Viewer> mViewer = nullptr;
        std::unique_ptr<MWVR::OpenXRInputManager> mXRInput = nullptr;
        std::unique_ptr<MWVR::OpenXRMenu> mXRMenu = nullptr;
        std::array<osg::ref_ptr<OpenXRWorldView>, 2> mViews{};

        //osg::ref_ptr<SDLUtil::GraphicsWindowSDL2> mViewerGW;
        //osg::ref_ptr<SDLUtil::GraphicsWindowSDL2> mLeftGW;
        //osg::ref_ptr<SDLUtil::GraphicsWindowSDL2> mRightGW;

        osg::Camera* mMainCamera = nullptr;
        osg::Camera* mLeftCamera = nullptr;
        osg::Camera* mRightCamera = nullptr;

        std::unique_ptr<OpenXRSwapchain> mMirrorTextureSwapchain = nullptr;

        std::mutex mMutex;

        float mMetersPerUnit = 1.f;
        bool mConfigured = false;
    };
}

#endif
