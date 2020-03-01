#include "openxrmenu.hpp"
#include "openxrenvironment.hpp"
#include "openxrsession.hpp"
#include "openxrmanagerimpl.hpp"
#include <openxr/openxr.h>
#include <osg/Texture2D>
#include <osg/ClipNode>
#include <osg/FrontFace>
#include <components/sceneutil/visitor.hpp>
#include <components/sceneutil/shadow.hpp>
#include <osgViewer/Renderer>
#include "../mwrender/util.hpp"

namespace MWVR
{


class Menus : public osg::Camera
{
public:
    Menus()
    {
        setRenderOrder(osg::Camera::PRE_RENDER);
        setClearMask(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT);
        setReferenceFrame(osg::Camera::ABSOLUTE_RF);
        setSmallFeatureCullingPixelSize(Settings::Manager::getInt("small feature culling pixel size", "Water"));
        setComputeNearFarMode(osg::CullSettings::DO_NOT_COMPUTE_NEAR_FAR);
        setName("ReflectionCamera");

        setCullMask(MWRender::Mask_GUI);
        setNodeMask(MWRender::Mask_RenderToTexture);

        unsigned int rttSize = 1000;
        setViewport(0, 0, rttSize, rttSize);

        // No need for Update traversal since the mSceneRoot is already updated as part of the main scene graph
        // A double update would mess with the light collection (in addition to being plain redundant)
        setUpdateCallback(new MWRender::NoTraverseCallback);

        mMenuTexture = new osg::Texture2D;
        mMenuTexture->setTextureSize(rttSize, rttSize);
        mMenuTexture->setInternalFormat(GL_RGB);
        mMenuTexture->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR);
        mMenuTexture->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
        mMenuTexture->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE);
        mMenuTexture->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE);

        attach(osg::Camera::COLOR_BUFFER, mMenuTexture);

        // XXX: should really flip the FrontFace on each renderable instead of forcing clockwise.
        //osg::ref_ptr<osg::FrontFace> frontFace(new osg::FrontFace);
        //frontFace->setMode(osg::FrontFace::CLOCKWISE);
        //getOrCreateStateSet()->setAttributeAndModes(frontFace, osg::StateAttribute::ON);

        //mClipCullNode = new ClipCullNode;
        //addChild(mClipCullNode);

        SceneUtil::ShadowManager::disableShadowsForStateSet(getOrCreateStateSet());
    }

    void setScene(osg::Node* scene)
    {
        if (mScene)
            removeChild(mScene);
        mScene = scene;
        addChild(scene);
    }

    osg::Texture2D* getMenuTexture() const
    {
        return mMenuTexture.get();
    }

