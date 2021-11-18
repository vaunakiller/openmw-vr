#include "stereo.hpp"
#include "stringops.hpp"
#include "callbackmanager.hpp"

#include <osg/io_utils>
#include <osg/Texture2DArray>

#include <osgUtil/CullVisitor>
#include <osgUtil/RenderStage>

#include <osgViewer/Renderer>
#include <osgViewer/Viewer>

#include <iostream>
#include <map>
#include <string>

#include <components/debug/debuglog.hpp>

#include <components/sceneutil/statesetupdater.hpp>
#include <components/sceneutil/visitor.hpp>

#include <components/settings/settings.hpp>

namespace Misc
{

    Pose Pose::operator+(const Pose& rhs)
    {
        Pose pose = *this;
        pose.position += this->orientation * rhs.position;
        pose.orientation = rhs.orientation * this->orientation;
        return pose;
    }

    const Pose& Pose::operator+=(const Pose& rhs)
    {
        *this = *this + rhs;
        return *this;
    }

    Pose Pose::operator*(float scalar)
    {
        Pose pose = *this;
        pose.position *= scalar;
        return pose;
    }

    const Pose& Pose::operator*=(float scalar)
    {
        *this = *this * scalar;
        return *this;
    }

    Pose Pose::operator/(float scalar)
    {
        Pose pose = *this;
        pose.position /= scalar;
        return pose;
    }
    const Pose& Pose::operator/=(float scalar)
    {
        *this = *this / scalar;
        return *this;
    }

    bool Pose::operator==(const Pose& rhs) const
    {
        return position == rhs.position && orientation == rhs.orientation;
    }

    osg::Matrix Pose::viewMatrix(bool useGLConventions)
    {
        if (useGLConventions)
        {
            // When applied as an offset to an existing view matrix,
            // that view matrix will already convert points to a camera space
            // with opengl conventions. So we need to convert offsets to opengl 
            // conventions.
            float y = position.y();
            float z = position.z();
            position.y() = z;
            position.z() = -y;

            y = orientation.y();
            z = orientation.z();
            orientation.y() = z;
            orientation.z() = -y;

            osg::Matrix viewMatrix;
            viewMatrix.setTrans(-position);
            viewMatrix.postMultRotate(orientation.conj());
            return viewMatrix;
        }
        else
        {
            osg::Vec3d forward = orientation * osg::Vec3d(0, 1, 0);
            osg::Vec3d up = orientation * osg::Vec3d(0, 0, 1);
            osg::Matrix viewMatrix;
            viewMatrix.makeLookAt(position, position + forward, up);

            return viewMatrix;
        }
    }

    bool FieldOfView::operator==(const FieldOfView& rhs) const
    {
        return angleDown == rhs.angleDown
            && angleUp == rhs.angleUp
            && angleLeft == rhs.angleLeft
            && angleRight == rhs.angleRight;
    }

    // near and far named with an underscore because of windows' headers galaxy brain defines.
    osg::Matrix FieldOfView::perspectiveMatrix(float near_, float far_) const
    {
        const float tanLeft = tanf(angleLeft);
        const float tanRight = tanf(angleRight);
        const float tanDown = tanf(angleDown);
        const float tanUp = tanf(angleUp);

        const float tanWidth = tanRight - tanLeft;
        const float tanHeight = tanUp - tanDown;

        const float offset = near_;

        float matrix[16] = {};

        matrix[0] = 2 / tanWidth;
        matrix[4] = 0;
        matrix[8] = (tanRight + tanLeft) / tanWidth;
        matrix[12] = 0;

        matrix[1] = 0;
        matrix[5] = 2 / tanHeight;
        matrix[9] = (tanUp + tanDown) / tanHeight;
        matrix[13] = 0;

        if (far_ <= near_) {
            matrix[2] = 0;
            matrix[6] = 0;
            matrix[10] = -1;
            matrix[14] = -(near_ + offset);
        }
        else {
            matrix[2] = 0;
            matrix[6] = 0;
            matrix[10] = -(far_ + offset) / (far_ - near_);
            matrix[14] = -(far_ * (near_ + offset)) / (far_ - near_);
        }

        matrix[3] = 0;
        matrix[7] = 0;
        matrix[11] = -1;
        matrix[15] = 0;

        return osg::Matrix(matrix);
    }

