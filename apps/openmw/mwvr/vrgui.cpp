#include "vrgui.hpp"
#include "vrenvironment.hpp"
#include "openxrsession.hpp"
#include "openxrmanagerimpl.hpp"
#include "openxrinputmanager.hpp"
#include "vranimation.hpp"
#include <openxr/openxr.h>
#include <osg/Texture2D>
#include <osg/ClipNode>
#include <osg/FrontFace>
#include <osg/BlendFunc>
#include <osg/Depth>
#include <components/sceneutil/visitor.hpp>
#include <components/sceneutil/shadow.hpp>
#include <components/myguiplatform/myguirendermanager.hpp>
#include <osgViewer/Renderer>
#include "../mwrender/util.hpp"
#include "../mwrender/renderbin.hpp"
#include "../mwrender/renderingmanager.hpp"
#include "../mwrender/camera.hpp"
#include "../mwrender/vismask.hpp"
#include "../mwbase/world.hpp"
#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwgui/windowbase.hpp"

#include <MyGUI_Widget.h>
#include <MyGUI_ILayer.h>
#include <MyGUI_InputManager.h>
#include <MyGUI_WidgetManager.h>
#include <MyGUI_Window.h>

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
    const float width = std::fabs( 2.f * radius * std::tanf(angleRadian / 2.f) );
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
    std::string layerName,
    LayerConfig config,
    VRGUIManager* parent)
    : mConfig(config)
    , mLayerName(layerName)
    , mGeometryRoot(geometryRoot)
    , mCameraRoot(cameraRoot)
{
    osg::ref_ptr<osg::Vec3Array> vertices{ new osg::Vec3Array(4) };
    osg::ref_ptr<osg::Vec2Array> texCoords{ new osg::Vec2Array(4) };
    osg::ref_ptr<osg::Vec3Array> normals{ new osg::Vec3Array(1) };

    auto extent_units = config.extent * Environment::get().unitsPerMeter();

    float left = mConfig.center.x() - 0.5;
    float right = left + 1.f;
    float top = 0.5f + mConfig.center.y();
    float bottom = top - 1.f;

    // Define the menu quad
    osg::Vec3 top_left    (left, 1, top);
    osg::Vec3 bottom_left(left, 1, bottom);
    osg::Vec3 bottom_right(right , 1, bottom);
    osg::Vec3 top_right   (right, 1, top);
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
    mGeometry->setDataVariance(osg::Object::DYNAMIC);
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
        mConfig.offset.y() -= 0.001f * mConfig.priority;
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
    auto position = mTrackedPose.position + orientation * mConfig.offset * MWVR::Environment::get().unitsPerMeter();

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
    if (mLayerName == "JournalBooks" )
    {
        mRealRect.left = 0.f;
        mRealRect.top = 0.f;
        mRealRect.right = 1.f;
        mRealRect.bottom = 1.f;
    }

    if (mLayerName == "Notification")
    {
        // The latest widget for notification is always the top one
        // So we just have to stretch the rectangle to the bottom
        // TODO: This might get deprecated with this new system?
        mRealRect.bottom = 1.f;
    }
}

