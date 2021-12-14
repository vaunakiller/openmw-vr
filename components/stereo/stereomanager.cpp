#include "stereomanager.hpp"
#include "multiview.hpp"

#include <osg/io_utils>
#include <osg/Texture2D>
#include <osg/Texture2DMultisample>
#include <osg/Texture2DArray>
#include <osg/DisplaySettings>

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
#include <components/sceneutil/util.hpp>

#include <components/settings/settings.hpp>

#include <components/misc/stringops.hpp>
#include <components/misc/callbackmanager.hpp>

namespace Stereo
{
    // Update stereo view/projection during update
    class StereoUpdateCallback : public osg::Callback
    {
    public:
        StereoUpdateCallback(Manager* stereoView) : stereoView(stereoView) {}

        bool run(osg::Object* object, osg::Object* data) override
        {
            auto b = traverse(object, data);
            stereoView->update();
            return b;
        }

        Manager* stereoView;
    };

    // Update states during cull
    class BruteForceStereoStatesetUpdateCallback : public SceneUtil::StateSetUpdater
    {
    public:
        BruteForceStereoStatesetUpdateCallback(Manager* manager)
            : mManager(manager)
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
                uProjectionMatrix->set(mManager->computeLeftEyeProjection(true));
        }

        void applyRight(osg::StateSet* stateset, osgUtil::CullVisitor* nv) override
        {
            osg::Matrix dummy;
            auto* uProjectionMatrix = stateset->getUniform("projectionMatrix");
            if (uProjectionMatrix)
                uProjectionMatrix->set(mManager->computeRightEyeProjection(true));
        }