    bool View::operator==(const View& rhs) const
    {
        return pose == rhs.pose && fov == rhs.fov;
    }

    std::ostream& operator <<(
        std::ostream& os,
        const Pose& pose)
    {
        os << "position=" << pose.position << ", orientation=" << pose.orientation;
        return os;
    }

    std::ostream& operator <<(
        std::ostream& os,
        const FieldOfView& fov)
    {
        os << "left=" << fov.angleLeft << ", right=" << fov.angleRight << ", down=" << fov.angleDown << ", up=" << fov.angleUp;
        return os;
    }

    std::ostream& operator <<(
        std::ostream& os,
        const View& view)
    {
        os << "pose=< " << view.pose << " >, fov=< " << view.fov << " >";
        return os;
    }

    // Update stereo view/projection during update
    class StereoUpdateCallback : public osg::Callback
    {
    public:
        StereoUpdateCallback(StereoView* stereoView) : stereoView(stereoView) {}

        bool run(osg::Object* object, osg::Object* data) override
        {
            auto b = traverse(object, data);
            stereoView->update();
            return b;
        }

        StereoView* stereoView;
    };

    // Update states during cull
    class BruteForceStereoStatesetUpdateCallback : public SceneUtil::StateSetUpdater
    {
    public:
        BruteForceStereoStatesetUpdateCallback(StereoView* view)
            : stereoView(view)
        {
        }

    protected:
        virtual void setDefaults(osg::StateSet* stateset) override
        {
            stateset->addUniform(new osg::Uniform("projectionMatrix", osg::Matrixf{}));
        }

        virtual void apply(osg::StateSet* stateset, osg::NodeVisitor* /*nv*/) override
        {
        }

        void applyLeft(osg::StateSet* stateset, osgUtil::CullVisitor* nv) override
        {
            osg::Matrix dummy;
            auto* uProjectionMatrix = stateset->getUniform("projectionMatrix");
            if (uProjectionMatrix)
                uProjectionMatrix->set(stereoView->computeLeftEyeProjection(dummy));
        }

        void applyRight(osg::StateSet* stateset, osgUtil::CullVisitor* nv) override
        {
            osg::Matrix dummy;
            auto* uProjectionMatrix = stateset->getUniform("projectionMatrix");
            if (uProjectionMatrix)
                uProjectionMatrix->set(stereoView->computeRightEyeProjection(dummy));
        }

    private:
        StereoView* stereoView;
    };

    // Update states during cull
    class OVRMultiViewStereoStatesetUpdateCallback : public SceneUtil::StateSetUpdater
    {
    public:
        OVRMultiViewStereoStatesetUpdateCallback(StereoView* view)
            : stereoView(view)
        {
        }

    protected:
        virtual void setDefaults(osg::StateSet* stateset)
        {
            stateset->addUniform(new osg::Uniform(osg::Uniform::FLOAT_MAT4, "viewMatrixMultiView", 2));
            stateset->addUniform(new osg::Uniform(osg::Uniform::FLOAT_MAT4, "projectionMatrixMultiView", 2));
        }

        virtual void apply(osg::StateSet* stateset, osg::NodeVisitor* /*nv*/)
        {
            stereoView->updateStateset(stateset);
        }

    private:
        StereoView* stereoView;
    };

    static StereoView* sInstance = nullptr;

    StereoView& StereoView::instance()
    {
        return *sInstance;
    }

    StereoView::StereoView(osgViewer::Viewer* viewer)
        : mViewer(viewer)
        , mStereoRoot(new osg::Group)
        , mUpdateCallback(new StereoUpdateCallback(this))
        , mMultiview(false)
        , mMasterConfig(new SharedShadowMapConfig)
        , mSlaveConfig(new SharedShadowMapConfig)
        , mSharedShadowMaps(Settings::Manager::getBool("shared shadow maps", "Stereo"))
        , mUpdateViewCallback(new DefaultUpdateViewCallback)
    {
        if (sInstance)
            throw std::logic_error("Double instance of StereoView");
        sInstance = this;

        mMasterConfig->_id = "STEREO";
        mMasterConfig->_master = true;
        mSlaveConfig->_id = "STEREO";
        mSlaveConfig->_master = false;

        mStereoRoot->setName("Stereo Root");
        mStereoRoot->setDataVariance(osg::Object::STATIC);
    }