private:
    osg::ref_ptr<osg::Texture2D> mMenuTexture;
    osg::ref_ptr<osg::Node> mScene;
};




    class PredrawCallback : public osg::Camera::DrawCallback
    {
    public:
        PredrawCallback(OpenXRMenu* menu)
            : mMenu(menu)
        {}

        void operator()(osg::RenderInfo& info) const override { mMenu->preRenderCallback(info); };

    private:

        OpenXRMenu* mMenu;
    };

    class PostdrawCallback : public osg::Camera::DrawCallback
    {
    public:
        PostdrawCallback(OpenXRMenu* menu)
            : mMenu(menu)
        {}

        void operator()(osg::RenderInfo& info) const override { mMenu->postRenderCallback(info); };

    private:

        OpenXRMenu* mMenu;
    };

    OpenXRMenu::OpenXRMenu(
        osg::ref_ptr<osg::Group> parent,
        osg::ref_ptr<osg::Node> menuSubgraph,
        const std::string& title,
        osg::Vec2 extent_meters,
        Pose pose,
        int width,
        int height,
        const osg::Vec4& clearColor,
        osgViewer::Viewer* viewer)
        : mTitle(title)
        , mParent(parent)
        , mMenuSubgraph(menuSubgraph)
    {
        osg::ref_ptr<osg::Vec3Array> vertices{ new osg::Vec3Array(4) };
        osg::ref_ptr<osg::Vec2Array> texCoords{ new osg::Vec2Array(4) };
        osg::ref_ptr<osg::Vec3Array> normals{ new osg::Vec3Array(1) };
        osg::ref_ptr<osg::Vec4Array> colors{ new osg::Vec4Array(1) };

        extent_meters *= OpenXREnvironment::get().unitsPerMeter() / 2.f;

        float w = extent_meters.x();
        float h = extent_meters.y();

        osg::Vec3 top_left    (-w, h, 1);
        osg::Vec3 bottom_left(-w, -h, 1);
        osg::Vec3 bottom_right(w, -h, 1);
        osg::Vec3 top_right   (w, h, 1);
        (*vertices)[0] = top_left;
        (*vertices)[1] = bottom_left;
        (*vertices)[2] = bottom_right;
        (*vertices)[3] = top_right;
        mGeometry->setVertexArray(vertices);
        (*texCoords)[0].set(0.0f, 0.0f);
        (*texCoords)[1].set(1.0f, 0.0f);
        (*texCoords)[2].set(1.0f, 1.0f);
        (*texCoords)[3].set(0.0f, 1.0f);
        mGeometry->setTexCoordArray(0, texCoords);
        (*normals)[0].set(0.0f, -1.0f, 0.0f);
        (*colors)[0].set(1.0f, 1.0f, 1.0f, 0.0f);
        mGeometry->setNormalArray(normals, osg::Array::BIND_OVERALL);
        mGeometry->setColorArray(colors, osg::Array::BIND_OVERALL);
        mGeometry->addPrimitiveSet(new osg::DrawArrays(GL_QUADS, 0, 4));
        mGeometry->setDataVariance(osg::Object::DYNAMIC);
        mGeometry->setSupportsDisplayList(false);

        mMenuCamera = new Menus();
        mMenuCamera->setScene(menuSubgraph);

        mStateSet->setTextureAttributeAndModes(0, menuTexture(), osg::StateAttribute::ON);
        mStateSet->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
        mGeometry->setStateSet(mStateSet);

        // Units are divided by 2 because geometry has side lengths of 2 units.

        // mTransform->setScale(osg::Vec3(extent_meters.x(), extent_meters.y(), 1.f));
        mTransform->setScale(osg::Vec3(1.f, 1.f, 1.f));
        mTransform->setAttitude(pose.orientation);
        mTransform->setPosition(pose.position);

        mGeode->addDrawable(mGeometry);
        mTransform->addChild(mGeode);
        mParent->addChild(mTransform);



        mParent->addChild(mMenuCamera.get());
    }

    OpenXRMenu::~OpenXRMenu()
    {
        mParent->removeChild(mTransform);
        mParent->removeChild(mMenuCamera.get());
    }

    void OpenXRMenu::updateCallback()
    {
    }

    osg::Camera* OpenXRMenu::camera()
    {
        return mMenuCamera.get();
    }

    osg::ref_ptr<osg::Texture2D> OpenXRMenu::menuTexture()
    {
        if (mMenuCamera)
            return mMenuCamera->getMenuTexture();
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
        mMenusRoot->setName("XR Menus Root");
        auto* root = viewer->getSceneData();

        SceneUtil::FindByNameVisitor findGUIVisitor("GUI Root");
        root->accept(findGUIVisitor);
        mGuiRoot = findGUIVisitor.mFoundNode;
        if (!mGuiRoot)
            throw std::logic_error("Gui root doesn't exist");

        root->asGroup()->addChild(mMenusRoot);
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
                mMenusRoot,
                mGuiRoot,
                "Main Menu",
                osg::Vec2(2.f, 2.f),
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

        // Position the menu half a meter in front of the player
        osg::Vec3 dir = center - eye;
        dir.normalize();
        mPose.position = eye + dir * OpenXREnvironment::get().unitsPerMeter();


        mPose.orientation = camera->getViewMatrix().getRotate().inverse();

        if (mMenu)
            mMenu->updatePose(mPose);

        Log(Debug::Error) << "New menu pose: " << mPose;
    }
}
