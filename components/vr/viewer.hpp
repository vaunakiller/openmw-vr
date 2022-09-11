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
#include <components/stereo/multiview.hpp>
#include <components/misc/callbackmanager.hpp>
#include <components/vr/constants.hpp>
#include <components/vr/frame.hpp>
#include <components/vr/layer.hpp>
#include <components/vr/trackingpath.hpp>

namespace osg
{
    class Transform;
}

namespace Stereo
{
    class MultiviewFramebuffer;
    struct View;
}

namespace VR
{
    class Swapchain;
    class TrackingManager;
    class Session;
    struct InitialDrawCallback;
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
        static Viewer& instance();

        Viewer(
            std::shared_ptr<VR::Session> session,
            osg::ref_ptr<osgViewer::Viewer> viewer);

        ~Viewer(void);

        void swapBuffersCallback(osg::GraphicsContext* gc);
        void initialDrawCallback(osg::RenderInfo& info, Misc::CallbackManager::View view);
        void finalDrawCallback(osg::RenderInfo& info, Misc::CallbackManager::View view);
        void blit(osg::RenderInfo& gc);
        void configureCallbacks();
        void setupMirrorTexture();
        void processChangedSettings(const std::set< std::pair<std::string, std::string> >& changed);
        void updateView(Stereo::View& left, Stereo::View& right);

        bool callbacksConfigured() { return mCallbacksConfigured; };

        bool applyGamma(osg::RenderInfo& info, int i);

        osg::ref_ptr<osg::FrameBufferObject> getFboForView(Stereo::Eye view);

        void submitDepthForView(osg::State& state, osg::FrameBufferObject* fbo, Stereo::Eye view);

        osg::Transform* getTrackingNode(const std::string& path);
        osg::Transform* getTrackingNode(VR::VRPath path);
        osg::Group* getTrackersRoot() { return  mTrackersRoot.get(); }

    private:
        osg::ref_ptr<osg::FrameBufferObject> getXrFramebuffer(uint32_t view, osg::State* state);
        void blitXrFramebuffer(osg::State* state, int i);
        void blitMirrorTexture(osg::State* state, int i);
        void resolveGamma(osg::RenderInfo& info, int i);
        //std::shared_ptr<VR::Swapchain> colorSwapchain(int i);
        //std::shared_ptr<VR::Swapchain> depthSwapchain(int i);

    private:
        std::mutex mMutex{};

        osg::ref_ptr<osg::Group> mTrackersRoot;

        std::shared_ptr<VR::Session> mSession;
        osg::ref_ptr<osgViewer::Viewer> mViewer;
        osg::ref_ptr<SwapBuffersCallback> mSwapBuffersCallback;
        std::shared_ptr<InitialDrawCallback> mInitialDraw;
        std::shared_ptr<FinaldrawCallback> mFinalDraw;
        std::shared_ptr<UpdateViewCallback> mUpdateViewCallback;
        bool mCallbacksConfigured{ false };

        std::vector<VR::Side> mMirrorTextureViews;
        bool mMirrorTextureEnabled{ false };
        bool mFlipMirrorTextureOrder{ false };
        MirrorTextureEye mMirrorTextureEye{ MirrorTextureEye::Both };

        osg::ref_ptr<osg::FrameBufferObject> mGammaResolveFramebuffer;
        int mFramebufferWidth = 0;
        int mFramebufferHeight = 0;

        std::array<std::shared_ptr<VR::Swapchain>, 2> mColorSwapchain;
        std::array<std::shared_ptr<VR::Swapchain>, 2> mDepthSwapchain;
        std::array<VR::SubImage, 2> mSubImages;

        std::map< std::pair<uint64_t, uint32_t>, osg::ref_ptr<osg::FrameBufferObject> > mSwapchainFramebuffers;

        std::queue<VR::Frame> mReadyFrames;
        VR::Frame mDrawFrame;

        std::map<osg::FrameBufferObject*, std::unique_ptr<Stereo::MultiviewFramebufferResolve> > mMultiviewResolve;
    };
}


#endif