    void StereoView::initializeStereo(osg::GraphicsContext* gc)
    {
        mMainCamera = mViewer->getCamera();
        mRoot = mViewer->getSceneData()->asGroup();

        auto mainCameraCB = mMainCamera->getUpdateCallback();
        mMainCamera->removeUpdateCallback(mainCameraCB);
        mMainCamera->addUpdateCallback(mUpdateCallback);
        mMainCamera->addUpdateCallback(mainCameraCB);

        auto ci = gc->getState()->getContextID();
        mMultiview = Misc::getMultiview(ci);

        if(mMultiview)
            setupOVRMultiView2Technique();
        else
            setupBruteForceTechnique();
    }

    void StereoView::shaderStereoDefines(Shader::ShaderManager::DefineMap& defines) const
    {
        if (mMultiview)
        {
            defines["GLSLVersion"] = "330 compatibility";
            defines["useOVR_multiview"] = "1";
            defines["numViews"] = "2";
        }
        else
        {
            defines["useOVR_multiview"] = "0";
            defines["numViews"] = "1";
        }
    }

    void StereoView::setStereoFramebuffer(std::shared_ptr<StereoFramebuffer> fbo)
    {
        if (mMultiview)
            fbo->attachTo(mMainCamera, StereoFramebuffer::Attachment::Layered);
    }

    const std::string& StereoView::error() const
    {
        return mError;
    }

    void StereoView::setupBruteForceTechnique()
    {
        auto* ds = osg::DisplaySettings::instance().get();
        ds->setStereo(true);
        ds->setStereoMode(osg::DisplaySettings::StereoMode::HORIZONTAL_SPLIT);
        ds->setUseSceneViewForStereoHint(true);
        
        mStereoRoot->addChild(mRoot);
        mStereoRoot->addCullCallback(new BruteForceStereoStatesetUpdateCallback(this));
        mViewer->setSceneData(mStereoRoot);

        struct ComputeStereoMatricesCallback : public osgUtil::SceneView::ComputeStereoMatricesCallback
        {
            ComputeStereoMatricesCallback(StereoView* sv)
                : mStereoView(sv)
            {

            }

            osg::Matrixd computeLeftEyeProjection(const osg::Matrixd& projection) const override
            {
                return mStereoView->computeLeftEyeProjection(projection);
            }

            osg::Matrixd computeLeftEyeView(const osg::Matrixd& view) const override
            {
                return mStereoView->computeLeftEyeView(view);
            }

            osg::Matrixd computeRightEyeProjection(const osg::Matrixd& projection) const override
            {
                return mStereoView->computeRightEyeProjection(projection);
            }

            osg::Matrixd computeRightEyeView(const osg::Matrixd& view) const override
            {
                return mStereoView->computeRightEyeView(view);
            }

            StereoView* mStereoView;
        };

        auto* renderer = static_cast<osgViewer::Renderer*>(mMainCamera->getRenderer());
        for (auto* sceneView : { renderer->getSceneView(0), renderer->getSceneView(1) })
            sceneView->setComputeStereoMatricesCallback(new ComputeStereoMatricesCallback(this));

        setupSharedShadows();
    }

    void StereoView::setupOVRMultiView2Technique()
    {
        mStereoShaderRoot->addCullCallback(new OVRMultiViewStereoStatesetUpdateCallback(this));
        mStereoShaderRoot->addChild(mRoot);
        mStereoRoot->addChild(mStereoShaderRoot);

        // Inject self as the root of the scene graph
        mViewer->setSceneData(mStereoRoot);
    }

    void StereoView::setupSharedShadows()
    {
        auto* renderer = static_cast<osgViewer::Renderer*>(mMainCamera->getRenderer());

        // osgViewer::Renderer always has two scene views
        for (auto* sceneView : { renderer->getSceneView(0), renderer->getSceneView(1) })
        {
            if (mSharedShadowMaps)
            {
                sceneView->getCullVisitorLeft()->setUserData(mMasterConfig);
                sceneView->getCullVisitorRight()->setUserData(mSlaveConfig);
            }
        }
    }

