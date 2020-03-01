#ifndef OPENXR_MENU_HPP
#define OPENXR_MENU_HPP

#include <osg/Geometry>
#include <osg/TexMat>
#include <osg/Texture2D>
#include <osg/Camera>
#include <osg/PositionAttitudeTransform>

#include "openxrview.hpp"
#include "openxrlayer.hpp"

struct XrCompositionLayerQuad;
namespace MWVR
{
    class Menus;

    class OpenXRMenu
    {
    public:
        OpenXRMenu(
            osg::ref_ptr<osg::Group> parent,
            osg::ref_ptr<osg::Node> menuSubgraph,
            const std::string& title, 
            osg::Vec2 extent_meters,
            Pose pose,
            int width,
            int height,
            const osg::Vec4& clearColor, 
            osgViewer::Viewer* viewer);
        ~OpenXRMenu();
        const std::string& title() const { return mTitle; }
        void updateCallback();

        void preRenderCallback(osg::RenderInfo& renderInfo);
        void postRenderCallback(osg::RenderInfo& renderInfo);
        osg::Camera* camera();

        osg::ref_ptr<osg::Texture2D> menuTexture();

        void updatePose(Pose pose);

    public:
        std::string mTitle;
        osg::ref_ptr<osg::Group> mParent;
        osg::ref_ptr<osg::Geometry> mGeometry{ new osg::Geometry };
        osg::ref_ptr<osg::Geode> mGeode{ new osg::Geode };
        osg::ref_ptr<osg::PositionAttitudeTransform> mTransform{ new osg::PositionAttitudeTransform };

        osg::ref_ptr<osg::Node> mMenuSubgraph;
        osg::ref_ptr<osg::StateSet> mStateSet{ new osg::StateSet };
        osg::ref_ptr<Menus> mMenuCamera;
    };

    class OpenXRMenuManager
    {
    public:
        OpenXRMenuManager(
            osg::ref_ptr<osgViewer::Viewer> viewer);

        ~OpenXRMenuManager(void);

        void showMenus(bool show);

        void updatePose(void);

        OpenXRMenu* getMenu(void) const { return mMenu.get(); }

    private:
        Pose mPose{};
        osg::ref_ptr<osgViewer::Viewer> mOsgViewer{ nullptr };
        osg::ref_ptr<osg::Group> mMenusRoot{ new osg::Group };
        osg::ref_ptr<osg::Node> mGuiRoot{ nullptr };
        std::unique_ptr<OpenXRMenu> mMenu{ nullptr };
    };
}

#endif
