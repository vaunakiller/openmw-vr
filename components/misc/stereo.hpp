#ifndef MISC_STEREO_H
#define MISC_STEREO_H

#include <osg/Matrix>
#include <osg/Vec3>
#include <osg/Camera>
#include <osg/StateSet>

#include <memory>
#include <array>

#include <components/sceneutil/mwshadowtechnique.hpp>

// Some cursed headers like to define these
#if defined(near) || defined(far)
#undef near
#undef far
#endif

namespace osg
{
    class FrameBufferObject;
    class Texture2DArray;
}

namespace osgViewer
{
    class Viewer;
}

namespace Misc
{
    class StereoFramebuffer
    {
    public:
        StereoFramebuffer(int width, int height, int samples);
        ~StereoFramebuffer();

        void attachColorComponent(GLint internalFormat);
        void attachDepthComponent(GLint internalFormat);

        osg::FrameBufferObject* layeredFbo();
        osg::FrameBufferObject* unlayeredFbo(int i);

        enum class Attachment
        {
            Layered, Left, Right
        };
        void attachTo(osg::Camera* camera, Attachment attachment);

    private:
        osg::Texture2DArray* createTextureArray(GLint internalFormat);

        int mWidth;
        int mHeight;
        int mSamples;
        osg::ref_ptr<osg::FrameBufferObject> mLayeredFbo;
        std::array<osg::ref_ptr<osg::FrameBufferObject>, 2> mUnlayeredFbo;
        osg::ref_ptr<osg::Texture2DArray> mColorTextureArray;
        osg::ref_ptr<osg::Texture2DArray> mDepthTextureArray;
    };


    //! Represents the relative pose in space of some object
    struct Pose
    {
        //! Position in space
        osg::Vec3 position{ 0,0,0 };
        //! Orientation in space.
        osg::Quat orientation{ 0,0,0,1 };

        //! Add one pose to another
        Pose operator+(const Pose& rhs);
        const Pose& operator+=(const Pose& rhs);

        //! Scale a pose (does not affect orientation)
        Pose operator*(float scalar);
        const Pose& operator*=(float scalar);
        Pose operator/(float scalar);
        const Pose& operator/=(float scalar);

        bool operator==(const Pose& rhs) const;

        osg::Matrix viewMatrix(bool useGLConventions);
    };

    //! Fov that defines all 4 angles from center
    struct FieldOfView {
        float    angleLeft{ 0.f };
        float    angleRight{ 0.f };
        float    angleUp{ 0.f };
        float    angleDown{ 0.f };

        bool operator==(const FieldOfView& rhs) const;

        //! Generate a perspective matrix from this fov
        osg::Matrix perspectiveMatrix(float near, float far) const;
    };

    //! Represents an eye including both pose and fov.
    struct View
    {
        Pose pose;
        FieldOfView fov;
        bool operator==(const View& rhs) const;
    };

    //! Represent two eyes. The eyes are in relative terms, and are assumed to lie on the horizon plane.
    struct StereoView
    {
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

        enum class Technique
        {
            None = 0, //!< Stereo disabled (do nothing).
            BruteForce, //!< Two slave cameras culling and drawing everything.
            OVR_MultiView2, //!< Frustum camera culls and draws stereo into indexed viewports using an automatically generated geometry shader.
        };

        static StereoView& instance();

        //! Adds two cameras in stereo to the mainCamera.
        //! All nodes matching the mask are rendered in stereo using brute force via two camera transforms, the rest are rendered in stereo via a geometry shader.
        //! \param noShaderMask mask in all nodes that do not use shaders and must be rendered brute force.
        //! \param sceneMask must equal MWRender::VisMask::Mask_Scene. Necessary while VisMask is still not in components/
        //! \note the masks apply only to the GeometryShader_IndexdViewports technique and can be 0 for the BruteForce technique.
        StereoView();

        //! Updates uniforms with the view and projection matrices of each stereo view, and replaces the camera's view and projection matrix
        //! with a view and projection that closely envelopes the frustums of the two eyes.
        void update();
        void updateStateset(osg::StateSet* stateset);

        void initializeStereo(osgViewer::Viewer* viewer);

        //! Callback that updates stereo configuration during the update pass
        void setUpdateViewCallback(std::shared_ptr<UpdateViewCallback> cb);

        //! Set the cull callback on the appropriate camera object
        void setCullCallback(osg::ref_ptr<osg::NodeCallback> cb);

        osg::Matrixd computeLeftEyeProjection(const osg::Matrixd& projection) const;
        osg::Matrixd computeLeftEyeView(const osg::Matrixd& view) const;

        osg::Matrixd computeRightEyeProjection(const osg::Matrixd& projection) const;
        osg::Matrixd computeRightEyeView(const osg::Matrixd& view) const;

        Technique getTechnique() const { return mTechnique; };

        void shaderStereoDefines(Shader::ShaderManager::DefineMap& defines) const;

        void setStereoFramebuffer(std::shared_ptr<StereoFramebuffer> fbo);

        const std::string& error() const;

    private:
        Technique stereoTechniqueFromSettings(void);
        void setupBruteForceTechnique();
        void setupOVRMultiView2Technique();
        void setupSharedShadows();

        osg::ref_ptr<osgViewer::Viewer> mViewer;
        osg::ref_ptr<osg::Camera>       mMainCamera;
        osg::ref_ptr<osg::Group>        mRoot;
        osg::ref_ptr<osg::Group>        mStereoRoot;
        osg::ref_ptr<osg::Callback>     mUpdateCallback;
        Technique                       mTechnique;
        std::string                     mError;

        // Stereo matrices
        osg::Matrix                 mLeftViewMatrix;
        osg::Matrix                 mLeftViewOffsetMatrix;
        osg::Matrix                 mLeftProjectionMatrix;
        osg::Matrix                 mRightViewMatrix;
        osg::Matrix                 mRightViewOffsetMatrix;
        osg::Matrix                 mRightProjectionMatrix;

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

        std::shared_ptr<StereoFramebuffer> mStereoFramebuffer = nullptr;
    };

    //! Reads settings to determine stereo technique
    StereoView::Technique getStereoTechnique(void);
}

#endif