    void StereoView::update()
    {
        View left{};
        View right{};
        double near_ = 1.f;
        double far_ = 10000.f;
        if (!mUpdateViewCallback)
        {
            Log(Debug::Error) << "StereoView: No update view callback. Stereo rendering will not work.";
            return;
        }
        mUpdateViewCallback->updateView(left, right);
        auto viewMatrix = mViewer->getCamera()->getViewMatrix();
        auto projectionMatrix = mViewer->getCamera()->getProjectionMatrix();
        near_ = Settings::Manager::getFloat("near clip", "Camera");
        far_ = Settings::Manager::getFloat("viewing distance", "Camera");

        osg::Vec3d leftEye = left.pose.position;
        osg::Vec3d rightEye = right.pose.position;

        mLeftViewOffsetMatrix = left.pose.viewMatrix(true);
        mRightViewOffsetMatrix = right.pose.viewMatrix(true);

        mLeftViewMatrix = viewMatrix * mLeftViewOffsetMatrix;
        mRightViewMatrix = viewMatrix * mRightViewOffsetMatrix;

        mLeftProjectionMatrix = left.fov.perspectiveMatrix(near_, far_);
        mRightProjectionMatrix = right.fov.perspectiveMatrix(near_, far_);


        // To correctly cull when drawing stereo using the geometry shader, the main camera must
        // draw a fake view+perspective that includes the full frustums of both the left and right eyes.
        // This frustum will be computed as a perspective frustum from a position P slightly behind the eyes L and R
        // where it creates the minimum frustum encompassing both eyes' frustums.
        // NOTE: I make an assumption that the eyes lie in a horizontal plane relative to the base view,
        // and lie mirrored around the Y axis (straight ahead).
        // Re-think this if that turns out to be a bad assumption.
        View frustumView;

        // Compute Frustum angles. A simple min/max.
        /* Example values for reference:
            Left:
            angleLeft   -0.767549932    float
            angleRight   0.620896876    float
            angleDown   -0.837898076    float
            angleUp	     0.726982594    float

            Right:
            angleLeft   -0.620896876    float
            angleRight   0.767549932    float
            angleDown   -0.837898076    float
            angleUp	     0.726982594    float
        */
        frustumView.fov.angleLeft = std::min(left.fov.angleLeft, right.fov.angleLeft);
        frustumView.fov.angleRight = std::max(left.fov.angleRight, right.fov.angleRight);
        frustumView.fov.angleDown = std::min(left.fov.angleDown, right.fov.angleDown);
        frustumView.fov.angleUp = std::max(left.fov.angleUp, right.fov.angleUp);

        // Check that the case works for this approach
        auto maxAngle = std::max(frustumView.fov.angleRight - frustumView.fov.angleLeft, frustumView.fov.angleUp - frustumView.fov.angleDown);
        if (maxAngle > osg::PI)
        {
            Log(Debug::Error) << "Total FOV exceeds 180 degrees. Case cannot be culled in single-pass VR. Disabling culling to cope. Consider switching to dual-pass VR.";
            mMainCamera->setCullingActive(false);
            return;
            // TODO: An explicit frustum projection could cope, so implement that later. Guarantee you there will be VR headsets with total horizontal fov > 180 in the future. Maybe already.
        }

        // Use the law of sines on the triangle spanning PLR to determine P
        double angleLeft = std::abs(frustumView.fov.angleLeft);
        double angleRight = std::abs(frustumView.fov.angleRight);
        double lengthRL = (rightEye - leftEye).length();
        double ratioRL = lengthRL / std::sin(osg::PI - angleLeft - angleRight);
        double lengthLP = ratioRL * std::sin(angleRight);

        osg::Vec3d directionLP = osg::Vec3(std::cos(-angleLeft), std::sin(-angleLeft), 0);
        osg::Vec3d LP = directionLP * lengthLP;
        frustumView.pose.position = leftEye + LP;
        //frustumView.pose.position.x() += 1000;

        // Base view position is 0.0, by definition.
        // The length of the vector P is therefore the required offset to near/far.
        auto nearFarOffset = frustumView.pose.position.length();

        // Generate the frustum matrices
        auto frustumViewMatrix = viewMatrix * frustumView.pose.viewMatrix(true);
        auto frustumProjectionMatrix = frustumView.fov.perspectiveMatrix(near_ + nearFarOffset, far_ + nearFarOffset);

        if (mMultiview)
        {
            mMainCamera->setViewMatrix(frustumViewMatrix);
            mMainCamera->setProjectionMatrix(frustumProjectionMatrix);
        }
        else
        {
            if (mMasterConfig->_projection == nullptr)
                mMasterConfig->_projection = new osg::RefMatrix;
            if (mMasterConfig->_modelView == nullptr)
                mMasterConfig->_modelView = new osg::RefMatrix;

            if (mSharedShadowMaps)
            {
                mMasterConfig->_referenceFrame = mMainCamera->getReferenceFrame();
                mMasterConfig->_modelView->set(frustumViewMatrix);
                mMasterConfig->_projection->set(frustumProjectionMatrix);
            }
        }
    }

