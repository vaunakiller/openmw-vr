#ifndef MWVR_OPENRXVIEWER_H
#define MWVR_OPENRXVIEWER_H

#include <memory>
#include <array>
#include <map>
#include <osg/Group>
#include <osg/Camera>
#include <osgViewer/Viewer>

#include "openxrmanager.hpp"
#include "openxrsession.hpp"
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

        class SwapBuffersCallback : public osg::GraphicsContext::SwapCallback
        {
        public:
            SwapBuffersCallback(OpenXRViewer* viewer) : mViewer(viewer) {};
            void swapBuffersImplementation(osg::GraphicsContext* gc) override;

        private:
            OpenXRViewer* mViewer;
        };

        class PredrawCallback : public osg::Camera::DrawCallback
        {
        public:
            PredrawCallback(OpenXRViewer* viewer)
                : mViewer(viewer)
            {}

            void operator()(osg::RenderInfo& info) const override { mViewer->preDrawCallback(info); };

        private:

            OpenXRViewer* mViewer;
        };

        class PostdrawCallback : public osg::Camera::DrawCallback
        {
        public:
            PostdrawCallback(OpenXRViewer* viewer)
                : mViewer(viewer)
            {}

            void operator()(osg::RenderInfo& info) const override { mViewer->postDrawCallback(info); };

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
        void preDrawCallback(osg::RenderInfo& info);
        void postDrawCallback(osg::RenderInfo& info);
        void blitEyesToMirrorTexture(osg::GraphicsContext* gc);
        void swapBuffers(osg::GraphicsContext* gc) override;
        void realize(osg::GraphicsContext* gc);

        bool realized() { return mConfigured; }

    public:

        std::unique_ptr<XrCompositionLayerProjection> mLayer = nullptr;
        std::vector<XrCompositionLayerProjectionView> mCompositionLayerProjectionViews;
        osg::observer_ptr<OpenXRManager> mXR = nullptr;
        osg::ref_ptr<OpenXRManager::RealizeOperation> mRealizeOperation = nullptr;
        osg::observer_ptr<osgViewer::Viewer> mViewer = nullptr;
        std::unique_ptr<OpenXRInputManager> mXRInput = nullptr;
        std::unique_ptr<OpenXRSession> mXRSession = nullptr;
        std::map<std::string, osg::ref_ptr<OpenXRView> > mViews{};
        std::map<std::string, osg::ref_ptr<osg::Camera> > mCameras{};

        PredrawCallback* mPreDraw{ nullptr };
        PostdrawCallback* mPostDraw{ nullptr };

        std::unique_ptr<OpenXRSwapchain> mMirrorTextureSwapchain = nullptr;

        std::mutex mMutex;

        float mMetersPerUnit = 1.f;
        bool mConfigured = false;
    };
}

#endif
