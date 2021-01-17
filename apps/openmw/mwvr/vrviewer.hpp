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
#include <components/misc/stereo.hpp>

namespace MWVR
{
    class VRFramebuffer;
    class VRView;
    class OpenXRSwapchain;

    /// \brief Manages stereo rendering and mirror texturing.
    ///
    /// Manipulates the osgViewer by disabling main camera rendering, and instead rendering to
    /// two slave cameras, each connected to and manipulated by a VRView class.
    class VRViewer
    {
    public:
        struct UpdateViewCallback : public Misc::StereoView::UpdateViewCallback
        {
            UpdateViewCallback(VRViewer* viewer) : mViewer(viewer) {};

            //! Called during the update traversal of every frame to source updated stereo values.
            virtual void updateView(Misc::View& left, Misc::View& right) override;

            VRViewer* mViewer;
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

        class InitialDrawCallback : public osg::Camera::DrawCallback
        {
        public:
            InitialDrawCallback(VRViewer* viewer)
                : mViewer(viewer)
            {}

            void operator()(osg::RenderInfo& info) const override { mViewer->initialDrawCallback(info); };

        private:

            VRViewer* mViewer;
        };

        static const std::array<const char*, 2> sViewNames;
        enum class MirrorTextureEye
        {
            Left, 
            Right, 
            Both
        };

    public:
        VRViewer(
            osg::ref_ptr<osgViewer::Viewer> viewer);

        ~VRViewer(void);

        void swapBuffersCallback(osg::GraphicsContext* gc);
        void initialDrawCallback(osg::RenderInfo& info);
        void preDrawCallback(osg::RenderInfo& info);
        void postDrawCallback(osg::RenderInfo& info);
        void blit(osg::GraphicsContext* gc);
        void configureXR(osg::GraphicsContext* gc);
        void configureCallbacks();
        void setupMirrorTexture();
        void processChangedSettings(const std::set< std::pair<std::string, std::string> >& changed);
        void updateView(Misc::View& left, Misc::View& right);

        SubImage subImage(Side side);

        bool xrConfigured() { return mOpenXRConfigured; };
        bool callbacksConfigured() { return mCallbacksConfigured; };

    private:
        std::mutex mMutex{};
        bool mOpenXRConfigured{ false };
        bool mCallbacksConfigured{ false };

        osg::ref_ptr<osgViewer::Viewer> mViewer = nullptr;
        osg::ref_ptr<PredrawCallback> mPreDraw{ nullptr };
        osg::ref_ptr<PostdrawCallback> mPostDraw{ nullptr };
        std::shared_ptr<UpdateViewCallback> mUpdateViewCallback{ nullptr };
        bool mRenderingReady{ false };

        std::unique_ptr<VRFramebuffer> mMirrorTexture;
        std::vector<std::string> mMirrorTextureViews;
        bool mMirrorTextureShouldBeCleanedUp{ false };
        bool mMirrorTextureEnabled{ false };
        bool mFlipMirrorTextureOrder{ false };
        MirrorTextureEye mMirrorTextureEye{ MirrorTextureEye::Both };

        std::unique_ptr<VRFramebuffer> mFramebuffer;
        std::unique_ptr<VRFramebuffer> mMsaaResolveTexture;
        std::unique_ptr<VRFramebuffer> mGammaResolveTexture;
        std::array<std::unique_ptr<OpenXRSwapchain>, 2> mSwapchain;
        std::array<SubImage, 2> mSubImages;
        std::array<SwapchainConfig, 2> mSwapchainConfig;
    };
}

#endif