    void StereoView::updateStateset(osg::StateSet* stateset)
    {
        // Update stereo uniforms
        auto * viewMatrixMultiViewUniform = stateset->getUniform("viewMatrixMultiView");
        auto * projectionMatrixMultiViewUniform = stateset->getUniform("projectionMatrixMultiView");
        
        viewMatrixMultiViewUniform->setElement(0, mLeftViewOffsetMatrix);
        viewMatrixMultiViewUniform->setElement(1, mRightViewOffsetMatrix);
        projectionMatrixMultiViewUniform->setElement(0, mLeftProjectionMatrix);
        projectionMatrixMultiViewUniform->setElement(1, mRightProjectionMatrix);
    }

    void StereoView::setUpdateViewCallback(std::shared_ptr<UpdateViewCallback> cb)
    {
        mUpdateViewCallback = cb;
    }

    void StereoView::DefaultUpdateViewCallback::updateView(View& left, View& right)
    {
        left.pose.position = osg::Vec3(-2.2, 0, 0);
        right.pose.position = osg::Vec3(2.2, 0, 0);
        left.fov = { -0.767549932, 0.620896876, 0.726982594, -0.837898076 };
        right.fov = { -0.620896876, 0.767549932, 0.726982594, -0.837898076 };
    }

    void StereoView::setCullCallback(osg::ref_ptr<osg::NodeCallback> cb)
    {
        mMainCamera->setCullCallback(cb);
    }
    osg::Matrixd StereoView::computeLeftEyeProjection(const osg::Matrixd& projection) const
    {
        return mLeftProjectionMatrix;
    }
    osg::Matrixd StereoView::computeLeftEyeView(const osg::Matrixd& view) const
    {
        return mLeftViewMatrix;
    }
    osg::Matrixd StereoView::computeRightEyeProjection(const osg::Matrixd& projection) const
    {
        return mRightProjectionMatrix;
    }
    osg::Matrixd StereoView::computeRightEyeView(const osg::Matrixd& view) const
    {
        return mRightViewMatrix;
    }

    StereoFramebuffer::StereoFramebuffer(int width, int height, int samples)
        : mWidth(width)
        , mHeight(height)
        , mSamples(samples)
        , mLayeredFbo(new osg::FrameBufferObject)
        , mUnlayeredFbo{ new osg::FrameBufferObject , new osg::FrameBufferObject }
        , mColorTextureArray()
        , mDepthTextureArray()
    {
    (void)mSamples;
    }

    StereoFramebuffer::~StereoFramebuffer()
    {
    }

    void StereoFramebuffer::attachColorComponent(GLint internalFormat)
    {
        mColorTextureArray = createTextureArray(internalFormat);
#ifdef OSG_HAS_MULTIVIEW
        mLayeredFbo->setAttachment(osg::Camera::COLOR_BUFFER, osg::FrameBufferAttachment(mColorTextureArray, osg::Camera::FACE_CONTROLLED_BY_MULTIVIEW_SHADER, 0));
#endif
        mUnlayeredFbo[0]->setAttachment(osg::Camera::COLOR_BUFFER, osg::FrameBufferAttachment(mColorTextureArray, 0, 0));
        mUnlayeredFbo[1]->setAttachment(osg::Camera::COLOR_BUFFER, osg::FrameBufferAttachment(mColorTextureArray, 1, 0));
    }

