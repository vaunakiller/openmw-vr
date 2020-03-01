#include "openxrmenu.hpp"
#include "openxrenvironment.hpp"
#include "openxrsession.hpp"
#include "openxrmanagerimpl.hpp"
#include <openxr/openxr.h>
#include <osg/Texture2D>
#include <components/sceneutil/visitor.hpp>

namespace MWVR
{
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
        osg::ref_ptr<osg::Group> menuSubgraph,
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
        mMenuTexture->setTextureSize(width, height);
        mMenuTexture->setInternalFormat(GL_RGBA);
        mMenuTexture->setFilter(osg::Texture2D::MIN_FILTER, osg::Texture2D::LINEAR);
        mMenuTexture->setFilter(osg::Texture2D::MAG_FILTER, osg::Texture2D::LINEAR);

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

        mStateSet->setTextureAttributeAndModes(0, mMenuTexture, osg::StateAttribute::ON);
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



        mMenuCamera->setClearColor(clearColor);
        mMenuCamera->setClearMask(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


        // This is based on the osgprerender example.
        // I'm not sure this part is meaningful when all i'm rendering is the GUI.

        //auto& bs = mMenuSubgraph->getBound();
        //if (!bs.valid())
        //    Log(Debug::Verbose) << "OpenXRMenu: Invalid bound";
        //float znear = 1.0f * bs.radius();
        //float zfar = 3.0f * bs.radius();
        //float proj_top = 0.25f * znear;
        //float proj_right = 0.5f * znear;
        //znear *= 0.9f;
        //zfar *= 1.1f;
        //mMenuCamera->setProjectionMatrixAsFrustum(-proj_right, proj_right, -proj_top, proj_top, znear, zfar);
        //mMenuCamera->setViewMatrixAsLookAt(bs.center() - osg::Vec3(0.0f, 2.0f, 0.0f) * bs.radius(), bs.center(), osg::Vec3(0.0f, 0.0f, 1.0f));


        mMenuCamera->setViewMatrix(viewer->getCamera()->getViewMatrix());
        mMenuCamera->setProjectionMatrix(viewer->getCamera()->getProjectionMatrix());

        // Camera details
        mMenuCamera->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
        mMenuCamera->setViewport(0, 0, width, height);
        mMenuCamera->setRenderOrder(osg::Camera::PRE_RENDER);
        mMenuCamera->setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT);
        mMenuCamera->attach(osg::Camera::COLOR_BUFFER, mMenuTexture);
        mMenuCamera->addChild(mMenuSubgraph);
        mMenuCamera->setCullMask(MWRender::Mask_GUI);
        mMenuCamera->setPreDrawCallback(new PredrawCallback(this));
        mMenuCamera->setPostDrawCallback(new PostdrawCallback(this));
        mMenuCamera->setComputeNearFarMode(osg::CullSettings::DO_NOT_COMPUTE_NEAR_FAR);
        mMenuCamera->setAllowEventFocus(false);

        mParent->addChild(mMenuCamera);
    }

    OpenXRMenu::~OpenXRMenu()
    {
        mParent->removeChild(mTransform);
        mParent->removeChild(mMenuCamera);
    }

    void OpenXRMenu::updateCallback()
    {
    }

    void OpenXRMenu::postRenderCallback(osg::RenderInfo& renderInfo)
    {
        Log(Debug::Verbose) << "Menu: PostRender";
    }

    void OpenXRMenu::updatePose(Pose pose)
    {
        mTransform->setAttitude(pose.orientation);
        mTransform->setPosition(pose.position);
    }

    void OpenXRMenu::preRenderCallback(osg::RenderInfo& renderInfo)
    {
        Log(Debug::Verbose) << "Menu: PreRender";
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
