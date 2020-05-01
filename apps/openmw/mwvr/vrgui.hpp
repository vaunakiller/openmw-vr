#ifndef OPENXR_MENU_HPP
#define OPENXR_MENU_HPP

#include <map>
#include <set>
#include <regex>

#include <osg/Geometry>
#include <osg/TexMat>
#include <osg/Texture2D>
#include <osg/Camera>
#include <osg/PositionAttitudeTransform>

#include "openxrview.hpp"
#include "openxrlayer.hpp"

namespace MyGUI
{
class Widget;
class Window;
}

namespace MWGui
{
class Layout;
class WindowBase;
}

struct XrCompositionLayerQuad;
namespace MWVR
{
    class GUICamera;
    class VRGUIManager;

    enum class TrackingMode
    {
        Auto, //!< Update tracking every frame
        Manual //!< Update tracking only on user request or when GUI visibility changes.
    };

    struct LayerConfig
    {
        bool stretch; //!< Resize layer window to occupy full quad
        osg::Vec4 backgroundColor; //!< Background color of layer
        osg::Quat rotation; //!< Rotation relative to the tracking node
        osg::Vec3 offset; //!< Offset from tracked node in meters
        osg::Vec2 extent; //!< Spatial extent of the layer in meters
        osg::Vec2i resolution; //!< Pixel resolution of the texture
        TrackedLimb trackedLimb; //!< Which limb to track
        TrackingMode trackingMode; //!< Tracking mode
        bool vertical; //!< Make layer vertical regardless of tracking orientation
    };

    class VRGUILayer
    {
    public:
        VRGUILayer(
            osg::ref_ptr<osg::Group> geometryRoot,
            osg::ref_ptr<osg::Group> cameraRoot,
            int width,
            int height,
            std::string filter,
            LayerConfig config,
            MWGui::Layout* widget,
            VRGUIManager* parent);
        ~VRGUILayer();

        osg::Camera* camera();

        osg::ref_ptr<osg::Texture2D> menuTexture();

        void updatePose();
        void update();

    public:
        Pose mTrackedPose{};
        Pose mLayerPose{};
        LayerConfig mConfig;
        std::string mFilter;
        MWGui::Layout* mWidget;
        MWGui::WindowBase* mWindow;
        MyGUI::Window* mMyGUIWindow;
        VRGUIManager* mParent;
        osg::ref_ptr<osg::Group> mGeometryRoot;
        osg::ref_ptr<osg::Geometry> mGeometry{ new osg::Geometry };
        osg::ref_ptr<osg::PositionAttitudeTransform> mTransform{ new osg::PositionAttitudeTransform };

        osg::ref_ptr<osg::Group> mCameraRoot;
        osg::ref_ptr<osg::StateSet> mStateSet{ new osg::StateSet };
        osg::ref_ptr<GUICamera> mGUICamera;
    };

    class VRGUILayerUserData : public osg::Referenced
    {
    public:
        VRGUILayerUserData(VRGUILayer* layer) : mLayer(layer) {};

        VRGUILayer* mLayer;
    };

    class VRGUIManager
    {
    public:
        VRGUIManager(
            osg::ref_ptr<osgViewer::Viewer> viewer);

        ~VRGUIManager(void);

        void showGUIs(bool show);

        void setVisible(MWGui::Layout*, bool visible);

        void updatePose(void);

        void setFocusLayer(VRGUILayer* layer);

    private:
        osg::ref_ptr<osgViewer::Viewer> mOsgViewer{ nullptr };

        osg::ref_ptr<osg::Group> mGUIGeometriesRoot{ new osg::Group };
        osg::ref_ptr<osg::Group> mGUICamerasRoot{ new osg::Group };

        std::map<std::string, std::unique_ptr<VRGUILayer>> mLayers;

        VRGUILayer* mFocusLayer{ nullptr };
    };
}

#endif
