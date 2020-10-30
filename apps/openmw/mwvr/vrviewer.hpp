#ifndef MWVR_VRVIEWER_H
#define MWVR_VRVIEWER_H

#include <memory>
#include <array>
#include <map>

#include <osg/Group>
#include <osg/Camera>
#include <osgViewer/Viewer>

#include "openxrmanager.hpp"
#include "vrshadow.hpp"

#include <components/sceneutil/positionattitudetransform.hpp>

namespace MWVR
{
    class VRFramebuffer;
    class VRView;

    /// \brief Manages stereo rendering and mirror texturing.
    ///
    /// Manipulates the osgViewer by disabling main camera rendering, and instead rendering to
    /// two slave cameras, each connected to and manipulated by a VRView class.
    class VRViewer
    {
    public:
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


        static const std::array<const char*, 2> sViewNames;

    public:
        VRViewer(
            osg::ref_ptr<osgViewer::Viewer> viewer);

        ~VRViewer(void);

        void traversals();
        void preDrawCallback(osg::RenderInfo& info);
        void postDrawCallback(osg::RenderInfo& info);
        void blitEyesToMirrorTexture(osg::GraphicsContext* gc);
        void realize(osg::GraphicsContext* gc);
        bool realized() { return mConfigured; }
        VRView* getView(std::string name);
        VrShadow& vrShadow() { return mVrShadow; }

        void enableMainCamera(void);
        void disableMainCamera(void);

    private:
        osg::ref_ptr<osgViewer::Viewer> mViewer = nullptr;
        std::map<std::string, osg::ref_ptr<VRView> > mViews;
        std::map<std::string, osg::ref_ptr<osg::Camera> > mCameras{};
        osg::ref_ptr<PredrawCallback> mPreDraw{ nullptr };
        osg::ref_ptr<PostdrawCallback> mPostDraw{ nullptr };
        osg::GraphicsContext* mMainCameraGC{ nullptr };
        std::unique_ptr<VRFramebuffer> mMsaaResolveMirrorTexture[2]{ };
        std::unique_ptr<VRFramebuffer> mMirrorTexture{ nullptr };
        VrShadow    mVrShadow;

        std::mutex mMutex{};

        bool mConfigured{ false };
        std::vector<std::string> mMirrorTextureViews;
    };
}

#endif
