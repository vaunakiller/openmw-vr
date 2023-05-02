#ifndef OPENXR_MENU_HPP
#define OPENXR_MENU_HPP

#include <map>
#include <set>
#include <regex>
#include <array>
#include <chrono>
#include <MyGUI_Widget.h>

#include <osg/Geometry>
#include <osg/TexMat>
#include <osg/Texture2D>
#include <osg/Camera>
#include <osg/PositionAttitudeTransform>

#include <components/vr/layer.hpp>
#include <components/vr/trackingsource.hpp>
#include <components/vr/trackinglistener.hpp>
#include <components/vr/trackingpath.hpp>

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

namespace Resource
{
    class ResourceSystem;
}

struct XrCompositionLayerQuad;
namespace MWVR
{
    class VRGUIManager;

    // Some UI elements should occupy predefined geometries
    // Others should grow/shrink freely
    enum class SizingMode
    {
        Auto,
        Fixed
    };

    // Applies to the user pointer only. Any other intersection context
    // will always ignore UI elements.
    enum class Intersectable
    {
        No,
        Yes
    };

    /// Configuration of a VRGUILayer
    struct LayerConfig
    {
        int priority; //!< Higher priority shows over lower priority windows by moving higher priority layers slightly closer to the player.
        bool sideBySide; //!< All layers with this config will show up side by side in a partial circle around the player, and will all be resized to a predefined size.
        float opacity; //!< Background color of layer
        Stereo::Position offset; //!< Offset from tracked node in meters
        osg::Vec2 center; //!< Model space centerpoint of menu geometry. All menu geometries have model space lengths of 1 in each dimension. Use this to affect how geometries grow with changing size.
        osg::Vec2 extent; //!< Spatial extent of the layer in meters when using Fixed sizing mode
        int spatialResolution; //!< Pixels when using the Auto sizing mode. \note Pixels per meter of the GUI viewport, not the RTT texture.
        osg::Vec2i pixelResolution; //!< Pixel resolution of the RTT texture
        osg::Vec2 myGUIViewSize; //!< Resizable elements are resized to this (fraction of full view)
        SizingMode sizingMode; //!< How to size the layer
        std::string trackingPath; //!< The path that will be used to read tracking data
        std::string extraLayers; //!< Additional layers to draw (list separated by any non-alphabetic)
        Intersectable intersectable;

        bool operator<(const LayerConfig& rhs) const { return priority < rhs.priority; }
    };

    //! TODO: Update description
    //! Old description: Extends the tracking source with /ui/input/stationary/pose
    class VRGUITracking : public VR::TrackingSource
    {
    public:
        VRGUITracking();
        ~VRGUITracking();

        std::vector<VR::VRPath> listSupportedPaths() const override;
        void updateTracking(VR::DisplayTime predictedDisplayTime) override;
        void resetStationaryPose();

    protected:
        virtual VR::TrackingPose locate(VR::VRPath path, VR::DisplayTime predictedDisplayTime) override;

    private:
        VR::TrackingPose mStationaryPose = VR::TrackingPose();
        VR::TrackingPose mHUDTopLeftPose = VR::TrackingPose();
        VR::TrackingPose mHUDTopRightPose = VR::TrackingPose();
        VR::TrackingPose mHUDBottomLeftPose = VR::TrackingPose();
        VR::TrackingPose mHUDBottomRightPose = VR::TrackingPose();
        VR::TrackingPose mHUDMessagePose = VR::TrackingPose();
        VR::TrackingPose mHUDKeyboardPose = VR::TrackingPose();
        VR::TrackingPose mWristInnerLeftPose = VR::TrackingPose();
        VR::TrackingPose mWristInnerRightPose = VR::TrackingPose();
        VR::TrackingPose mWristTopLeftPose = VR::TrackingPose();
        VR::TrackingPose mWristTopRightPose = VR::TrackingPose();

        bool mShouldUpdateStationaryPose = true;
        bool mHasInitialPose = false;
        bool mTimedPoseRefresh = false;
        std::chrono::steady_clock::time_point mTimedPoseRefreshTime;

        VR::DisplayTime mLastTime = 0;
    };

    /// \brief A single VR GUI Quad.
    ///
    /// In VR menus are shown as quads within the game world.
    /// The behaviour of that quad is defined by the MWVR::LayerConfig struct
    /// Each instance of VRGUILayer is used to show one MYGUI layer.
    class VRGUILayer : public VR::TrackingListener, public osg::Referenced
    {
    public:
        VRGUILayer(
            osg::ref_ptr<osg::Group> geometryRoot,
            osg::ref_ptr<osg::Group> cameraRoot,
            std::string layerName,
            LayerConfig config,
            VRGUIManager* parent);
        ~VRGUILayer();

        void update(osg::NodeVisitor* nv);

