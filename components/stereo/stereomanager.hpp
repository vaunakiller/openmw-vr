#ifndef STEREO_MANAGER_H
#define STEREO_MANAGER_H

#include <osg/Matrix>
#include <osg/Vec3>
#include <osg/Camera>
#include <osg/StateSet>

#include <memory>
#include <array>

#include <components/stereo/types.hpp>
#include <components/sceneutil/mwshadowtechnique.hpp>

namespace osg
{
    class FrameBufferObject;
    class Texture2D;
    class Texture2DMultisample;
    class Texture2DArray;
}

namespace osgViewer
{
    class Viewer;
}

namespace Stereo
{
    class StereoFramebuffer
    {
    public:
        StereoFramebuffer(int width, int height, int samples);
        ~StereoFramebuffer();

        void attachColorComponent(GLint sourceFormat, GLint sourceType, GLint internalFormat);
        void attachDepthComponent(GLint sourceFormat, GLint sourceType, GLint internalFormat);

        osg::FrameBufferObject* multiviewFbo();
        osg::FrameBufferObject* fbo(int i);

        void attachTo(osg::Camera* camera);

    private:
        osg::Texture2D* createTexture(GLint sourceFormat, GLint sourceType, GLint internalFormat);
        osg::Texture2DMultisample* createTextureMsaa(GLint sourceFormat, GLint sourceType, GLint internalFormat);
        osg::Texture2DArray* createTextureArray(GLint sourceFormat, GLint sourceType, GLint internalFormat);

        int mWidth;
        int mHeight;
        int mSamples;
        bool mMultiview;
        osg::ref_ptr<osg::FrameBufferObject> mMultiviewFbo;
        std::array<osg::ref_ptr<osg::FrameBufferObject>, 2> mFbo;
        osg::ref_ptr<osg::Texture2DArray> mMultiviewColorTexture;
        std::array<osg::ref_ptr<osg::Texture>, 2> mColorTexture;
        osg::ref_ptr<osg::Texture2DArray> mMultiviewDepthTexture;
        std::array<osg::ref_ptr<osg::Texture>, 2> mDepthTexture;
    };

    //! Represent two eyes. The eyes are in relative terms, and are assumed to lie on the horizon plane.
    class Manager
    {
    public:
        struct UpdateViewCallback
        {
            virtual ~UpdateViewCallback() = default;
                                
            //! Called during the update traversal of every frame to source updated stereo values.
            virtual void updateView(View& left, View& right) = 0;
        };

        //! Default implementation of UpdateViewCallback that just provides some hardcoded values for debugging purposes
        struct DefaultUpdateViewCallback : public UpdateViewCallback
        {
            void updateView(View& left, View& right) override;
        };

        static Manager& instance();

        //! Adds two cameras in stereo to the mainCamera.
        //! All nodes matching the mask are rendered in stereo using brute force via two camera transforms, the rest are rendered in stereo via a geometry shader.
        //! \param noShaderMask mask in all nodes that do not use shaders and must be rendered brute force.
        //! \param sceneMask must equal MWRender::VisMask::Mask_Scene. Necessary while VisMask is still not in components/
        //! \note the masks apply only to the GeometryShader_IndexdViewports technique and can be 0 for the BruteForce technique.
        Manager(osgViewer::Viewer* viewer);

        //! Updates uniforms with the view and projection matrices of each stereo view, and replaces the camera's view and projection matrix
        //! with a view and projection that closely envelopes the frustums of the two eyes.
        void update();
        void updateStateset(osg::StateSet* stateset);

        void initializeStereo(osg::GraphicsContext* gc);

        //! Callback that updates stereo configuration during the update pass
        void setUpdateViewCallback(std::shared_ptr<UpdateViewCallback> cb);

        //! Set the cull callback on the appropriate camera object
        void setCullCallback(osg::ref_ptr<osg::NodeCallback> cb);

        osg::Matrixd computeLeftEyeProjection(bool allowReverseZ) const;
        osg::Matrixd computeLeftEyeView() const;

        osg::Matrixd computeRightEyeProjection(bool allowReverseZ) const;
        osg::Matrixd computeRightEyeView() const;

        void shaderStereoDefines(Shader::ShaderManager::DefineMap& defines) const;

        void setStereoFramebuffer(std::shared_ptr<StereoFramebuffer> fbo);

        const std::string& error() const;

    private:
        void setupBruteForceTechnique();
        void setupOVRMultiView2Technique();
        void setupSharedShadows();

        osg::ref_ptr<osgViewer::Viewer> mViewer;
        osg::ref_ptr<osg::Camera>       mMainCamera;
        osg::ref_ptr<osg::Group>        mRoot;
        osg::ref_ptr<osg::Group>        mStereoRoot;
        osg::ref_ptr<osg::Callback>     mUpdateCallback;
        std::string                     mError;
        bool                            mMultiview;

        // Stereo matrices
        View                        mLeftView;
        osg::Matrix                 mLeftViewMatrix;
        osg::Matrix                 mLeftViewOffsetMatrix;
        View                        mRightView;
        osg::Matrix                 mRightViewMatrix;
        osg::Matrix                 mRightViewOffsetMatrix;

        // Keeps state relevant to OVR_MultiView2
        osg::ref_ptr<osg::Group>    mStereoShaderRoot = new osg::Group;

        osg::ref_ptr<osg::FrameBufferObject> mLayeredFbo;
        osg::ref_ptr<osg::FrameBufferObject> mLeftFbo;
        osg::ref_ptr<osg::FrameBufferObject> mRightFbo;

        using SharedShadowMapConfig = SceneUtil::MWShadowTechnique::SharedShadowMapConfig;
        osg::ref_ptr<SharedShadowMapConfig> mMasterConfig;
        osg::ref_ptr<SharedShadowMapConfig> mSlaveConfig;
        bool                                mSharedShadowMaps;

        // Updates stereo configuration during the update pass
        std::shared_ptr<UpdateViewCallback> mUpdateViewCallback;

        // OSG camera callbacks set using set*callback. StereoView manages that these are always set on the appropriate camera(s);
        osg::ref_ptr<osg::NodeCallback>         mCullCallback = nullptr;
    };
}

#endif