void VRGUILayer::update()
{
    if (mConfig.trackingMode != TrackingMode::Menu)
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
    float res = static_cast<float>(mConfig.spatialResolution) / Environment::get().unitsPerMeter();

    if (mConfig.sizingMode == SizingMode::Auto)
    {
        mTransform->setScale(osg::Vec3(w / res, 1.f, h / res));
    }
    if (mLayerName == "Notification")
    {
        auto viewSize = MyGUI::RenderManager::getInstance().getViewSize();
        h = (1.f - mRealRect.top) * viewSize.height;
        mTransform->setScale(osg::Vec3(w / res, 1.f, h / res));
    }

    osg::ref_ptr<osg::Vec2Array> texCoords{ new osg::Vec2Array(4) };
    (*texCoords)[0].set(mRealRect.left, 1.f - mRealRect.top);
    (*texCoords)[1].set(mRealRect.left, 1.f - mRealRect.bottom);
    (*texCoords)[2].set(mRealRect.right, 1.f - mRealRect.bottom);
    (*texCoords)[3].set(mRealRect.right, 1.f - mRealRect.top);
    mGeometry->setTexCoordArray(0, texCoords);
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

VRGUIManager::VRGUIManager(
    osg::ref_ptr<osgViewer::Viewer> viewer)
    : mOsgViewer(viewer)
{
    mGUIGeometriesRoot->setName("XR GUI Geometry Root");
    mGUICamerasRoot->setName("XR GUI Cameras Root");
    auto* root = viewer->getSceneData();

    SceneUtil::FindByNameVisitor findSceneVisitor("Scene Root");
    root->accept(findSceneVisitor);
    if(!findSceneVisitor.mFoundNode)
    {
        Log(Debug::Error) << "Scene Root doesn't exist";
        return;
    }

    findSceneVisitor.mFoundNode->addChild(mGUIGeometriesRoot);
    root->asGroup()->addChild(mGUICamerasRoot);
        
}

VRGUIManager::~VRGUIManager(void)
{
}

static const LayerConfig createDefaultConfig(int priority, bool background = true, SizingMode sizingMode = SizingMode::Auto)
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
        "Popup"
    };
}
LayerConfig gDefaultConfig = createDefaultConfig(1);
LayerConfig gJournalBooksConfig = createDefaultConfig(2, false, SizingMode::Fixed);
LayerConfig gDefaultWindowsConfig = createDefaultConfig(3, true);
LayerConfig gMessageBoxConfig = createDefaultConfig(6, false, SizingMode::Auto);;
LayerConfig gNotificationConfig = createDefaultConfig(7, false, SizingMode::Fixed);;

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
        ""
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
    ""
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
    ""
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
    {"Notification", gNotificationConfig},
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

    float span = sSideBySideAzimuthInterval * (n - 1); // zero index, places lone layers straight ahead
    float low = -span / 2;

    for (int i = 0; i < mSideBySideLayers.size(); i++)
        mSideBySideLayers[i]->setAngle(low + static_cast<float>(i) * sSideBySideAzimuthInterval);
}

void VRGUIManager::insertLayer(const std::string& name)
{
    LayerConfig config = gDefaultConfig;
    auto configIt = gLayerConfigs.find(name);
    if (configIt != gLayerConfigs.end())
    {
        config = configIt->second;
    }
    else
    {
        Log(Debug::Warning) << "Layer " << name << " has no configuration, using default";
    }

    auto layer = std::shared_ptr<VRGUILayer>(new VRGUILayer(
        mGUIGeometriesRoot,
        mGUICamerasRoot,
        name,
        config,
        this
    ));
    mLayers[name] = layer;

    layer->mGeometry->setUserData(new VRGUILayerUserData(mLayers[name]));

    if (config.sideBySide)
    {
        mSideBySideLayers.push_back(layer);
        updateSideBySideLayers();
    }

    if (config.trackingMode == TrackingMode::Menu)
        layer->updateTracking(mHeadPose);
}

void VRGUIManager::insertWidget(MWGui::Layout* widget)
{
    auto* layer = widget->mMainWidget->getLayer();
    auto name = layer->getName();

    auto it = mLayers.find(name);
    if (it == mLayers.end())
    {
        insertLayer(name);
        it = mLayers.find(name);
        if (it == mLayers.end())
        {
            Log(Debug::Error) << "Failed to insert layer " << name;
            return;
        }
    }

    it->second->insertWidget(widget);

    if (it->second.get() != mFocusLayer)
        widget->setLayerPick(false);
}