    public:
        osg::ref_ptr<osg::Geometry> createLayerGeometry(osg::ref_ptr<osg::StateSet> stateset);
        void setAngle(float angle);
        void updatePose();
        void updateRect();
        void insertWidget(MWGui::Layout* widget);
        void removeWidget(MWGui::Layout* widget);
        int  widgetCount() { return mWidgets.size(); }
        bool operator<(const VRGUILayer& rhs) const { return mConfig.priority < rhs.mConfig.priority; }
        void cull(osgUtil::CullVisitor* cv);
        void setVisible(bool visible);
        void removeFromSceneGraph();
        void addToSceneGraph();

        /// Update layer quads based on current tracking information
        void onTrackingUpdated(VR::TrackingManager& manager, VR::DisplayTime predictedDisplayTime) override;

    public:
        VR::VRPath mTrackingPath = 0;
        Stereo::Pose mTrackingPose = Stereo::Pose();

        Stereo::Pose mTrackedPose{};
        LayerConfig mConfig;
        std::string mLayerName;
        std::vector<MWGui::Layout*> mWidgets;
        osg::ref_ptr<osg::Group> mGeometryRoot;
        std::array<osg::ref_ptr<osg::Geometry>, 2> mGeometries;
        osg::ref_ptr<osg::PositionAttitudeTransform> mTransform{ new osg::PositionAttitudeTransform };
        osg::ref_ptr<osg::Group> mCameraRoot;
        osg::ref_ptr<osg::Node> mGUIRTT;
        osg::ref_ptr<osg::Camera> mMyGUICamera{ nullptr };
        osg::ref_ptr<osg::StateSet> mStateset;
        MyGUI::IntRect mRect{};
        MyGUI::FloatRect mRealRect{};
        osg::Quat mRotation{ 0,0,0,1 };
        bool mVisible = false;

        std::shared_ptr<VR::QuadLayer> mVrLayer;

    };

    /// \brief Manager of VRGUILayer objects.
    ///
    /// Constructs and destructs VRGUILayer objects in response to MWGui::Layout::setVisible calls.
    /// Layers can also be made visible directly by calling insertLayer() directly, e.g. to show
    /// the video player.
    class VRGUIManager
    {
    public:
        static VRGUIManager& instance();

        VRGUIManager(
            Resource::ResourceSystem* resourceSystem,
            osg::Group* rootNode);

        ~VRGUIManager(void);

        void configure();

        void clear();

        /// Set visibility of the layer this layout is on
        void setVisible(MWGui::Layout*, bool visible);
        
        /// Show the given layer
        void showLayer(const std::string& name);

        /// Hide the given layer
        void hideLayer(const std::string& name);

        /// Check current pointer target and update focus layer
        void updateFocus(osg::Node* focusNode, osg::Vec3f hitPoint);

        /// True if user is currently pointing at something
        bool hasFocus() const;

        /// Update traversal
        void update(osg::NodeVisitor* nv);

        /// Update traversal
        void updateTracking();

        /// Gui cursor coordinates to use to simulate a mouse press/move if the player is currently pointing at a vr gui layer
        osg::Vec2i guiCursor() { return mGuiCursor; };

        /// Inject mouse click if applicable
        bool injectMouseClick(bool onPress);

        /// Update settings where applicable
        void processChangedSettings(const std::set< std::pair<std::string, std::string> >& changed);

        static void registerMyGUIFactories();

        static void setPick(MWGui::Layout* widget, bool pick);

    private:
        void insertLayer(const std::string& name);
        // Not used: void removeLayer(const std::string& name);
        void computeGuiCursor(osg::Vec3 hitPoint);
        void updateSideBySideLayers();
        void insertWidget(MWGui::Layout* widget);
        void removeWidget(MWGui::Layout* widget);
        void setFocusLayer(osg::ref_ptr<VRGUILayer> layer);
        void setFocusWidget(MyGUI::Widget* widget);
        void configUpdated(const std::string& layer);

        osg::ref_ptr<osgViewer::Viewer> mOsgViewer;
        Resource::ResourceSystem* mResourceSystem;

        osg::ref_ptr<osg::Group> mRootNode = nullptr;
        osg::ref_ptr<osg::Group> mGeometries = new osg::Group;
        osg::ref_ptr<osg::Group> mGUICameras = new osg::Group;

        std::unique_ptr<VRGUITracking> mUiTracking = nullptr;

        std::map<std::string, osg::ref_ptr<VRGUILayer>> mLayers{};
        std::vector<osg::ref_ptr<VRGUILayer> > mSideBySideLayers{};

        osg::Vec2i  mGuiCursor = osg::Vec2i();
        osg::ref_ptr<VRGUILayer> mFocusLayer = nullptr;
        MyGUI::Widget* mFocusWidget = nullptr;
        std::map<std::string, LayerConfig> mLayerConfigs{};
    };
}

#endif
