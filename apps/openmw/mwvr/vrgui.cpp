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
#include "../mwbase/world.hpp"
#include "../mwbase/environment.hpp"
#include "../mwgui/windowbase.hpp"

#include <MyGUI_Widget.h>
#include <MyGUI_ILayer.h>
#include <MyGUI_InputManager.h>
#include <MyGUI_WidgetManager.h>
#include <MyGUI_Window.h>

namespace MWVR
{

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

        setCullMask(SceneUtil::Mask_GUI);
        setNodeMask(SceneUtil::Mask_RenderToTexture);

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
    int width,
    int height,
    std::string filter,
    LayerConfig config,
    MWGui::Layout* widget,
    VRGUIManager* parent)
    : mConfig(config)
    , mFilter(filter)
    , mWidget(widget)
    , mWindow(dynamic_cast<MWGui::WindowBase*>(mWidget))
    , mMyGUIWindow(dynamic_cast<MyGUI::Window*>(mWidget->mMainWidget))
    , mParent(parent)
    , mGeometryRoot(geometryRoot)
    , mCameraRoot(cameraRoot)
{
    osg::ref_ptr<osg::Vec3Array> vertices{ new osg::Vec3Array(4) };
    osg::ref_ptr<osg::Vec2Array> texCoords{ new osg::Vec2Array(4) };
    osg::ref_ptr<osg::Vec3Array> normals{ new osg::Vec3Array(1) };

    // Units are divided by 2 because geometry has an extent of 2 (-1 to 1)
    auto extent_units = config.extent * Environment::get().unitsPerMeter() / 2.f;

    // Define the menu quad
    osg::Vec3 top_left    (-1, 1, 1);
    osg::Vec3 bottom_left(-1, 1, -1);
    osg::Vec3 bottom_right(1, 1, -1);
    osg::Vec3 top_right   (1, 1, 1);
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
    mGeometry->setUserData(new VRGUILayerUserData(this));

    // Create the camera that will render the menu texture
    mGUICamera = new GUICamera(width, height, config.backgroundColor);
    osgMyGUI::RenderManager& renderManager = static_cast<osgMyGUI::RenderManager&>(MyGUI::RenderManager::getInstance());
    mGUICamera->setScene(renderManager.createGUICamera(osg::Camera::NESTED_RENDER, filter));

    // Define state set that allows rendering with transparency
    mStateSet->setTextureAttributeAndModes(0, menuTexture(), osg::StateAttribute::ON);
    mStateSet->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    mStateSet->setMode(GL_BLEND, osg::StateAttribute::ON);
    mStateSet->setAttributeAndModes(new osg::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    mStateSet->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
    mGeometry->setStateSet(mStateSet);

    // Position in the game world
    mTransform->setScale(osg::Vec3(extent_units.x(), 1.f, extent_units.y()));
    mTransform->addChild(mGeometry);

    // Add to scene graph
    mGeometryRoot->addChild(mTransform);
    mCameraRoot->addChild(mGUICamera);

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

void VRGUILayer::updatePose()
{
    osg::Vec3 eye{};
    osg::Vec3 center{};
    osg::Vec3 up{};

    // Get head pose by reading the camera view matrix to place the GUI in the world.
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

    if (mConfig.trackedLimb == TrackedLimb::HEAD)
    {
        mTrackedPose = headPose;
        mTrackedPose.orientation = mTrackedPose.orientation.inverse();
    }
    else
    {
        // If it's not head, it's one of the hands, so i don't bother checking
        auto* session = MWVR::Environment::get().getSession();
        auto& poses = session->predictedPoses(OpenXRSession::PredictionSlice::Predraw);
        mTrackedPose = poses.hands[(int)TrackedSpace::STAGE][(int)mConfig.trackedLimb];
        // World position is the head, so must add difference between head and hand in tracking space to world pose
        mTrackedPose.position = mTrackedPose.position * MWVR::Environment::get().unitsPerMeter() - poses.head[(int)TrackedSpace::STAGE].position * MWVR::Environment::get().unitsPerMeter() + headPose.position;
    }

    mLayerPose.orientation = mConfig.rotation * mTrackedPose.orientation;

    if (mConfig.vertical)
    {
        // Force layer to be vertical
        auto axis = osg::Z_AXIS;
        osg::Quat vertical;
        auto local = mLayerPose.orientation * axis;
        vertical.makeRotate(local, axis);
        mLayerPose.orientation = mLayerPose.orientation * vertical;
    }
    // Orient the offset and move the layer
    mLayerPose.position = mTrackedPose.position + mLayerPose.orientation * mConfig.offset * MWVR::Environment::get().unitsPerMeter();

    mTransform->setAttitude(mLayerPose.orientation);
    mTransform->setPosition(mLayerPose.position);
}

void VRGUILayer::update()
{
    if (mConfig.trackingMode == TrackingMode::Auto)
        updatePose();

    if (mConfig.stretch)
    {
        if (mWindow && mMyGUIWindow)
        {
            mWindow->setCoordf(0.f, 0.f, 1.f, 1.f);
            mWindow->onWindowResize(mMyGUIWindow);
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

void VRGUIManager::showGUIs(bool show)
{
}

LayerConfig gDefaultConfig = LayerConfig
{
    true, // stretch
    osg::Vec4{0.f,0.f,0.f,.75f}, // background
    osg::Quat{}, // rotation
    osg::Vec3(0.f,1.f,0.f), // offset
    osg::Vec2(1.f, 1.f), // extent (meters)
    osg::Vec2i(2024,2024), // resolution (pixels)
    TrackedLimb::HEAD,
    TrackingMode::Manual,
    true // vertical
};

LayerConfig gStatusHUDConfig = LayerConfig
{
    false, // stretch
    osg::Vec4{0.f,0.f,0.f,0.f}, // background
    osg::Quat{}, // rotation
    osg::Vec3(0.f,.0f,.2f), // offset (meters)
    osg::Vec2(.2f, .2f), // extent (meters)
    osg::Vec2i(1024,512), // resolution (pixels)
    TrackedLimb::RIGHT_HAND,
    TrackingMode::Auto,
    false // vertical
};

LayerConfig gMinimapHUDConfig = LayerConfig
{
    false, // stretch
    osg::Vec4{0.f,0.f,0.f,0.f}, // background
    osg::Quat{}, // rotation
    osg::Vec3(0.f,.0f,.2f), // offset (meters)
    osg::Vec2(.2f, .2f), // extent (meters)
    osg::Vec2i(1024,512), // resolution (pixels)
    TrackedLimb::RIGHT_HAND,
    TrackingMode::Auto,
    false // vertical
};

LayerConfig gPopupConfig = LayerConfig
{
    false, // stretch
    osg::Vec4{0.f,0.f,0.f,0.f}, // background
    osg::Quat{}, // rotation
    osg::Vec3(0.f,0.f,.2f), // offset
    osg::Vec2(.2f, .2f), // extent (meters)
    osg::Vec2i(1024,1024),
    TrackedLimb::RIGHT_HAND,
    TrackingMode::Auto,
    false // vertical
};

LayerConfig gWindowsConfig = gDefaultConfig;
LayerConfig gJournalBooksConfig = LayerConfig
{
    true, // stretch
    gDefaultConfig.backgroundColor,
    osg::Quat{}, // rotation
    gDefaultConfig.offset,
    gDefaultConfig.extent,
    gDefaultConfig.resolution,
    TrackedLimb::HEAD,
    TrackingMode::Manual,
    true // vertical
};
LayerConfig gSpellWindowConfig = LayerConfig
{
    true, // stretch
    gDefaultConfig.backgroundColor,
    osg::Quat{-osg::PI_2, osg::Z_AXIS}, // rotation
    gDefaultConfig.offset,
    gDefaultConfig.extent,
    gDefaultConfig.resolution,
    TrackedLimb::HEAD,
    TrackingMode::Manual,
    true // vertical
};
LayerConfig gInventoryWindowConfig = LayerConfig
{
    true, // stretch
    gDefaultConfig.backgroundColor,
    osg::Quat{}, // rotation
    gDefaultConfig.offset,
    gDefaultConfig.extent,
    gDefaultConfig.resolution,
    TrackedLimb::HEAD,
    TrackingMode::Manual,
    true // vertical
};
LayerConfig gMapWindowConfig = LayerConfig
{
    true, // stretch
    gDefaultConfig.backgroundColor,
    osg::Quat{osg::PI, osg::Z_AXIS}, // rotation
    gDefaultConfig.offset,
    gDefaultConfig.extent,
    gDefaultConfig.resolution,
    TrackedLimb::HEAD,
    TrackingMode::Manual,
    true // vertical
};
LayerConfig gStatsWindowConfig = LayerConfig
{
    true, // stretch
    gDefaultConfig.backgroundColor,
    osg::Quat{osg::PI_2, osg::Z_AXIS}, // rotation
    gDefaultConfig.offset,
    gDefaultConfig.extent,
    gDefaultConfig.resolution,
    TrackedLimb::HEAD,
    TrackingMode::Manual,
    true // vertical
};


static std::map<std::string, LayerConfig&> gLayerConfigs =
{
    {"StatusHUD", gStatusHUDConfig},
    {"MinimapHUD", gMinimapHUDConfig},
    {"Popup", gPopupConfig},
    {"Windows", gWindowsConfig},
    {"JournalBooks", gJournalBooksConfig},
    {"SpellWindow", gSpellWindowConfig},
    {"InventoryWindow", gInventoryWindowConfig},
    {"MapWindow", gMapWindowConfig},
    {"StatsWindow", gStatsWindowConfig},
    {"Default", gDefaultConfig},
};

static std::set<std::string> layerBlacklist =
{
    "Overlay"
};

void VRGUIManager::setVisible(MWGui::Layout* widget, bool visible)
{
    auto* layer = widget->mMainWidget->getLayer();
    //if (!layer)
    //{
    //    Log(Debug::Warning) << "Hark! MyGUI has betrayed us. The widget " << widget->mMainWidget->getName() << " has no layer";
    //    return;
    //}
    auto name = layer->getName();

    Log(Debug::Verbose) << "setVisible (" << name << "): " << visible;
    if (layerBlacklist.find(name) != layerBlacklist.end())
    {
        Log(Debug::Verbose) << "Blacklisted";
        // Never pick an invisible layer
        widget->setLayerPick(false);
        return;
    }
    if (visible)
    {
        if (mLayers.find(name) == mLayers.end())
        {
            LayerConfig config = gDefaultConfig;
            auto configIt = gLayerConfigs.find(name);
            if (configIt != gLayerConfigs.end())
                config = configIt->second;

            mLayers[name] = std::unique_ptr<VRGUILayer>(new VRGUILayer(
                mGUIGeometriesRoot,
                mGUICamerasRoot,
                2048,
                2048,
                name,
                config,
                widget,
                this
            ));

            // Default new layer's pick to false
            widget->setLayerPick(false);

            Log(Debug::Verbose) << "Created GUI layer " << name;
        }
        updatePose();
    }
    else
    {
        auto it = mLayers.find(name);
        if (it != mLayers.end())
        {
            if (it->second.get() == mFocusLayer)
                setFocusLayer(nullptr);
            mLayers.erase(it);
            Log(Debug::Verbose) << "Erased GUI layer " << name;
        }
    }
}

void VRGUIManager::updatePose(void)
{
    for (auto& layer : mLayers)
        layer.second->updatePose();
}

void VRGUIManager::setFocusLayer(VRGUILayer* layer)
{
    if (mFocusLayer)
        mFocusLayer->mWidget->setLayerPick(false);
    mFocusLayer = layer;
    if (mFocusLayer)
        mFocusLayer->mWidget->setLayerPick(true);
    
}

}
