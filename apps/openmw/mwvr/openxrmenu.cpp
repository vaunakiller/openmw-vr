#include "openxrmenu.hpp"
#include "vrenvironment.hpp"
#include "openxrsession.hpp"
#include "openxrmanagerimpl.hpp"
#include "vranimation.hpp"
#include <openxr/openxr.h>
#include <osg/Texture2D>
#include <osg/ClipNode>
#include <osg/FrontFace>
#include <osg/BlendFunc>
#include <osg/Depth>
#include <components/sceneutil/visitor.hpp>
#include <components/sceneutil/shadow.hpp>
#include <osgViewer/Renderer>
#include "../mwrender/util.hpp"
#include "../mwrender/renderbin.hpp"

namespace MWVR
{

/// Draw callback for RTT that can be used to regenerate mipmaps
/// either as a predraw before use or a postdraw after RTT.
class MipmapCallback : public osg::Camera::DrawCallback
{
public:
    MipmapCallback(osg::Texture2D* texture)
        : mTexture(texture)
    {}

    void operator()(osg::RenderInfo& info) const override;

private:

    osg::ref_ptr<osg::Texture2D> mTexture;
};

/// RTT camera used to draw the osg GUI to a texture
class GUICamera : public osg::Camera
{
public:
    GUICamera()
    {
        setRenderOrder(osg::Camera::PRE_RENDER);
        setClearMask(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Make the texture just a little transparent to feel more natural in the game world.
        setClearColor(osg::Vec4(0.f,0.f,0.f,.75f));

        setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT);
        setReferenceFrame(osg::Camera::ABSOLUTE_RF);
        setComputeNearFarMode(osg::CullSettings::DO_NOT_COMPUTE_NEAR_FAR);
        setName("MenuCamera");

        setCullMask(SceneUtil::Mask_GUI);
        setNodeMask(SceneUtil::Mask_RenderToTexture);

        unsigned int rttSize = 4000;
        setViewport(0, 0, rttSize, rttSize);

        // No need for Update traversal since the mSceneRoot is already updated as part of the main scene graph
        // A double update would mess with the light collection (in addition to being plain redundant)
        setUpdateCallback(new MWRender::NoTraverseCallback);

        // Create the texture
        mTexture = new osg::Texture2D;
        mTexture->setTextureSize(rttSize, rttSize);
        mTexture->setInternalFormat(GL_RGBA);
        mTexture->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR_MIPMAP_LINEAR);
        mTexture->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
        mTexture->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE);
        mTexture->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE);
        attach(osg::Camera::COLOR_BUFFER, mTexture);
        // Need to regenerate mipmaps every frame
        setPostDrawCallback(new MipmapCallback(mTexture));

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


    OpenXRMenu::OpenXRMenu(
        osg::ref_ptr<osg::Group> geometryRoot,
        osg::ref_ptr<osg::Group> cameraRoot,
        osg::ref_ptr<osg::Node> menuSubgraph,
        const std::string& title,
        osg::Vec2 extent_meters,
        Pose pose,
        int width,
        int height,
        const osg::Vec4& clearColor,
        osgViewer::Viewer* viewer)
        : mTitle(title)
        , mGeometryRoot(geometryRoot)
        , mCameraRoot(cameraRoot)
        , mMenuSubgraph(menuSubgraph)
    {
        osg::ref_ptr<osg::Vec3Array> vertices{ new osg::Vec3Array(4) };
        osg::ref_ptr<osg::Vec2Array> texCoords{ new osg::Vec2Array(4) };
        osg::ref_ptr<osg::Vec3Array> normals{ new osg::Vec3Array(1) };

        // Units are divided by 2 because geometry has an extent of 2 (-1 to 1)
        auto extent_units = extent_meters * Environment::get().unitsPerMeter() / 2.f;

        // Define the menu quad
        osg::Vec3 top_left    (-1, 1, 1);
        osg::Vec3 bottom_left(-1, -1, 1);
        osg::Vec3 bottom_right(1, -1, 1);
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
        mGeometry->setName("XR Menu Geometry");
        //mGeode->addDrawable(mGeometry);

        // Define the camera that will render the menu texture
        mGUICamera = new GUICamera();
        mGUICamera->setScene(menuSubgraph);

        // Define state set that allows rendering with transparency
        mStateSet->setTextureAttributeAndModes(0, menuTexture(), osg::StateAttribute::ON);
        mStateSet->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
        mStateSet->setMode(GL_BLEND, osg::StateAttribute::ON);
        mStateSet->setAttributeAndModes(new osg::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
        mStateSet->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
        mGeometry->setStateSet(mStateSet);

        // Position in the game world
        mTransform->setScale(osg::Vec3(extent_units.x(), extent_units.y(), 1.f));
        mTransform->setAttitude(pose.orientation);
        mTransform->setPosition(pose.position);
        mTransform->addChild(mGeometry);
        //mTransform->addChild(VRAnimation::createPointerGeometry());

        // Add to scene graph
        mGeometryRoot->addChild(mTransform);
        mCameraRoot->addChild(mGUICamera);
    }

    OpenXRMenu::~OpenXRMenu()
    {
        mGeometryRoot->removeChild(mTransform);
        mCameraRoot->removeChild(mGUICamera);
    }

    void OpenXRMenu::updateCallback()
    {
    }

    void MipmapCallback::operator()(osg::RenderInfo& renderInfo) const
    {
        auto* gl = renderInfo.getState()->get<osg::GLExtensions>();
        auto* tex = mTexture->getTextureObject(renderInfo.getContextID());
        if (tex)
        {
            tex->bind();
            gl->glGenerateMipmap(tex->target());
        }
    }

    osg::Camera* OpenXRMenu::camera()
    {
        return mGUICamera.get();
    }

    osg::ref_ptr<osg::Texture2D> OpenXRMenu::menuTexture()
    {
        if (mGUICamera)
            return mGUICamera->getTexture();
        return nullptr;
    }

    void OpenXRMenu::updatePose(Pose pose)
    {
        mTransform->setAttitude(pose.orientation);
        mTransform->setPosition(pose.position);
    }


    OpenXRMenuManager::OpenXRMenuManager(
        osg::ref_ptr<osgViewer::Viewer> viewer)
        : mOsgViewer(viewer)
    {
        mGUIGeometriesRoot->setName("XR GUI Geometry Root");
        mGUICamerasRoot->setName("XR GUI Cameras Root");
        auto* root = viewer->getSceneData();

        SceneUtil::FindByNameVisitor findGUIVisitor("GUI Root");
        root->accept(findGUIVisitor);
        mGuiRoot = findGUIVisitor.mFoundNode;
        if (!mGuiRoot)
        {
            Log(Debug::Error) << "GUI Root doesn't exist";
            return;
        }

        SceneUtil::FindByNameVisitor findSceneVisitor("Scene Root");
        root->accept(findSceneVisitor);
        if(!findSceneVisitor.mFoundNode)
        {
            Log(Debug::Error) << "Scene Root doesn't exist";
            return;
        }

        Log(Debug::Verbose) << "Root note: " << root->getName();

        findSceneVisitor.mFoundNode->addChild(mGUIGeometriesRoot);
        root->asGroup()->addChild(mGUICamerasRoot);
        
    }

    OpenXRMenuManager::~OpenXRMenuManager(void)
    {
    }

    void OpenXRMenuManager::showMenus(bool show)
    {
        // TODO: Configurable menu dimensions
        int width = 1000;
        int height = 1000;

        if (show && !mMenu)
        {
            updatePose();
            mMenu.reset(new OpenXRMenu(
                mGUIGeometriesRoot,
                mGUICamerasRoot,
                mGuiRoot,
                "Main Menu",
                osg::Vec2(1.5f, 1.5f),
                mPose,
                width,
                height,
                mOsgViewer->getCamera()->getClearColor(),
                mOsgViewer
            ));
            Log(Debug::Error) << "Created menu";
        }
        else if (!show && mMenu)
        {
            mMenu = nullptr;
            Log(Debug::Error) << "Destroyed menu";
        }
    }

    void OpenXRMenuManager::updatePose(void)
    {
        osg::Vec3 eye{};
        osg::Vec3 center{};
        osg::Vec3 up{};

        auto* camera = mOsgViewer->getCamera();
        if (!camera)
        {
            Log(Debug::Error) << "osg viewer has no camera";
            return;
        }

        camera->getViewMatrixAsLookAt(eye, center, up);

        // Position the menu about two thirds of a meter in front of the player
        osg::Vec3 dir = center - eye;
        dir.normalize();
        mPose.position = eye + dir * Environment::get().unitsPerMeter() * 2.f / 3.f;


        mPose.orientation = camera->getViewMatrix().getRotate().inverse();

        if (mMenu)
            mMenu->updatePose(mPose);

        Log(Debug::Error) << "New menu pose: " << mPose;
    }
}