    private:
        Manager* mManager;
    };

    // Update states during cull
    class OVRMultiViewStereoStatesetUpdateCallback : public SceneUtil::StateSetUpdater
    {
    public:
        OVRMultiViewStereoStatesetUpdateCallback(Manager* manager)
            : mManager(manager)
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
            mManager->updateStateset(stateset);
        }

    private:
        Manager* mManager;
    };

    static Manager* sInstance = nullptr;

    Manager& Manager::instance()
    {
        return *sInstance;
    }

    Manager::Manager(osgViewer::Viewer* viewer)
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
            throw std::logic_error("Double instance of Stereo::Manager");
        sInstance = this;

        mMasterConfig->_id = "STEREO";
        mMasterConfig->_master = true;
        mSlaveConfig->_id = "STEREO";
        mSlaveConfig->_master = false;

        mStereoRoot->setName("Stereo Root");
        mStereoRoot->setDataVariance(osg::Object::STATIC);
    }

    void Manager::initializeStereo(osg::GraphicsContext* gc)
    {
        mMainCamera = mViewer->getCamera();
        mRoot = mViewer->getSceneData()->asGroup();

        auto mainCameraCB = mMainCamera->getUpdateCallback();
        mMainCamera->removeUpdateCallback(mainCameraCB);
        mMainCamera->addUpdateCallback(mUpdateCallback);
        mMainCamera->addUpdateCallback(mainCameraCB);

        auto ci = gc->getState()->getContextID();
        mMultiview = Stereo::getMultiview(ci);

        if(mMultiview)
            setupOVRMultiView2Technique();
        else
            setupBruteForceTechnique();
    }

    void Manager::shaderStereoDefines(Shader::ShaderManager::DefineMap& defines) const
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

    void Manager::setStereoFramebuffer(std::shared_ptr<StereoFramebuffer> fbo)
    {
        fbo->attachTo(mMainCamera);
    }

    const std::string& Manager::error() const
    {
        return mError;
    }

    void Manager::setupBruteForceTechnique()
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
            ComputeStereoMatricesCallback(Manager* sv)
                : mManager(sv)
            {

            }

            osg::Matrixd computeLeftEyeProjection(const osg::Matrixd& projection) const override
            {
                (void)projection;
                return mManager->computeLeftEyeProjection(false);
            }

            osg::Matrixd computeLeftEyeView(const osg::Matrixd& view) const override
            {
                (void)view;
                return mManager->computeLeftEyeView();
            }

            osg::Matrixd computeRightEyeProjection(const osg::Matrixd& projection) const override
            {
                (void)projection;
                return mManager->computeRightEyeProjection(false);
            }

            osg::Matrixd computeRightEyeView(const osg::Matrixd& view) const override
            {
                (void)view;
                return mManager->computeRightEyeView();
            }

            Manager* mManager;
        };

        auto* renderer = static_cast<osgViewer::Renderer*>(mMainCamera->getRenderer());
        for (auto* sceneView : { renderer->getSceneView(0), renderer->getSceneView(1) })
            sceneView->setComputeStereoMatricesCallback(new ComputeStereoMatricesCallback(this));

        setupSharedShadows();
    }

    void Manager::setupOVRMultiView2Technique()
    {
#ifdef OSG_MADSBUVI_DISPLAY_LIST_PATCH
        if (Settings::Manager::getBool("disable display lists for multiview", "Stereo"))
        {
            auto* ds = osg::DisplaySettings::instance().get();
            ds->setDisplayListHint(osg::DisplaySettings::DISPLAYLIST_DISABLED);
            Log(Debug::Verbose) << "Disabling display lists";
        }
#endif

        mStereoShaderRoot->addCullCallback(new OVRMultiViewStereoStatesetUpdateCallback(this));
        mStereoShaderRoot->addChild(mRoot);
        mStereoRoot->addChild(mStereoShaderRoot);

        // Inject self as the root of the scene graph
        mViewer->setSceneData(mStereoRoot);
    }

    void Manager::setupSharedShadows()
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

    void Manager::update()
    {
        double near_ = 1.f;
        double far_ = 10000.f;
        if (!mUpdateViewCallback)
        {
            Log(Debug::Error) << "Manager: No update view callback. Stereo rendering will not work.";
            return;
        }
        mUpdateViewCallback->updateView(mLeftView, mRightView);
        auto viewMatrix = mViewer->getCamera()->getViewMatrix();
        auto projectionMatrix = mViewer->getCamera()->getProjectionMatrix();
        near_ = Settings::Manager::getFloat("near clip", "Camera");
        far_ = Settings::Manager::getFloat("viewing distance", "Camera");

        osg::Vec3d leftEye = mLeftView.pose.position;
        osg::Vec3d rightEye = mRightView.pose.position;

        mLeftViewOffsetMatrix = mLeftView.pose.viewMatrix(true);
        mRightViewOffsetMatrix = mRightView.pose.viewMatrix(true);

        mLeftViewMatrix = viewMatrix * mLeftViewOffsetMatrix;
        mRightViewMatrix = viewMatrix * mRightViewOffsetMatrix;


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
        frustumView.fov.angleLeft = std::min(mLeftView.fov.angleLeft, mRightView.fov.angleLeft);
        frustumView.fov.angleRight = std::max(mLeftView.fov.angleRight, mRightView.fov.angleRight);
        frustumView.fov.angleDown = std::min(mLeftView.fov.angleDown, mRightView.fov.angleDown);
        frustumView.fov.angleUp = std::max(mLeftView.fov.angleUp, mRightView.fov.angleUp);

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
        auto frustumProjectionMatrix = frustumView.fov.perspectiveMatrix(near_ + nearFarOffset, far_ + nearFarOffset, false);

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

    void Manager::updateStateset(osg::StateSet* stateset)
    {
        // Update stereo uniforms
        auto * viewMatrixMultiViewUniform = stateset->getUniform("viewMatrixMultiView");
        auto * projectionMatrixMultiViewUniform = stateset->getUniform("projectionMatrixMultiView");
        auto near_ = Settings::Manager::getFloat("near clip", "Camera");
        auto far_ = Settings::Manager::getFloat("viewing distance", "Camera");
        
        viewMatrixMultiViewUniform->setElement(0, mLeftViewOffsetMatrix);
        viewMatrixMultiViewUniform->setElement(1, mRightViewOffsetMatrix);
        projectionMatrixMultiViewUniform->setElement(0, mLeftView.fov.perspectiveMatrix(near_, far_, SceneUtil::getReverseZ()));
        projectionMatrixMultiViewUniform->setElement(1, mRightView.fov.perspectiveMatrix(near_, far_, SceneUtil::getReverseZ()));
    }

    void Manager::setUpdateViewCallback(std::shared_ptr<UpdateViewCallback> cb)
    {
        mUpdateViewCallback = cb;
    }

    void Manager::DefaultUpdateViewCallback::updateView(View& left, View& right)
    {
        left.pose.position = osg::Vec3(-2.2, 0, 0);
        right.pose.position = osg::Vec3(2.2, 0, 0);
        left.fov = { -0.767549932, 0.620896876, 0.726982594, -0.837898076 };
        right.fov = { -0.620896876, 0.767549932, 0.726982594, -0.837898076 };
    }

    void Manager::setCullCallback(osg::ref_ptr<osg::NodeCallback> cb)
    {
        mMainCamera->setCullCallback(cb);
    }
    osg::Matrixd Manager::computeLeftEyeProjection(bool allowReverseZ) const
    {
        auto near_ = Settings::Manager::getFloat("near clip", "Camera");
        auto far_ = Settings::Manager::getFloat("viewing distance", "Camera");
        return mLeftView.fov.perspectiveMatrix(near_, far_, allowReverseZ && SceneUtil::getReverseZ());
    }
    osg::Matrixd Manager::computeLeftEyeView() const
    {
        return mLeftViewMatrix;
    }
    osg::Matrixd Manager::computeRightEyeProjection(bool allowReverseZ) const
    {
        auto near_ = Settings::Manager::getFloat("near clip", "Camera");
        auto far_ = Settings::Manager::getFloat("viewing distance", "Camera");
        return mRightView.fov.perspectiveMatrix(near_, far_, allowReverseZ && SceneUtil::getReverseZ());
    }
    osg::Matrixd Manager::computeRightEyeView() const
    {
        return mRightViewMatrix;
    }

    StereoFramebuffer::StereoFramebuffer(int width, int height, int samples)
        : mWidth(width)
        , mHeight(height)
        , mSamples(samples)
        , mMultiview(getMultiview())
        , mMultiviewFbo{ new osg::FrameBufferObject }
        , mFbo{ new osg::FrameBufferObject, new osg::FrameBufferObject }
    {
    }

    StereoFramebuffer::~StereoFramebuffer()
    {
    }

    void StereoFramebuffer::attachColorComponent(GLint sourceFormat, GLint sourceType, GLint internalFormat)
    {
        if (mMultiview)
        {
#ifdef OSG_HAS_MULTIVIEW
            mMultiviewColorTexture = createTextureArray(sourceFormat, sourceType, internalFormat);
            mMultiviewFbo->setAttachment(osg::Camera::COLOR_BUFFER, osg::FrameBufferAttachment(mMultiviewColorTexture, osg::Camera::FACE_CONTROLLED_BY_MULTIVIEW_SHADER, 0));
            mFbo[0]->setAttachment(osg::Camera::COLOR_BUFFER, osg::FrameBufferAttachment(mMultiviewColorTexture, 0, 0));
            mFbo[1]->setAttachment(osg::Camera::COLOR_BUFFER, osg::FrameBufferAttachment(mMultiviewColorTexture, 1, 0));
#endif
        }
        else
        {
            for (unsigned i = 0; i < 2; i++)
            {
                if (mSamples > 1)
                {
                    auto colorTexture2DMsaa = createTextureMsaa(sourceFormat, sourceType, internalFormat);
                    mColorTexture[i] = colorTexture2DMsaa;
                    mFbo[i]->setAttachment(osg::Camera::COLOR_BUFFER, osg::FrameBufferAttachment(colorTexture2DMsaa));
                }
                else
                {
                    auto colorTexture2D = createTexture(sourceFormat, sourceType, internalFormat);
                    mColorTexture[i] = colorTexture2D;
                    mFbo[i]->setAttachment(osg::Camera::COLOR_BUFFER, osg::FrameBufferAttachment(colorTexture2D));
                }
            }
        }
    }

    void StereoFramebuffer::attachDepthComponent(GLint sourceFormat, GLint sourceType, GLint internalFormat)
    {
        if (mMultiview)
        {
#ifdef OSG_HAS_MULTIVIEW
            mMultiviewDepthTexture = createTextureArray(sourceFormat, sourceType, internalFormat);
            mMultiviewFbo->setAttachment(osg::Camera::DEPTH_BUFFER, osg::FrameBufferAttachment(mMultiviewDepthTexture, osg::Camera::FACE_CONTROLLED_BY_MULTIVIEW_SHADER, 0));
            mFbo[0]->setAttachment(osg::Camera::DEPTH_BUFFER, osg::FrameBufferAttachment(mMultiviewDepthTexture, 0, 0));
            mFbo[1]->setAttachment(osg::Camera::DEPTH_BUFFER, osg::FrameBufferAttachment(mMultiviewDepthTexture, 1, 0));
#endif
        }
        else
        {
            for (unsigned i = 0; i < 2; i++)
            {
                if (mSamples > 1)
                {
                    auto depthTexture2DMsaa = createTextureMsaa(sourceFormat, sourceType, internalFormat);
                    mDepthTexture[i] = depthTexture2DMsaa;
                    mFbo[i]->setAttachment(osg::Camera::DEPTH_BUFFER, osg::FrameBufferAttachment(depthTexture2DMsaa));
                }
                else
                {
                    auto depthTexture2D = createTexture(sourceFormat, sourceType, internalFormat);
                    mDepthTexture[i] = depthTexture2D;
                    mFbo[i]->setAttachment(osg::Camera::DEPTH_BUFFER, osg::FrameBufferAttachment(depthTexture2D));
                }
            }
        }
    }

    osg::FrameBufferObject* StereoFramebuffer::multiviewFbo()
    {
        return mMultiviewFbo;
    }

    osg::FrameBufferObject* StereoFramebuffer::fbo(int i)
    {
        return mFbo[i];
    }

    void StereoFramebuffer::attachTo(osg::Camera* camera)
    {
#ifdef OSG_HAS_MULTIVIEW
        if (mMultiview)
        {
            if (mMultiviewColorTexture)
            {
                camera->attach(osg::Camera::COLOR_BUFFER, mMultiviewColorTexture, 0, osg::Camera::FACE_CONTROLLED_BY_MULTIVIEW_SHADER, false, mSamples);
                camera->getBufferAttachmentMap()[osg::Camera::COLOR_BUFFER]._internalFormat = mMultiviewColorTexture->getInternalFormat();
                camera->getBufferAttachmentMap()[osg::Camera::COLOR_BUFFER]._mipMapGeneration = false;
            }
            if (mMultiviewDepthTexture)
            {
                camera->attach(osg::Camera::DEPTH_BUFFER, mMultiviewDepthTexture, 0, osg::Camera::FACE_CONTROLLED_BY_MULTIVIEW_SHADER, false, mSamples);
                camera->getBufferAttachmentMap()[osg::Camera::DEPTH_BUFFER]._internalFormat = mMultiviewDepthTexture->getInternalFormat();
                camera->getBufferAttachmentMap()[osg::Camera::DEPTH_BUFFER]._mipMapGeneration = false;
            }
            camera->setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT);
        }
#endif
    }

    osg::Texture2D* StereoFramebuffer::createTexture(GLint sourceFormat, GLint sourceType, GLint internalFormat)
    {
        osg::Texture2D* texture = new osg::Texture2D;
        texture->setTextureSize(mWidth, mHeight);
        texture->setSourceFormat(sourceFormat);
        texture->setSourceType(sourceType);
        texture->setInternalFormat(internalFormat);
        texture->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR);
        texture->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
        texture->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE);
        texture->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE);
        texture->setWrap(osg::Texture::WRAP_R, osg::Texture::CLAMP_TO_EDGE);
        return texture;
    }

    osg::Texture2DMultisample* StereoFramebuffer::createTextureMsaa(GLint sourceFormat, GLint sourceType, GLint internalFormat)
    {
        osg::Texture2DMultisample* texture = new osg::Texture2DMultisample;
        texture->setTextureSize(mWidth, mHeight);
        texture->setNumSamples(mSamples);
        texture->setSourceFormat(sourceFormat);
        texture->setSourceType(sourceType);
        texture->setInternalFormat(internalFormat);
        texture->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR);
        texture->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
        texture->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE);
        texture->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE);
        texture->setWrap(osg::Texture::WRAP_R, osg::Texture::CLAMP_TO_EDGE);
        return texture;
    }

    osg::Texture2DArray* StereoFramebuffer::createTextureArray(GLint sourceFormat, GLint sourceType, GLint internalFormat)
    {
        osg::Texture2DArray* textureArray = new osg::Texture2DArray;
        textureArray->setTextureSize(mWidth, mHeight, 2);
        textureArray->setSourceFormat(sourceFormat);
        textureArray->setSourceType(sourceType);
        textureArray->setInternalFormat(internalFormat);
        textureArray->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR);
        textureArray->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
        textureArray->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE);
        textureArray->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE);
        textureArray->setWrap(osg::Texture::WRAP_R, osg::Texture::CLAMP_TO_EDGE);
        return textureArray;
    }
}
