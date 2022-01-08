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
#include <components/sceneutil/depth.hpp>

#include <components/settings/settings.hpp>

#include <components/misc/stringops.hpp>
#include <components/misc/callbackmanager.hpp>

#include <components/vr/vr.hpp>

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
        , mEyeWidthOverride(0)
        , mEyeHeightOverride(0)
        , mEyeResolutionOverriden(false)
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
        configureExtensions(ci);

        updateStereoFramebuffer();

        if(getMultiview())
            setupOVRMultiView2Technique();
        else
            setupBruteForceTechnique();
    }

    void Manager::shaderStereoDefines(Shader::ShaderManager::DefineMap& defines) const
    {
        if (getMultiview())
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

    const std::string& Manager::error() const
    {
        return mError;
    }

    void Manager::overrideEyeResolution(int width, int height)
    {
        mEyeWidthOverride = width;
        mEyeHeightOverride = height;
        mEyeResolutionOverriden = true;

        if (mMultiviewFramebuffer)
            updateStereoFramebuffer();
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
        auto* ds = osg::DisplaySettings::instance().get();
        ds->setStereo(false);

#ifdef OSG_MADSBUVI_DISPLAY_LIST_PATCH
        if (Settings::Manager::getBool("disable display lists for multiview", "Stereo"))
        {
            auto* ds = osg::DisplaySettings::instance().get();
            ds->setDisplayListHint(osg::DisplaySettings::DISPLAYLIST_DISABLED);
            Log(Debug::Verbose) << "Disabling display lists";
        }
#endif

        mMainCamera->addCullCallback(new OVRMultiViewStereoStatesetUpdateCallback(this));
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

    void Manager::updateStereoFramebuffer()
    {
        auto samples = Settings::Manager::getInt("antialiasing", "Video");

        auto width = mMainCamera->getViewport()->width() / 2;
        auto height = mMainCamera->getViewport()->height();
        if (mEyeResolutionOverriden)
        {
            width = mEyeWidthOverride;
            height = mEyeHeightOverride;
        }

        if (mMultiviewFramebuffer)
            mMultiviewFramebuffer->detachFrom(mMainCamera);
        mMultiviewFramebuffer = std::make_shared<MultiviewFramebuffer>(width, height, samples);
        mMultiviewFramebuffer->attachColorComponent(GL_RGB, GL_UNSIGNED_BYTE, GL_RGB);
        mMultiviewFramebuffer->attachDepthComponent(GL_DEPTH_COMPONENT, SceneUtil::AutoDepth::depthType(), SceneUtil::AutoDepth::depthFormat());
        mMultiviewFramebuffer->attachTo(mMainCamera);
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

        if (getMultiview())
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
        projectionMatrixMultiViewUniform->setElement(0, mLeftView.fov.perspectiveMatrix(near_, far_, SceneUtil::AutoDepth::isReversed()));
        projectionMatrixMultiViewUniform->setElement(1, mRightView.fov.perspectiveMatrix(near_, far_, SceneUtil::AutoDepth::isReversed()));
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
        return mLeftView.fov.perspectiveMatrix(near_, far_, allowReverseZ && SceneUtil::AutoDepth::isReversed());
    }

    osg::Matrixd Manager::computeLeftEyeView() const
    {
        return mLeftViewMatrix;
    }

    osg::Matrixd Manager::computeRightEyeProjection(bool allowReverseZ) const
    {
        auto near_ = Settings::Manager::getFloat("near clip", "Camera");
        auto far_ = Settings::Manager::getFloat("viewing distance", "Camera");
        return mRightView.fov.perspectiveMatrix(near_, far_, allowReverseZ && SceneUtil::AutoDepth::isReversed());
    }

    osg::Matrixd Manager::computeRightEyeView() const
    {
        return mRightViewMatrix;
    }

    namespace
    {
        bool getStereoImpl()
        {
            return VR::getVR() || Settings::Manager::getBool("stereo enabled", "Stereo") || osg::DisplaySettings::instance().get()->getStereo();
        }
    }

    bool getStereo()
    {
        static bool stereo = getStereoImpl();
        return stereo;
    }
}
