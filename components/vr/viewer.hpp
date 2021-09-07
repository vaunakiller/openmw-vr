#ifndef VR_VIEWER_H
#define VR_VIEWER_H

#include <memory>
#include <array>
#include <map>
#include <queue>

#include <osg/Group>
#include <osg/Camera>
#include <osgViewer/Viewer>
#include <osg/RenderInfo>

#include <components/sceneutil/positionattitudetransform.hpp>
#include <components/misc/stereo.hpp>
#include <components/misc/callbackmanager.hpp>
#include <components/vr/constants.hpp>
#include <components/vr/frame.hpp>
#include <components/vr/layer.hpp>

namespace VR
{
    class Swapchain;
    class TrackingManager;
    class Session;

    /// \brief Manages stereo rendering and mirror texturing.
    ///
    /// Manipulates the osgViewer by disabling main camera rendering, and instead rendering to
    /// two slave cameras, each connected to and manipulated by a VRView class.
    class Viewer
    {
    public:
        struct UpdateViewCallback : public Misc::StereoView::UpdateViewCallback
        {
            UpdateViewCallback(Viewer* viewer) : mViewer(viewer) {};

            //! Called during the update traversal of every frame to source updated stereo values.
            virtual void updateView(Misc::View& left, Misc::View& right) override;

            Viewer* mViewer;
        };

        class SwapBuffersCallback : public osg::GraphicsContext::SwapCallback
        {
        public:
            SwapBuffersCallback(Viewer* viewer) : mViewer(viewer) {};
            void swapBuffersImplementation(osg::GraphicsContext* gc) override;

        private:
            Viewer* mViewer;
        };

        class PredrawCallback : public Misc::CallbackManager::DrawCallback
        {
        public:
            PredrawCallback(Viewer* viewer)
                : mViewer(viewer)
            {}

            void run(osg::RenderInfo& info, Misc::CallbackManager::View view) const override { mViewer->preDrawCallback(info, view); };

        private:

            Viewer* mViewer;
        };

        class PostdrawCallback : public Misc::CallbackManager::DrawCallback
        {
        public:
            PostdrawCallback(Viewer* viewer)
                : mViewer(viewer)
            {}

            void run(osg::RenderInfo& info, Misc::CallbackManager::View view) const override { mViewer->postDrawCallback(info, view); };

        private:

            Viewer* mViewer;
        };

        class InitialDrawCallback : public Misc::CallbackManager::DrawCallback
        {
        public:
            InitialDrawCallback(Viewer* viewer)
                : mViewer(viewer)
            {}

            void run(osg::RenderInfo& info, Misc::CallbackManager::View view) const override { mViewer->initialDrawCallback(info, view); };

        private:

            Viewer* mViewer;
        };

        class FinaldrawCallback : public Misc::CallbackManager::DrawCallback
        {
        public:
            FinaldrawCallback(Viewer* viewer)
                : mViewer(viewer)
            {}

            void run(osg::RenderInfo& info, Misc::CallbackManager::View view) const override { mViewer->finalDrawCallback(info, view); };

        private:

            Viewer* mViewer;
        };

        enum class MirrorTextureEye
        {
            Left,
            Right,
            Both
        };

    public:
        Viewer(
            std::unique_ptr<VR::Session> session,
            osg::ref_ptr<osgViewer::Viewer> viewer);

        ~Viewer(void);

        void swapBuffersCallback(osg::GraphicsContext* gc);
        void initialDrawCallback(osg::RenderInfo& info, Misc::CallbackManager::View view);
        void preDrawCallback(osg::RenderInfo& info, Misc::CallbackManager::View view);
        void postDrawCallback(osg::RenderInfo& info, Misc::CallbackManager::View view);
        void finalDrawCallback(osg::RenderInfo& info, Misc::CallbackManager::View view);
        void blit(osg::RenderInfo& gc);
        void configureCallbacks();
        void setupMirrorTexture();
        void processChangedSettings(const std::set< std::pair<std::string, std::string> >& changed);
        void updateView(Misc::View& left, Misc::View& right);

        bool callbacksConfigured() { return mCallbacksConfigured; };

        bool applyGamma(osg::RenderInfo& info);

    private:
        osg::ref_ptr<osg::FrameBufferObject> getXrFramebuffer(uint32_t view, osg::State* state);
        void blitXrFramebuffers(osg::State* state);
        void blitMirrorTexture(osg::State* state);
        void resolveMSAA(osg::State* state);
        void resolveGamma(osg::RenderInfo& info);

    private:
        std::mutex mMutex{};
        bool mCallbacksConfigured{ false };

        std::unique_ptr<VR::Session> mSession;
        osg::ref_ptr<osgViewer::Viewer> mViewer;
        std::shared_ptr<InitialDrawCallback> mInitialDraw{ nullptr };
        std::shared_ptr<PredrawCallback> mPreDraw{ nullptr };
        std::shared_ptr<PostdrawCallback> mPostDraw{ nullptr };
        std::shared_ptr<FinaldrawCallback> mFinalDraw{ nullptr };
        std::shared_ptr<UpdateViewCallback> mUpdateViewCallback{ nullptr };

        osg::ref_ptr<osg::FrameBufferObject> mMirrorFramebuffer;
        std::vector<VR::Side> mMirrorTextureViews;
        bool mMirrorTextureShouldBeCleanedUp{ false };
        bool mMirrorTextureEnabled{ false };
        bool mFlipMirrorTextureOrder{ false };
        MirrorTextureEye mMirrorTextureEye{ MirrorTextureEye::Both };

        osg::ref_ptr<osg::FrameBufferObject> mDrawFramebuffer;
        osg::ref_ptr<osg::FrameBufferObject> mMsaaResolveFramebuffer;
        osg::ref_ptr<osg::FrameBufferObject> mGammaResolveFramebuffer;
        osg::ref_ptr<osg::Texture2D> mMsaaResolveTexture;
        int mFramebufferWidth = 0;
        int mFramebufferHeight = 0;

        std::array<std::shared_ptr<VR::Swapchain>, 2> mColorSwapchain;
        std::array<std::shared_ptr<VR::Swapchain>, 2> mDepthSwapchain;
        std::array<VR::SubImage, 2> mSubImages;

        std::map<uint64_t, osg::ref_ptr<osg::FrameBufferObject> > mSwapchainFramebuffers;

        std::queue<VR::Frame> mReadyFrames;
        VR::Frame mDrawFrame;
    };
}


#endif
