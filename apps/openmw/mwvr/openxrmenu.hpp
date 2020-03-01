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
    class OpenXRMenu
    {
    public:
        OpenXRMenu(
            osg::ref_ptr<osg::Group> parent,
            osg::ref_ptr<osg::Group> menuSubgraph,
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
        osg::Camera* camera() { return mMenuCamera; }

        void updatePose(Pose pose);

    protected:
        std::string mTitle;
        osg::ref_ptr<osg::Group> mParent;
        osg::ref_ptr<osg::Geometry> mGeometry{ new osg::Geometry };
        osg::ref_ptr<osg::Geode> mGeode{ new osg::Geode };
        osg::ref_ptr<osg::PositionAttitudeTransform> mTransform{ new osg::PositionAttitudeTransform };

        osg::ref_ptr<osg::Group> mMenuSubgraph;
        osg::ref_ptr<osg::Camera> mMenuCamera{ new osg::Camera };
        osg::ref_ptr<osg::Texture2D> mMenuTexture{ new osg::Texture2D };
        osg::ref_ptr<osg::StateSet> mStateSet{ new osg::StateSet };
    };

    class OpenXRMenuManager
    {
    public:
        OpenXRMenuManager(
            osg::ref_ptr<osgViewer::Viewer> viewer);

        ~OpenXRMenuManager(void);

        void showMenus(bool show);

        void updatePose(void);

    private:
        Pose mPose{};
        osg::ref_ptr<osgViewer::Viewer> mOsgViewer{ nullptr };
        osg::ref_ptr<osg::Group> mMenusRoot{ new osg::Group };
        osg::ref_ptr<osg::Group> mGuiRoot{ new osg::Group };
        std::unique_ptr<OpenXRMenu> mMenu{ nullptr };
    };
}

#endif
