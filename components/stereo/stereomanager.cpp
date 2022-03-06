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
#include <components/sceneutil/color.hpp>

#include <components/settings/settings.hpp>

#include <components/misc/stringops.hpp>
#include <components/misc/callbackmanager.hpp>

#include <components/vr/vr.hpp>

namespace Stereo
{
#ifdef OSG_HAS_MULTIVIEW
    struct MultiviewFrustumCallback : public osg::CullSettings::InitialFrustumCallback
    {
        MultiviewFrustumCallback()
        {

        }

        void setInitialFrustum(osg::CullStack& cullStack, osg::Polytope& frustum) const override
        {
            auto cm = cullStack.getCullingMode();
            bool nearCulling = !!(cm & osg::CullSettings::NEAR_PLANE_CULLING);
            bool farCulling = !!(cm & osg::CullSettings::FAR_PLANE_CULLING);
            frustum.setToBoundingBox(mBoundingBox, nearCulling, farCulling);
        }

        osg::BoundingBoxd mBoundingBox;
    };
#else
    // A dummy type to avoid the need to preprocessor switch away inconsequential calls.
    struct MultiviewFrustumCallback : public osg::Referenced
    {
        osg::BoundingBoxd mBoundingBox;
    };
#endif

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
                uProjectionMatrix->set(mManager->computeEyeProjection(0, true));
        }

        void applyRight(osg::StateSet* stateset, osgUtil::CullVisitor* nv) override
        {
            osg::Matrix dummy;
            auto* uProjectionMatrix = stateset->getUniform("projectionMatrix");
            if (uProjectionMatrix)
                uProjectionMatrix->set(mManager->computeEyeProjection(1, true));
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
        , mMainCamera(mViewer->getCamera())
        , mStereoRoot(new osg::Group)
        , mUpdateCallback(new StereoUpdateCallback(this))
        , mMasterReverseZProjectionMatrix(osg::Matrix::identity())
        , mEyeResolutionOverriden(false)
        , mEyeResolutionOverride(0,0)
        , mStereoShaderRoot(new osg::Group)
        , mMultiviewFrustumCallback(new MultiviewFrustumCallback)
        , mMasterConfig(new SharedShadowMapConfig)
        , mSlaveConfig(new SharedShadowMapConfig)
        , mSharedShadowMaps(Settings::Manager::getBool("shared shadow maps", "Stereo"))
        , mUpdateViewCallback(nullptr)
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

    Manager::~Manager()
    {
    }

    void Manager::initializeStereo(osg::GraphicsContext* gc)
    {
        mRoot = mViewer->getSceneData()->asGroup();
        mMainCamera->addUpdateCallback(mUpdateCallback);

        auto ci = gc->getState()->getContextID();
        configureExtensions(ci);

        if(getMultiview())
            setupOVRMultiView2Technique();
        else
            setupBruteForceTechnique();

        updateStereoFramebuffer();

        setupSharedShadows();
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

    void Manager::overrideEyeResolution(const osg::Vec2i& eyeResolution)
    {
        mEyeResolutionOverride = eyeResolution;
        mEyeResolutionOverriden = true;

        if (mMultiviewFramebuffer)
            updateStereoFramebuffer();
    }

    void Manager::screenResolutionChanged()
    {
        updateStereoFramebuffer();
    }

    osg::Vec2i Manager::eyeResolution()
    {
        if (mEyeResolutionOverriden)
            return mEyeResolutionOverride;
        auto width = mMainCamera->getViewport()->width() / 2;
        auto height = mMainCamera->getViewport()->height();

        return osg::Vec2i(width, height);
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
                return mManager->computeEyeProjection(0, false);
            }

            osg::Matrixd computeLeftEyeView(const osg::Matrixd& view) const override
            {
                (void)view;
                return mManager->computeEyeView(0);
            }

            osg::Matrixd computeRightEyeProjection(const osg::Matrixd& projection) const override
            {
                (void)projection;
                return mManager->computeEyeProjection(1, false);
            }

            osg::Matrixd computeRightEyeView(const osg::Matrixd& view) const override
            {
                (void)view;
                return mManager->computeEyeView(1);
            }

            Manager* mManager;
        };

        auto* renderer = static_cast<osgViewer::Renderer*>(mMainCamera->getRenderer());
        for (auto* sceneView : { renderer->getSceneView(0), renderer->getSceneView(1) })
            sceneView->setComputeStereoMatricesCallback(new ComputeStereoMatricesCallback(this));
    }

    void Manager::setupOVRMultiView2Technique()
    {
        auto* ds = osg::DisplaySettings::instance().get();
        ds->setStereo(false);

#ifdef OSG_MADSBUVI_DISPLAY_LIST_PATCH
        if (Settings::Manager::getBool("disable display lists for multiview", "Stereo"))
        {
            ds->setDisplayListHint(osg::DisplaySettings::DISPLAYLIST_DISABLED);
            Log(Debug::Verbose) << "Disabling display lists";
        }
#endif

        mMainCamera->addCullCallback(new OVRMultiViewStereoStatesetUpdateCallback(this));
        mStereoShaderRoot->addChild(mRoot);
        mStereoRoot->addChild(mStereoShaderRoot);

#ifdef OSG_HAS_MULTIVIEW
        mMainCamera->setInitialFrustumCallback(mMultiviewFrustumCallback);
#endif

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
                sceneView->getCullVisitor()->setUserData(mMasterConfig);
                sceneView->getCullVisitorLeft()->setUserData(mMasterConfig);
                sceneView->getCullVisitorRight()->setUserData(mSlaveConfig);
            }
        }
    }

    void Manager::updateStereoFramebuffer()
    {
        auto samples = Settings::Manager::getInt("antialiasing", "Video");
        auto eyeRes = eyeResolution();

        if (mMultiviewFramebuffer)
            mMultiviewFramebuffer->detachFrom(mMainCamera);
        mMultiviewFramebuffer = std::make_shared<MultiviewFramebuffer>(static_cast<int>(eyeRes.x()), static_cast<int>(eyeRes.y()), samples);
        mMultiviewFramebuffer->attachColorComponent(SceneUtil::Color::colorSourceFormat(), SceneUtil::Color::colorSourceType(), SceneUtil::Color::colorInternalFormat());
        mMultiviewFramebuffer->attachDepthComponent(SceneUtil::AutoDepth::depthSourceFormat(), SceneUtil::AutoDepth::depthSourceType(), SceneUtil::AutoDepth::depthInternalFormat());
        mMultiviewFramebuffer->attachTo(mMainCamera);
    }

    void Manager::update()
    {
        double near_ = 1.f;
        double far_ = 10000.f;

        near_ = Settings::Manager::getFloat("near clip", "Camera");
        far_ = Settings::Manager::getFloat("viewing distance", "Camera");
        auto projectionMatrix = mViewer->getCamera()->getProjectionMatrix();

        if (mUpdateViewCallback)
        {
            mUpdateViewCallback->updateView(mView[0], mView[1]);
            auto viewMatrix = mViewer->getCamera()->getViewMatrix();
            mViewOffsetMatrix[0] = mView[0].pose.viewMatrix(true);
            mViewOffsetMatrix[1] = mView[1].pose.viewMatrix(true);
            mViewMatrix[0] = viewMatrix * mViewOffsetMatrix[0];
            mViewMatrix[1] = viewMatrix * mViewOffsetMatrix[1];
            mProjectionMatrix[0] = mView[0].fov.perspectiveMatrix(near_, far_, false);
            mProjectionMatrix[1] = mView[1].fov.perspectiveMatrix(near_, far_, false);
            if (SceneUtil::AutoDepth::isReversed())
            {
                mProjectionMatrixReverseZ[0] = mView[0].fov.perspectiveMatrix(near_, far_, true);
                mProjectionMatrixReverseZ[1] = mView[1].fov.perspectiveMatrix(near_, far_, true);
            }

            FieldOfView masterFov;
            masterFov.angleDown = std::min(mView[0].fov.angleDown, mView[1].fov.angleDown);
            masterFov.angleUp = std::min(mView[0].fov.angleUp, mView[1].fov.angleUp);
            masterFov.angleLeft = std::min(mView[0].fov.angleLeft, mView[1].fov.angleLeft);
            masterFov.angleRight = std::min(mView[0].fov.angleRight, mView[1].fov.angleRight);
            projectionMatrix = masterFov.perspectiveMatrix(near_, far_, false);
            mMainCamera->setProjectionMatrix(projectionMatrix);

        }
        else
        {
            auto* ds = osg::DisplaySettings::instance().get();
            auto viewMatrix = mViewer->getCamera()->getViewMatrix();
            mViewMatrix[0] = ds->computeLeftEyeViewImplementation(viewMatrix);
            mViewMatrix[1] = ds->computeRightEyeViewImplementation(viewMatrix);
            mViewOffsetMatrix[0] = osg::Matrix::inverse(viewMatrix) * mViewMatrix[0];
            mViewOffsetMatrix[1] = osg::Matrix::inverse(viewMatrix) * mViewMatrix[1];
            mProjectionMatrix[0] = ds->computeLeftEyeProjectionImplementation(projectionMatrix);
            mProjectionMatrix[1] = ds->computeRightEyeProjectionImplementation(projectionMatrix);
            if (SceneUtil::AutoDepth::isReversed())
            {
                mProjectionMatrixReverseZ[0] = ds->computeLeftEyeProjectionImplementation(mMasterReverseZProjectionMatrix);
                mProjectionMatrixReverseZ[1] = ds->computeRightEyeProjectionImplementation(mMasterReverseZProjectionMatrix);
            }
        }

        mMasterConfig->_referenceFrame = mMainCamera->getReferenceFrame();
        mMasterConfig->_useCustomFrustum = true;
        mMasterConfig->_customFrustum.init();

        static const std::vector<osg::Vec3d> clipCorners{
            {-1.0, -1.0, -1.0},
            { 1.0, -1.0, -1.0},
            { 1.0, -1.0,  1.0},
            {-1.0, -1.0,  1.0},
            {-1.0,  1.0, -1.0},
            { 1.0,  1.0, -1.0},
            { 1.0,  1.0,  1.0},
            {-1.0,  1.0,  1.0}
        };

        for (int view : {0, 1})
        {

            osg::Matrix clipToMasterView;
            clipToMasterView.invert(mViewOffsetMatrix[view] * mProjectionMatrix[view]);

            for (const auto& clipCorner : clipCorners)
            {
                auto masterViewVertice = clipCorner * clipToMasterView;
                auto masterClipVertice = masterViewVertice * projectionMatrix;
                mMasterConfig->_customFrustum.expandBy(masterClipVertice);
            }
        }

        mMultiviewFrustumCallback->mBoundingBox = mMasterConfig->_customFrustum;
    }

    void Manager::updateStateset(osg::StateSet* stateset)
    {
        // Update stereo uniforms
        auto * viewMatrixMultiViewUniform = stateset->getUniform("viewMatrixMultiView");
        auto * projectionMatrixMultiViewUniform = stateset->getUniform("projectionMatrixMultiView");

        for (int view : {0, 1})
        {
            viewMatrixMultiViewUniform->setElement(view, mViewOffsetMatrix[view]);
            projectionMatrixMultiViewUniform->setElement(view, computeEyeProjection(view, SceneUtil::AutoDepth::isReversed()));
        }
    }

    void Manager::setUpdateViewCallback(std::shared_ptr<UpdateViewCallback> cb)
    {
        mUpdateViewCallback = cb;
    }

    void Manager::setCullCallback(osg::ref_ptr<osg::NodeCallback> cb)
    {
        mMainCamera->setCullCallback(cb);
    }

    osg::Matrixd Manager::computeEyeProjection(int view, bool allowReverseZ) const
    {
        return allowReverseZ && SceneUtil::AutoDepth::isReversed() ? mProjectionMatrixReverseZ[view] : mProjectionMatrix[view];
    }

    osg::Matrixd Manager::computeEyeView(int view) const
    {
        return mViewMatrix[view];
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