void VRGUIManager::removeLayer(const std::string& name)
{
    auto it = mLayers.find(name);
    if (it == mLayers.end())
        return;
    
    auto layer = it->second;

    for (auto it2 = mSideBySideLayers.begin(); it2 < mSideBySideLayers.end(); it2++)
    {
        if (*it2 == layer)
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
    auto name = layer->getName();

    auto it = mLayers.find(name);
    if (it == mLayers.end())
    {
        //Log(Debug::Warning) << "Tried to remove widget from nonexistent layer " << name;
        return;
    }

    it->second->removeWidget(widget);
    if (it->second->widgetCount() == 0)
    {
        removeLayer(name);
    }
}

void VRGUIManager::setVisible(MWGui::Layout* widget, bool visible)
{
    auto* layer = widget->mMainWidget->getLayer();
    auto name = layer->getName();

    //Log(Debug::Verbose) << "setVisible (" << name << "): " << visible;
    if (layerBlacklist.find(name) != layerBlacklist.end())
    {
        Log(Debug::Verbose) << "Blacklisted";
        // Never pick an invisible layer
        widget->setLayerPick(false);
        return;
    }

    if (visible)
        insertWidget(widget);
    else
        removeWidget(widget);
}

void VRGUIManager::updateTracking(void)
{
    // Get head pose by reading the camera view matrix to place the GUI in the world.
    osg::Vec3 eye{};
    osg::Vec3 center{};
    osg::Vec3 up{};
    Pose headPose{};
    auto* world = MWBase::Environment::get().getWorld();
    if (!world)
        return;
    auto* camera = world->getRenderingManager().getCamera()->getOsgCamera();
    if (!camera)
        return;
    camera->getViewMatrixAsLookAt(eye, center, up);
    headPose.position = eye;
    headPose.orientation = camera->getViewMatrix().getRotate();
    headPose.orientation = headPose.orientation.inverse();

    mHeadPose = headPose;

    for (auto& layer : mLayers)
        layer.second->updateTracking(mHeadPose);
}

bool VRGUIManager::updateFocus()
{
    auto* anim = MWVR::Environment::get().getPlayerAnimation();
    if (anim && anim->mPointerTarget.mHit)
    {
        std::shared_ptr<VRGUILayer> newFocusLayer = nullptr;
        auto* node = anim->mPointerTarget.mHitNode;
        if (node->getName() == "VRGUILayer")
        {
            VRGUILayerUserData* userData = static_cast<VRGUILayerUserData*>(node->getUserData());
            newFocusLayer = userData->mLayer.lock();
        }

        if (newFocusLayer && newFocusLayer->mLayerName != "Notification")
        {
            setFocusLayer(newFocusLayer.get());
            computeGuiCursor(anim->mPointerTarget.mHitPointLocal);
            return true;
        }
    }
    return false;
}

void VRGUIManager::setFocusLayer(VRGUILayer* layer)
{
    if (layer == mFocusLayer)
        return;

    if (mFocusLayer)
    {
        mFocusLayer->mWidgets.front()->setLayerPick(false);
    }
    mFocusLayer = layer;
    if (mFocusLayer)
    {
        Log(Debug::Verbose) << "Set focus layer to " << mFocusLayer->mWidgets.front()->mMainWidget->getLayer()->getName();
        mFocusLayer->mWidgets.front()->setLayerPick(true);
    }
    else
    {
        Log(Debug::Verbose) << "Set focus layer to null";
    }
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
        auto width = viewSize.width * rect.width();
        auto height = viewSize.height * rect.height();
        auto left = viewSize.width * rect.left;
        auto bottom = viewSize.height * rect.bottom;
        x = width * x + left;
        y = bottom - height * y;
    }

    mGuiCursor.x() = (int)x;
    mGuiCursor.y() = (int)y;

    MyGUI::InputManager::getInstance().injectMouseMove((int)x, (int)y, 0);
    MWBase::Environment::get().getWindowManager()->setCursorActive(true);
}

}
