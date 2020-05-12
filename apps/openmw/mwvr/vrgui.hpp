#ifndef OPENXR_MENU_HPP
#define OPENXR_MENU_HPP

#include <map>
#include <set>
#include <regex>
#include <MyGUI_Widget.h>

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
        Menu, //!< Menu quads with fixed position based on head tracking.
        HudLeftHand, //!< Hud quads tracking the left hand every frame
        HudRightHand, //!< Hud quads tracking the right hand every frame
    };

    // Some UI elements should occupy predefined geometries
    // Others should grow/shrink freely
    enum class SizingMode
    {
        Auto,
        Fixed
    };

    struct LayerConfig
    {
        int priority; //!< Higher priority shows over lower priority windows.
        bool sideBySide; //!< Resize layer window to occupy full quad
        osg::Vec4 backgroundColor; //!< Background color of layer
        osg::Vec3 offset; //!< Offset from tracked node in meters
        osg::Vec2 center; //!< Model space centerpoint of menu geometry. All menu geometries have model space lengths of 1 in each dimension. Use this to affect how geometries grow with changing size.
        osg::Vec2 extent; //!< Spatial extent of the layer in meters when using Fixed sizing mode
        int spatialResolution; //!< Pixels when using the Auto sizing mode. \note Meters per pixel of the GUI viewport, not the RTT texture.
        osg::Vec2i pixelResolution; //!< Pixel resolution of the RTT texture
        osg::Vec2 myGUIViewSize; //!< Resizable elements are resized to this (fraction of full view)
        SizingMode sizingMode; //!< How to size the layer
        TrackingMode trackingMode; //!< Tracking mode

        bool operator<(const LayerConfig& rhs) const { return priority < rhs.priority; }
    };

    class VRGUILayer
    {
    public:
        VRGUILayer(
            osg::ref_ptr<osg::Group> geometryRoot,
            osg::ref_ptr<osg::Group> cameraRoot,
            std::string filter,
            LayerConfig config,
            VRGUIManager* parent);
        ~VRGUILayer();

        osg::Camera* camera();

        osg::ref_ptr<osg::Texture2D> menuTexture();

        void setAngle(float angle);
        void updateTracking(const Pose& headPose = {});
        void updatePose();
        void updateRect();
        void update();

        void insertWidget(MWGui::Layout* widget);
        void removeWidget(MWGui::Layout* widget);
        int  widgetCount() { return mWidgets.size(); }

        bool operator<(const VRGUILayer& rhs) const { return mConfig.priority < rhs.mConfig.priority; }

    public:
        Pose mTrackedPose{};
        LayerConfig mConfig;
        std::string mFilter;
        std::vector<MWGui::Layout*> mWidgets;
        osg::ref_ptr<osg::Group> mGeometryRoot;
        osg::ref_ptr<osg::Geometry> mGeometry{ new osg::Geometry };
        osg::ref_ptr<osg::PositionAttitudeTransform> mTransform{ new osg::PositionAttitudeTransform };
        osg::ref_ptr<osg::Group> mCameraRoot;
        osg::ref_ptr<GUICamera> mGUICamera;
        osg::ref_ptr<osg::Camera> mMyGUICamera{ nullptr };
        MyGUI::FloatRect mRealRect{};
        osg::Quat mRotation{ 0,0,0,1 };
    };

    class VRGUILayerUserData : public osg::Referenced
    {
    public:
        VRGUILayerUserData(std::shared_ptr<VRGUILayer> layer) : mLayer(layer) {};

        std::weak_ptr<VRGUILayer> mLayer;
    };

    class VRGUIManager
    {
    public:
        VRGUIManager(
            osg::ref_ptr<osgViewer::Viewer> viewer);

        ~VRGUIManager(void);

        void showGUIs(bool show);

        void setVisible(MWGui::Layout*, bool visible);

        void updateSideBySideLayers();

        void insertLayer(const std::string& name);

        void insertWidget(MWGui::Layout* widget);

        void removeLayer(const std::string& name);

        void removeWidget(MWGui::Layout* widget);

        void updateTracking(void);

        void updateFocus();

        void setFocusLayer(VRGUILayer* layer);

        osg::Vec2i guiCursor() { return mGuiCursor; };

    private:
        void computeGuiCursor(osg::Vec3 hitPoint);

        osg::ref_ptr<osgViewer::Viewer> mOsgViewer{ nullptr };

        osg::ref_ptr<osg::Group> mGUIGeometriesRoot{ new osg::Group };
        osg::ref_ptr<osg::Group> mGUICamerasRoot{ new osg::Group };

        std::map<std::string, std::shared_ptr<VRGUILayer>> mLayers;
        std::vector<std::shared_ptr<VRGUILayer> > mSideBySideLayers;

        int         mVisibleMenus{ 0 };
        Pose        mHeadPose{};
        osg::Vec2i  mGuiCursor{};
        VRGUILayer* mFocusLayer{ nullptr };
    };
}

#endif
