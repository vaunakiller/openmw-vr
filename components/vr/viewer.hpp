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
    struct InitialDrawCallback;
    struct PredrawCallback;
    struct FinaldrawCallback;
    struct UpdateViewCallback;
    struct SwapBuffersCallback;

    /// \brief Manages stereo rendering and mirror texturing.
    ///
    /// Manipulates the osgViewer by disabling main camera rendering, and instead rendering to
    /// two slave cameras, each connected to and manipulated by a VRView class.
    class Viewer
    {
    public:
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
        void blitXrFramebuffer(osg::State* state, int i);
        void blitMirrorTexture(osg::State* state, int i);
        void resolveMSAA(osg::State* state, osg::FrameBufferObject* fbo);
        void resolveGamma(osg::RenderInfo& info);

    private:
        std::mutex mMutex{};

        std::unique_ptr<VR::Session> mSession;
        osg::ref_ptr<osgViewer::Viewer> mViewer;
        osg::ref_ptr<SwapBuffersCallback> mSwapBuffersCallback;
        std::shared_ptr<InitialDrawCallback> mInitialDraw;
        std::shared_ptr<PredrawCallback> mPreDraw;
        std::shared_ptr<FinaldrawCallback> mFinalDraw;
        std::shared_ptr<UpdateViewCallback> mUpdateViewCallback;
        bool mCallbacksConfigured{ false };

        osg::ref_ptr<osg::FrameBufferObject> mMirrorFramebuffer;
        std::vector<VR::Side> mMirrorTextureViews;
        bool mMirrorTextureShouldBeCleanedUp{ false };
        bool mMirrorTextureEnabled{ false };
        bool mFlipMirrorTextureOrder{ false };
        MirrorTextureEye mMirrorTextureEye{ MirrorTextureEye::Both };

        std::shared_ptr<Misc::StereoFramebuffer> mStereoFramebuffer;
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
