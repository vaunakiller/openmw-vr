#ifndef MWVR_OPENRXVIEWER_H
#define MWVR_OPENRXVIEWER_H

#include <memory>
#include <array>
#include <map>
#include <osg/Group>
#include <osg/Camera>
#include <osgViewer/Viewer>

#include "openxrmanager.hpp"
#include "vrgui.hpp"
#include <components/sceneutil/positionattitudetransform.hpp>

namespace MWVR
{
    class VRViewer
    {
    public:
        class RealizeOperation : public OpenXRManager::RealizeOperation
        {
        public:
            RealizeOperation() {};
            void operator()(osg::GraphicsContext* gc) override;
            bool realized() override;

        private:
        };

        class SwapBuffersCallback : public osg::GraphicsContext::SwapCallback
        {
        public:
            SwapBuffersCallback(VRViewer* viewer) : mViewer(viewer) {};
            void swapBuffersImplementation(osg::GraphicsContext* gc) override;

        private:
            VRViewer* mViewer;
        };

        class PredrawCallback : public osg::Camera::DrawCallback
        {
        public:
            PredrawCallback(VRViewer* viewer)
                : mViewer(viewer)
            {}

            void operator()(osg::RenderInfo& info) const override { mViewer->preDrawCallback(info); };

        private:

            VRViewer* mViewer;
        };

        class PostdrawCallback : public osg::Camera::DrawCallback
        {
        public:
            PostdrawCallback(VRViewer* viewer)
                : mViewer(viewer)
            {}

            void operator()(osg::RenderInfo& info) const override { mViewer->postDrawCallback(info); };

        private:

            VRViewer* mViewer;
        };

    public:
        VRViewer(
            osg::ref_ptr<osgViewer::Viewer> viewer);

        ~VRViewer(void);

        //virtual void traverse(osg::NodeVisitor& visitor) override;

        const XrCompositionLayerBaseHeader* layer();

        void traversals();
        void preDrawCallback(osg::RenderInfo& info);
        void postDrawCallback(osg::RenderInfo& info);
        void blitEyesToMirrorTexture(osg::GraphicsContext* gc);
        void swapBuffers(osg::GraphicsContext* gc);
        void realize(osg::GraphicsContext* gc);
        bool realized() { return mConfigured; }

        void enableMainCamera(void);
        void disableMainCamera(void);

    public:
        osg::ref_ptr<OpenXRManager::RealizeOperation> mRealizeOperation = nullptr;
        osg::ref_ptr<osgViewer::Viewer> mViewer = nullptr;
        std::map<std::string, osg::ref_ptr<VRView> > mViews{};
        std::map<std::string, osg::ref_ptr<osg::Camera> > mCameras{};
        osg::ref_ptr<PredrawCallback> mPreDraw{ nullptr };
        osg::ref_ptr<PostdrawCallback> mPostDraw{ nullptr };
        osg::GraphicsContext* mMainCameraGC{ nullptr };

        std::unique_ptr<OpenXRSwapchain> mMirrorTextureSwapchain{ nullptr };

        std::mutex mMutex;

        bool mConfigured{ false };
    };
}

#endif