    void StereoFramebuffer::attachDepthComponent(GLint internalFormat)
    {
        mDepthTextureArray = createTextureArray(internalFormat);
#ifdef OSG_HAS_MULTIVIEW
        mLayeredFbo->setAttachment(osg::Camera::DEPTH_BUFFER, osg::FrameBufferAttachment(mDepthTextureArray, osg::Camera::FACE_CONTROLLED_BY_MULTIVIEW_SHADER, 0));
#endif
        mUnlayeredFbo[0]->setAttachment(osg::Camera::DEPTH_BUFFER, osg::FrameBufferAttachment(mDepthTextureArray, 0, 0));
        mUnlayeredFbo[1]->setAttachment(osg::Camera::DEPTH_BUFFER, osg::FrameBufferAttachment(mDepthTextureArray, 1, 0));
    }

    osg::FrameBufferObject* StereoFramebuffer::layeredFbo()
    {
        return mLayeredFbo;
    }

    osg::FrameBufferObject* StereoFramebuffer::unlayeredFbo(int i)
    {
        // Don't bother protecting, stl exception is informative enough
        return mUnlayeredFbo[i];
    }

    void StereoFramebuffer::attachTo(osg::Camera* camera, Attachment attachment)
    {
        if (mColorTextureArray)
        {
            unsigned int level = 0;
            switch (attachment)
            {
#ifdef OSG_HAS_MULTIVIEW
            case Attachment::Layered:
                level = osg::Camera::FACE_CONTROLLED_BY_MULTIVIEW_SHADER;
                break;
#endif
            case Attachment::Left:
                level = 0;
                break;
            case Attachment::Right:
                level = 1;
                break;
            default:
                break;
            }

            camera->attach(osg::Camera::COLOR_BUFFER, mColorTextureArray, 0, level);
            if (mDepthTextureArray)
                camera->attach(osg::Camera::DEPTH_BUFFER, mDepthTextureArray, 0, level);

            camera->setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT);
        }
    }

    osg::Texture2DArray* StereoFramebuffer::createTextureArray(GLint internalFormat)
    {
        osg::Texture2DArray* textureArray = new osg::Texture2DArray;
        textureArray->setTextureSize(mWidth, mHeight, 2);
        textureArray->setInternalFormat(internalFormat);
        textureArray->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR);
        textureArray->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
        textureArray->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE);
        textureArray->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE);
        textureArray->setWrap(osg::Texture::WRAP_R, osg::Texture::CLAMP_TO_EDGE);
        return textureArray;
    }

    namespace
    {
        bool getMultiviewSupported(unsigned int contextID)
        {
#ifdef OSG_HAS_MULTIVIEW
            if (!Settings::Manager::getBool("multiview", "Stereo"))
            {
                Log(Debug::Verbose) << "Disabling Multiview (disabled by config)";
                return false;
            }

            if (!osg::isGLExtensionSupported(contextID, "GL_OVR_multiview"))
            {
                Log(Debug::Verbose) << "Disabling Multiview (opengl extension \"GL_OVR_multiview\" not supported)";
                return false;
            }

            if (!osg::isGLExtensionSupported(contextID, "GL_OVR_multiview2"))
            {
                Log(Debug::Verbose) << "Disabling Multiview (opengl extension \"GL_OVR_multiview2\" not supported)";
                return false;
            }

            if (!osg::isGLExtensionOrVersionSupported(contextID, "ARB_texture_view", 4.3))
            {
                Log(Debug::Verbose) << "Disabling Multiview (opengl extension \"ARB_texture_view\" not supported)";
                return false;
            }

            return true;
#else
            return false;
#endif
        }
    }

    bool getMultiview(unsigned int contextID)
    {
        static bool multiView = getMultiviewSupported(contextID);
        return multiView;
    }
}
