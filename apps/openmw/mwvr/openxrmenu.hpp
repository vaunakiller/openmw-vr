#ifndef OPENXR_MENU_HPP
#define OPENXR_MENU_HPP

#include <osg/Geometry>
#include <osg/TexMat>
#include <osg/PositionAttitudeTransform>

#include "openxrview.hpp"
#include "openxrlayer.hpp"

struct XrCompositionLayerQuad;
namespace MWVR
{
    class OpenXRMenu
    {
    public:
        OpenXRMenu(
            osg::ref_ptr<osg::Group> root,
            const std::string& title, 
            osg::Vec2 extent_meters,
            Pose pose,
            int width,
            int height,
            const osg::Vec4& clearColor, 
            osg::GraphicsContext* gc);
        ~OpenXRMenu();
        const std::string& title() const { return mTitle; }
        void updateCallback();

        void preRenderCallback(osg::RenderInfo& renderInfo);
        void postRenderCallback(osg::RenderInfo& renderInfo);
        osg::Camera* camera() { return mMenuCamera; }

    protected:
        std::string mTitle;
        osg::ref_ptr<osg::Group> mRoot;
        osg::ref_ptr<osg::Texture> mTexture{ nullptr };
        osg::ref_ptr<osg::TexMat> mTexMat{ nullptr };
        osg::ref_ptr<osg::Camera> mMenuCamera{ new osg::Camera() };
        //osg::ref_ptr<osg::Texture2D> mMenuImage{ new osg::Texture2D() };
        osg::ref_ptr<osg::Geometry> mGeometry{ new osg::Geometry() };
        osg::ref_ptr<osg::Geode> mGeode{ new osg::Geode };
        osg::ref_ptr<osg::PositionAttitudeTransform> mTransform{ new osg::PositionAttitudeTransform() };
    };

    class OpenXRMenuManager
    {
    public:
        OpenXRMenuManager(
            osg::ref_ptr<osgViewer::Viewer> viewer);

        ~OpenXRMenuManager(void);

        void showMenus(bool show);

        void updatePose(Pose pose);

    private:
        Pose pose{};
        osg::ref_ptr<osgViewer::Viewer> mOsgViewer{ nullptr };
        osg::ref_ptr<osg::Group> mMenusRoot{ new osg::Group };
        std::unique_ptr<OpenXRMenu> mMenu{ nullptr };
    };
}

#endif
