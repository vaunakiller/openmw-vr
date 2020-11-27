#include "vrgui.hpp"

#include <cmath>

#include "vrenvironment.hpp"
#include "vrsession.hpp"
#include "openxrmanagerimpl.hpp"
#include "openxrinput.hpp"
#include "vranimation.hpp"

#include <osg/Texture2D>
#include <osg/ClipNode>
#include <osg/FrontFace>
#include <osg/BlendFunc>
#include <osg/Depth>

#include <osgViewer/Renderer>

#include <components/sceneutil/visitor.hpp>
#include <components/sceneutil/shadow.hpp>
#include <components/myguiplatform/myguirendermanager.hpp>
#include <components/misc/constants.hpp>

#include "../mwrender/util.hpp"
#include "../mwrender/renderbin.hpp"
#include "../mwrender/renderingmanager.hpp"
#include "../mwrender/camera.hpp"
#include "../mwrender/vismask.hpp"

#include "../mwbase/world.hpp"
#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"

#include "../mwgui/windowbase.hpp"

#include "../mwbase/statemanager.hpp"

#include <MyGUI_Widget.h>
#include <MyGUI_ILayer.h>
#include <MyGUI_InputManager.h>
#include <MyGUI_WidgetManager.h>
#include <MyGUI_Window.h>
#include <MyGUI_Button.h>

namespace osg
{
    // Convenience
    const double PI_8 = osg::PI_4 / 2.;
}

namespace MWVR
{

    // When making a circle of a given radius of equally wide planes separated by a given angle, what is the width
    static osg::Vec2 radiusAngleWidth(float radius, float angleRadian)
    {
        const float width = std::fabs(2.f * radius * tanf(angleRadian / 2.f));
        return osg::Vec2(width, width);
    }

    /// RTT camera used to draw the osg GUI to a texture
    class GUICamera : public osg::Camera
    {
    public:
        GUICamera(int width, int height, osg::Vec4 clearColor)
        {
            setRenderOrder(osg::Camera::PRE_RENDER);
            setClearMask(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            setCullingActive(false);

            // Make the texture just a little transparent to feel more natural in the game world.
            setClearColor(clearColor);

            setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT);
            setReferenceFrame(osg::Camera::ABSOLUTE_RF);
            setComputeNearFarMode(osg::CullSettings::DO_NOT_COMPUTE_NEAR_FAR);
            setName("GUICamera");

            setCullMask(MWRender::Mask_GUI);
            setNodeMask(MWRender::Mask_RenderToTexture);

            setViewport(0, 0, width, height);

            // No need for Update traversal since the mSceneRoot is already updated as part of the main scene graph
            // A double update would mess with the light collection (in addition to being plain redundant)
            setUpdateCallback(new MWRender::NoTraverseCallback);

            // Create the texture
            mTexture = new osg::Texture2D;
            mTexture->setTextureSize(width, height);
            mTexture->setInternalFormat(GL_RGBA);
            mTexture->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR_MIPMAP_LINEAR);
            mTexture->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
            mTexture->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE);
            mTexture->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE);
            attach(osg::Camera::COLOR_BUFFER, mTexture);
            // Need to regenerate mipmaps every frame
            setPostDrawCallback(new MWRender::MipmapCallback(mTexture));

            // Do not want to waste time on shadows when generating the GUI texture
            SceneUtil::ShadowManager::disableShadowsForStateSet(getOrCreateStateSet());

            // Put rendering as early as possible
            getOrCreateStateSet()->setRenderBinDetails(-1, "RenderBin");

        }

        void setScene(osg::Node* scene)
        {
            if (mScene)
                removeChild(mScene);
            mScene = scene;
            addChild(scene);
            Log(Debug::Verbose) << "Set new scene: " << mScene->getName();
        }

        osg::Texture2D* getTexture() const
        {
            return mTexture.get();
        }

    private:
        osg::ref_ptr<osg::Texture2D> mTexture;
        osg::ref_ptr<osg::Node> mScene;
    };


    class LayerUpdateCallback : public osg::Callback
    {
    public:
        LayerUpdateCallback(VRGUILayer* layer)
            : mLayer(layer)
        {

        }

        bool run(osg::Object* object, osg::Object* data)
        {
            mLayer->update();
            return traverse(object, data);
        }

    private:
        VRGUILayer* mLayer;
    };

    VRGUILayer::VRGUILayer(
        osg::ref_ptr<osg::Group> geometryRoot,
        osg::ref_ptr<osg::Group> cameraRoot,
        MyGUI::ILayer* layer,
        LayerConfig config,
        VRGUIManager* parent)
        : mConfig(config)
        , mLayerName(layer->getName())
        , mMyGUILayer(layer)
        , mGeometryRoot(geometryRoot)
        , mCameraRoot(cameraRoot)
    {
        osg::ref_ptr<osg::Vec3Array> vertices{ new osg::Vec3Array(4) };
        osg::ref_ptr<osg::Vec2Array> texCoords{ new osg::Vec2Array(4) };
        osg::ref_ptr<osg::Vec3Array> normals{ new osg::Vec3Array(1) };

        auto extent_units = config.extent * Constants::UnitsPerMeter;

        float left = mConfig.center.x() - 0.5;
        float right = left + 1.f;
        float top = 0.5f + mConfig.center.y();
        float bottom = top - 1.f;

        // Define the menu quad
        osg::Vec3 top_left(left, 1, top);
        osg::Vec3 bottom_left(left, 1, bottom);
        osg::Vec3 bottom_right(right, 1, bottom);
        osg::Vec3 top_right(right, 1, top);
        (*vertices)[0] = top_left;
        (*vertices)[1] = bottom_left;
        (*vertices)[2] = bottom_right;
        (*vertices)[3] = top_right;
        mGeometry->setVertexArray(vertices);
        (*texCoords)[0].set(0.0f, 1.0f);
        (*texCoords)[1].set(0.0f, 0.0f);
        (*texCoords)[2].set(1.0f, 0.0f);
        (*texCoords)[3].set(1.0f, 1.0f);
        mGeometry->setTexCoordArray(0, texCoords);
        (*normals)[0].set(0.0f, -1.0f, 0.0f);
        mGeometry->setNormalArray(normals, osg::Array::BIND_OVERALL);
        mGeometry->addPrimitiveSet(new osg::DrawArrays(GL_QUADS, 0, 4));
        mGeometry->setDataVariance(osg::Object::STATIC);
        mGeometry->setSupportsDisplayList(false);
        mGeometry->setName("VRGUILayer");

        // Create the camera that will render the menu texture
        std::string filter = mLayerName;
        if (!mConfig.extraLayers.empty())
            filter = filter + ";" + mConfig.extraLayers;
        mGUICamera = new GUICamera(config.pixelResolution.x(), config.pixelResolution.y(), config.backgroundColor);
        osgMyGUI::RenderManager& renderManager = static_cast<osgMyGUI::RenderManager&>(MyGUI::RenderManager::getInstance());
        mMyGUICamera = renderManager.createGUICamera(osg::Camera::NESTED_RENDER, filter);
        //myGUICamera->setViewport(0, 0, 256, 256);
        //mMyGUICamera->setProjectionMatrixAsOrtho2D(-1, 1, -1, 1);
        mGUICamera->setScene(mMyGUICamera);

        // Define state set that allows rendering with transparency
        osg::StateSet* stateSet = mGeometry->getOrCreateStateSet();
        stateSet->setTextureAttributeAndModes(0, menuTexture(), osg::StateAttribute::ON);
        stateSet->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
        stateSet->setMode(GL_BLEND, osg::StateAttribute::ON);
        stateSet->setAttributeAndModes(new osg::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
        stateSet->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
        mGeometry->setStateSet(stateSet);

        // Position in the game world
        mTransform->setScale(osg::Vec3(extent_units.x(), 1.f, extent_units.y()));
        mTransform->addChild(mGeometry);

        // Add to scene graph
        mGeometryRoot->addChild(mTransform);
        mCameraRoot->addChild(mGUICamera);

        // Edit offset to account for priority
        if (!mConfig.sideBySide)
        {
            mConfig.offset.y() -= 0.001f * static_cast<float>(mConfig.priority);
        }

        mTransform->addUpdateCallback(new LayerUpdateCallback(this));
    }

    VRGUILayer::~VRGUILayer()
    {
        mGeometryRoot->removeChild(mTransform);
        mCameraRoot->removeChild(mGUICamera);
    }
    osg::Camera* VRGUILayer::camera()
    {
        return mGUICamera.get();
    }

    osg::ref_ptr<osg::Texture2D> VRGUILayer::menuTexture()
    {
        if (mGUICamera)
            return mGUICamera->getTexture();
        return nullptr;
    }

    void VRGUILayer::setAngle(float angle)
    {
        mRotation = osg::Quat{ angle, osg::Z_AXIS };
        updatePose();
    }

    void VRGUILayer::updateTracking(const Pose& headPose)
    {
        if (mConfig.trackingMode == TrackingMode::Menu)
        {
            mTrackedPose = headPose;
        }
        else
        {
            auto* anim = MWVR::Environment::get().getPlayerAnimation();
            if (anim)
            {
                const osg::Node* hand = nullptr;
                if (mConfig.trackingMode == TrackingMode::HudLeftHand)
                    hand = anim->getNode("bip01 l hand");
                else
                    hand = anim->getNode("bip01 r hand");
                if (hand)
                {
                    auto world = osg::computeLocalToWorld(hand->getParentalNodePaths()[0]);
                    mTrackedPose.position = world.getTrans();
                    mTrackedPose.orientation = world.getRotate();
                    if (mConfig.trackingMode == TrackingMode::HudRightHand)
                        mTrackedPose.orientation = osg::Quat(osg::PI, osg::Vec3(1, 0, 0)) * mTrackedPose.orientation;
                    mTrackedPose.orientation = osg::Quat(osg::PI_2, osg::Vec3(0, 0, 1)) * mTrackedPose.orientation;
                    mTrackedPose.orientation = osg::Quat(osg::PI, osg::Vec3(1, 0, 0)) * mTrackedPose.orientation;
                }
            }
        }

        updatePose();
    }

    void VRGUILayer::updatePose()
    {

        auto orientation = mRotation * mTrackedPose.orientation;

        if (mConfig.trackingMode == TrackingMode::Menu)
        {
            // Force menu layers to be vertical
            auto axis = osg::Z_AXIS;
            osg::Quat vertical;
            auto local = orientation * axis;
            vertical.makeRotate(local, axis);
            orientation = orientation * vertical;
        }
        // Orient the offset and move the layer
        auto position = mTrackedPose.position + orientation * mConfig.offset * Constants::UnitsPerMeter;

        mTransform->setAttitude(orientation);
        mTransform->setPosition(position);
    }

    void VRGUILayer::updateRect()
    {
        auto viewSize = MyGUI::RenderManager::getInstance().getViewSize();
        mRealRect.left = 1.f;
        mRealRect.top = 1.f;
        mRealRect.right = 0.f;
        mRealRect.bottom = 0.f;
        float realWidth = static_cast<float>(viewSize.width);
        float realHeight = static_cast<float>(viewSize.height);
        for (auto* widget : mWidgets)
        {
            auto rect = widget->mMainWidget->getAbsoluteRect();
            mRealRect.left = std::min(static_cast<float>(rect.left) / realWidth, mRealRect.left);
            mRealRect.top = std::min(static_cast<float>(rect.top) / realHeight, mRealRect.top);
            mRealRect.right = std::max(static_cast<float>(rect.right) / realWidth, mRealRect.right);
            mRealRect.bottom = std::max(static_cast<float>(rect.bottom) / realHeight, mRealRect.bottom);
        }

        // Some widgets don't capture the full visual
        if (mLayerName == "JournalBooks")
        {
            mRealRect.left = 0.f;
            mRealRect.top = 0.f;
            mRealRect.right = 1.f;
            mRealRect.bottom = 1.f;
        }

        if (mLayerName == "Notification")
        {
            // The latest widget for notification is always the top one
            // So i just stretch the rectangle to the bottom.
            mRealRect.bottom = 1.f;
        }
    }

    void VRGUILayer::update()
    {
        auto xr = MWVR::Environment::get().getManager();
        if (!xr)
            return;

        if (mConfig.trackingMode != TrackingMode::Menu || !xr->appShouldRender())
            updateTracking();

        if (mConfig.sideBySide)
        {
            // The side-by-side windows are also the resizable windows.
            // Stretch according to config
            // This genre of layer should only ever have 1 widget as it will cover the full layer
            auto* widget = mWidgets.front();
            auto* myGUIWindow = dynamic_cast<MyGUI::Window*>(widget->mMainWidget);
            auto* windowBase = dynamic_cast<MWGui::WindowBase*>(widget);
            if (windowBase && myGUIWindow)
            {
                auto w = mConfig.myGUIViewSize.x();
                auto h = mConfig.myGUIViewSize.y();
                windowBase->setCoordf(0.f, 0.f, w, h);
                windowBase->onWindowResize(myGUIWindow);
            }
        }
        updateRect();

        float w = 0.f;
        float h = 0.f;
        for (auto* widget : mWidgets)
        {
            w = std::max(w, (float)widget->mMainWidget->getWidth());
            h = std::max(h, (float)widget->mMainWidget->getHeight());
        }

        // Pixels per unit
        float res = static_cast<float>(mConfig.spatialResolution) / Constants::UnitsPerMeter;

        if (mConfig.sizingMode == SizingMode::Auto)
        {
            mTransform->setScale(osg::Vec3(w / res, 1.f, h / res));
        }
        if (mLayerName == "Notification")
        {
            auto viewSize = MyGUI::RenderManager::getInstance().getViewSize();
            h = (1.f - mRealRect.top) * static_cast<float>(viewSize.height);
            mTransform->setScale(osg::Vec3(w / res, 1.f, h / res));
        }

        // Convert from [0,1] range to [-1,1]
        float menuLeft = mRealRect.left * 2. - 1.;
        float menuRight = mRealRect.right * 2. - 1.;
        // Opposite convention 
        float menuBottom = (1.f - mRealRect.bottom) * 2. - 1.;
        float menuTop = (1.f - mRealRect.top) * 2.f - 1.;

        mMyGUICamera->setProjectionMatrixAsOrtho2D(menuLeft, menuRight, menuBottom, menuTop);
    }

    void
        VRGUILayer::insertWidget(
            MWGui::Layout* widget)
    {
        for (auto* w : mWidgets)
            if (w == widget)
                return;
        mWidgets.push_back(widget);
    }

    void
        VRGUILayer::removeWidget(
            MWGui::Layout* widget)
    {
        for (auto it = mWidgets.begin(); it != mWidgets.end(); it++)
        {
            if (*it == widget)
            {
                mWidgets.erase(it);
                return;
            }
        }
    }

    class VRGUIManagerUpdateCallback : public osg::Callback
    {
    public:
        VRGUIManagerUpdateCallback(VRGUIManager* manager)
            : mManager(manager)
        {

        }

        bool run(osg::Object* object, osg::Object* data)
        {
            mManager->update();
            return traverse(object, data);
        }

    private:
        VRGUIManager* mManager;
    };

    VRGUIManager::VRGUIManager(
        osg::ref_ptr<osgViewer::Viewer> viewer)
        : mOsgViewer(viewer)
    {
        mGUIGeometriesRoot->setName("VR GUI Geometry Root");
        mGUIGeometriesRoot->setUpdateCallback(new VRGUIManagerUpdateCallback(this));
        mGUICamerasRoot->setName("VR GUI Cameras Root");
        auto* root = viewer->getSceneData();
        root->asGroup()->addChild(mGUICamerasRoot);
        root->asGroup()->addChild(mGUIGeometriesRoot);

    }

    VRGUIManager::~VRGUIManager(void)
    {
    }

    static const LayerConfig createDefaultConfig(int priority, bool background = true, SizingMode sizingMode = SizingMode::Auto, std::string extraLayers = "Popup")
    {
        return LayerConfig{
            priority,
            false, // side-by-side
            background ? osg::Vec4{0.f,0.f,0.f,.75f} : osg::Vec4{}, // background
            osg::Vec3(0.f,0.66f,-.25f), // offset
            osg::Vec2(0.f,0.f), // center (model space)
            osg::Vec2(1.f, 1.f), // extent (meters)
            1024, // Spatial resolution (pixels per meter)
            osg::Vec2i(2048,2048), // Texture resolution
            osg::Vec2(1,1),
            sizingMode,
            TrackingMode::Menu,
            extraLayers,
            false
        };
    }
    LayerConfig gDefaultConfig = createDefaultConfig(1);
    LayerConfig gVideoPlayerConfig = createDefaultConfig(1, true, SizingMode::Fixed);
    LayerConfig gLoadingScreenConfig = createDefaultConfig(1, true, SizingMode::Fixed, "Menu");
    LayerConfig gMainMenuConfig = createDefaultConfig(1, true);
    LayerConfig gJournalBooksConfig = createDefaultConfig(2, false, SizingMode::Fixed);
    LayerConfig gDefaultWindowsConfig = createDefaultConfig(3, true);
    LayerConfig gMessageBoxConfig = createDefaultConfig(6, false, SizingMode::Auto);;
    LayerConfig gNotificationConfig = createDefaultConfig(7, false, SizingMode::Fixed);

    //LayerConfig gVirtualKeyboardConfig = createDefaultConfig(50);
    LayerConfig gVirtualKeyboardConfig = LayerConfig{
        10,
        false,
        osg::Vec4{0.f,0.f,0.f,.75f},
        osg::Vec3(0.025f,.025f,.066f), // offset (meters)
        osg::Vec2(0.f,0.5f), // center (model space)
        osg::Vec2(.25f, .25f), // extent (meters)
        2048, // Spatial resolution (pixels per meter)
        osg::Vec2i(2048,2048), // Texture resolution
        osg::Vec2(1,1),
        SizingMode::Auto,
        TrackingMode::HudLeftHand,
        "",
        true
    };

    static const float sSideBySideRadius = 1.f;
    static const float sSideBySideAzimuthInterval = -osg::PI_4;
    static const LayerConfig createSideBySideConfig(int priority)
    {
        return LayerConfig{
            priority,
            true, // side-by-side
            gDefaultConfig.backgroundColor,
            osg::Vec3(0.f,sSideBySideRadius,-.25f), // offset
            gDefaultConfig.center,
            radiusAngleWidth(sSideBySideRadius, sSideBySideAzimuthInterval), // extent (meters)
            gDefaultConfig.spatialResolution,
            gDefaultConfig.pixelResolution,
            osg::Vec2(0.70f, 0.70f),
            SizingMode::Fixed,
            gDefaultConfig.trackingMode,
            "",
            false
        };
    };

    LayerConfig gStatsWindowConfig = createSideBySideConfig(0);
    LayerConfig gInventoryWindowConfig = createSideBySideConfig(1);
    LayerConfig gSpellWindowConfig = createSideBySideConfig(2);
    LayerConfig gMapWindowConfig = createSideBySideConfig(3);
    LayerConfig gInventoryCompanionWindowConfig = createSideBySideConfig(4);
    LayerConfig gDialogueWindowConfig = createSideBySideConfig(5);

    LayerConfig gStatusHUDConfig = LayerConfig
    {
        0,
        false, // side-by-side
        osg::Vec4{}, // background
        osg::Vec3(0.025f,.025f,.066f), // offset (meters)
        osg::Vec2(0.f,0.5f), // center (model space)
        osg::Vec2(.1f, .1f), // extent (meters)
        1024, // resolution (pixels per meter)
        osg::Vec2i(1024,1024),
        gDefaultConfig.myGUIViewSize,
        SizingMode::Auto,
        TrackingMode::HudLeftHand,
        "",
        false
    };

    LayerConfig gPopupConfig = LayerConfig
    {
        0,
        false, // side-by-side
        osg::Vec4{0.f,0.f,0.f,0.f}, // background
        osg::Vec3(-0.025f,.025f,.066f), // offset (meters)
        osg::Vec2(0.f,0.5f), // center (model space)
        osg::Vec2(.1f, .1f), // extent (meters)
        1024, // resolution (pixels per meter)
        osg::Vec2i(2048,2048),
        gDefaultConfig.myGUIViewSize,
        SizingMode::Auto,
        TrackingMode::HudRightHand,
        "",
        false
    };



    static std::map<std::string, LayerConfig&> gLayerConfigs =
    {
        {"StatusHUD", gStatusHUDConfig},
        {"Tooltip", gPopupConfig},
        {"JournalBooks", gJournalBooksConfig},
        {"InventoryCompanionWindow", gInventoryCompanionWindowConfig},
        {"InventoryWindow", gInventoryWindowConfig},
        {"SpellWindow", gSpellWindowConfig},
        {"MapWindow", gMapWindowConfig},
        {"StatsWindow", gStatsWindowConfig},
        {"DialogueWindow", gDialogueWindowConfig},
        {"MessageBox", gMessageBoxConfig},
        {"Windows", gDefaultWindowsConfig},
        {"MainMenu", gMainMenuConfig},
        {"Notification", gNotificationConfig},
        {"InputBlocker", gVideoPlayerConfig},
        {"Menu", gVideoPlayerConfig},
        {"LoadingScreen", gLoadingScreenConfig},
        {"VirtualKeyboard", gVirtualKeyboardConfig},
    };

    static std::set<std::string> layerBlacklist =
    {
        "Overlay",
        "AdditiveOverlay",
    };

    void VRGUIManager::updateSideBySideLayers()
    {
        // Nothing to update
        if (mSideBySideLayers.size() == 0)
            return;

        std::sort(mSideBySideLayers.begin(), mSideBySideLayers.end(), [](const auto& lhs, const auto& rhs) { return *lhs < *rhs; });

        int n = mSideBySideLayers.size();

        float span = sSideBySideAzimuthInterval * static_cast<float>(n - 1); // zero index, places lone layers straight ahead
        float low = -span / 2;

        for (unsigned i = 0; i < mSideBySideLayers.size(); i++)
            mSideBySideLayers[i]->setAngle(low + static_cast<float>(i) * sSideBySideAzimuthInterval);
    }

    void VRGUIManager::insertLayer(MyGUI::ILayer* layer)
    {
        LayerConfig config = gDefaultConfig;
        const auto& name = layer->getName();
        auto configIt = gLayerConfigs.find(name);
        if (configIt != gLayerConfigs.end())
        {
            config = configIt->second;
        }
        else
        {
            Log(Debug::Warning) << "Layer " << name << " has no configuration, using default";
        }

        auto vrlayer = std::shared_ptr<VRGUILayer>(new VRGUILayer(
            mGUIGeometriesRoot,
            mGUICamerasRoot,
            layer,
            config,
            this
        ));
        mLayers[name] = vrlayer;

        vrlayer->mGeometry->setUserData(new VRGUILayerUserData(mLayers[name]));

        if (config.sideBySide)
        {
            mSideBySideLayers.push_back(vrlayer);
            updateSideBySideLayers();
        }

        if (config.trackingMode == TrackingMode::Menu)
            vrlayer->updateTracking(mHeadPose);
    }

    void VRGUIManager::insertWidget(MWGui::Layout* widget)
    {
        auto* layer = widget->mMainWidget->getLayer();
        auto name = layer->getName();

        auto it = mLayers.find(name);
        if (it == mLayers.end())
        {
            insertLayer(layer);
            it = mLayers.find(name);
        }

        it->second->insertWidget(widget);
    }

    void VRGUIManager::removeLayer(MyGUI::ILayer* layer)
    {
        auto it = mLayers.find(layer->getName());
        if (it == mLayers.end())
            return;

        auto vrlayer = it->second;

        for (auto it2 = mSideBySideLayers.begin(); it2 < mSideBySideLayers.end(); it2++)
        {
            if (*it2 == vrlayer)
            {
                mSideBySideLayers.erase(it2);
                updateSideBySideLayers();
            }
        }

        if (it->second.get() == mFocusLayer)
            setFocusLayer(nullptr);

        mLayers.erase(it);
    }

    void VRGUIManager::removeWidget(MWGui::Layout* widget)
    {
        auto* layer = widget->mMainWidget->getLayer();

        auto it = mLayers.find(layer->getName());
        if (it == mLayers.end())
        {
            //Log(Debug::Warning) << "Tried to remove widget from nonexistent layer " << name;
            return;
        }

        it->second->removeWidget(widget);
        if (it->second->widgetCount() == 0)
        {
            removeLayer(layer);
        }
    }

    void VRGUIManager::setVisible(MWGui::Layout* widget, bool visible)
    {
        auto* layer = widget->mMainWidget->getLayer();
        auto name = layer->getName();

        if (layerBlacklist.find(name) != layerBlacklist.end())
        {
            Log(Debug::Verbose) << "setVisible (" << name << "): ignored";
            return;
        }
        Log(Debug::Verbose) << "setVisible (" << name << "): " << visible;

        if (visible)
            insertWidget(widget);
        else
            removeWidget(widget);
    }

    void VRGUIManager::setCamera(osg::Camera* camera)
    {
        mCamera = camera;
        mShouldUpdatePoses = true;
    }

    void VRGUIManager::updateTracking(void)
    {
        // Get head pose by reading the camera view matrix to place the GUI in the world.
        osg::Vec3 eye{};
        osg::Vec3 center{};
        osg::Vec3 up{};
        Pose headPose{};

        osg::ref_ptr<osg::Camera> camera;
        mCamera.lock(camera);

        if (!camera)
        {
            // If a camera is not available, use VR stage poses directly.
            auto pose = MWVR::Environment::get().getSession()->predictedPoses(MWVR::VRSession::FramePhase::Update).head;
            osg::Vec3 position = pose.position * Constants::UnitsPerMeter;
            osg::Quat orientation = pose.orientation;
            headPose.position = position;
            headPose.orientation = orientation;
        }
        else
        {
            auto viewMatrix = camera->getViewMatrix();
            viewMatrix.getLookAt(eye, center, up);
            headPose.position = eye;
            headPose.orientation = viewMatrix.getRotate();
            headPose.orientation = headPose.orientation.inverse();
        }

        mHeadPose = headPose;

        for (auto& layer : mLayers)
            layer.second->updateTracking(mHeadPose);
    }

    bool VRGUIManager::updateFocus()
    {
        auto* anim = MWVR::Environment::get().getPlayerAnimation();
        if (anim && anim->getPointerTarget().mHit)
        {
            std::shared_ptr<VRGUILayer> newFocusLayer = nullptr;
            auto* node = anim->getPointerTarget().mHitNode;
            if (node->getName() == "VRGUILayer")
            {
                VRGUILayerUserData* userData = static_cast<VRGUILayerUserData*>(node->getUserData());
                newFocusLayer = userData->mLayer.lock();
            }

            if (newFocusLayer && newFocusLayer->mLayerName != "Notification")
            {
                setFocusLayer(newFocusLayer.get());
                computeGuiCursor(anim->getPointerTarget().mHitPointLocal);
                return true;
            }
        }
        return false;
    }

    void VRGUIManager::update()
    {
        auto xr = MWVR::Environment::get().getManager();
        if (xr)
        {
            if (xr->appShouldRender())
            {
                if (mShouldUpdatePoses)
                {
                    mShouldUpdatePoses = false;
                    updateTracking();
                }
            }
            else
                mShouldUpdatePoses = true;
        }
    }

    void VRGUIManager::setFocusLayer(VRGUILayer* layer)
    {
        if (layer == mFocusLayer)
            return;

        setFocusWidget(nullptr);
        mFocusLayer = layer;
    }

    void VRGUIManager::setFocusWidget(MyGUI::Widget* widget)
    {
        if (widget == mFocusWidget)
            return;

        // TODO: This relies on MyGUI internal functions and may break on any future version.
        if (validateFocusWidget())
            mFocusWidget->_riseMouseLostFocus(widget);
        if (widget)
            widget->_riseMouseSetFocus(mFocusWidget);
        mFocusWidget = widget;
    }

    // MyGUI may delete the focusWidget.
    // Call this and check the result before dereferencing mFocusWidget
    bool VRGUIManager::validateFocusWidget()
    {
        if (mFocusWidget)
        {
            auto* cursorWidget = widgetFromGuiCursor(mGuiCursor.x(), mGuiCursor.y());
            // If the focus widget is no longer seen at the coordinate it was last seen
            // it may have been deleted by mygui.
            // In theory, notifyWidgetUnlinked() should catch all cases but doesn't.
            if (cursorWidget != mFocusWidget)
            {
                mFocusWidget = nullptr;
                return false;
            }
            return true;
        }
        return false;
    }

    bool VRGUIManager::focusIsModalWindow()
    {
        if (mFocusLayer)
            for (auto* window : mFocusLayer->mWidgets)
                if (mModalWindow == window->mMainWidget)
                    return true;
        return false;
    }

    MyGUI::Widget* VRGUIManager::widgetFromGuiCursor(int x, int y)
    {
        if (mFocusLayer)
        {
            if (
                mFocusLayer->mConfig.ignoreModality
                || !mModalWindow
                || focusIsModalWindow()
                )
                return static_cast<MyGUI::Widget*>(mFocusLayer->mMyGUILayer->getLayerItemByPoint(x, y));
        }
        return nullptr;
    }

    void VRGUIManager::updateGuiCursor(int x, int y)
    {
        setFocusWidget(widgetFromGuiCursor(x, y));
        mGuiCursor.x() = x;
        mGuiCursor.y() = y;
    }

    bool VRGUIManager::injectMouseClick(bool onPress)
    {
        // TODO: This relies on MyGUI internal functions and may break on any future version.
        if (validateFocusWidget())
        {
            if (onPress)
            {
                mFocusWidget->_riseMouseButtonPressed(mGuiCursor.x(), mGuiCursor.y(), MyGUI::MouseButton::Left);

                MyGUI::Button* b = mFocusWidget->castType<MyGUI::Button>(false);
                if (b && b->getEnabled())
                {
                    MWBase::Environment::get().getWindowManager()->playSound("Menu Click");
                }
            }
            else
            {
                mFocusWidget->_riseMouseButtonReleased(mGuiCursor.x(), mGuiCursor.y(), MyGUI::MouseButton::Left);
                mFocusWidget->_riseMouseButtonClick();
            }
            return true;
        }
        return false;
    }

    void VRGUIManager::notifyWidgetUnlinked(MyGUI::Widget* widget)
    {
        if (widget == mFocusWidget)
        {
            mFocusWidget = nullptr;
        }
    }

    void VRGUIManager::notifyModalWindow(MyGUI::Widget* window)
    {
        mModalWindow = window;
    }

    void VRGUIManager::computeGuiCursor(osg::Vec3 hitPoint)
    {
        float x = 0;
        float y = 0;
        if (mFocusLayer)
        {
            osg::Vec2 bottomLeft = mFocusLayer->mConfig.center - osg::Vec2(0.5f, 0.5f);
            x = hitPoint.x() - bottomLeft.x();
            y = hitPoint.z() - bottomLeft.y();
            auto rect = mFocusLayer->mRealRect;
            auto viewSize = MyGUI::RenderManager::getInstance().getViewSize();
            float width = static_cast<float>(viewSize.width) * rect.width();
            float height = static_cast<float>(viewSize.height) * rect.height();
            float left = static_cast<float>(viewSize.width) * rect.left;
            float bottom = static_cast<float>(viewSize.height) * rect.bottom;
            x = width * x + left;
            y = bottom - height * y;
        }

        MWBase::Environment::get().getWindowManager()->setCursorActive(true);
        updateGuiCursor((int)x, (int)y);
    }

}
