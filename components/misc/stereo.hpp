#ifndef MISC_STEREO_H
#define MISC_STEREO_H

#include <osg/Matrix>
#include <osg/Vec3>
#include <osg/Camera>
#include <osg/StateSet>

#include <memory>

// Some cursed headers like to define these
#if defined(near) || defined(far)
#undef near
#undef far
#endif

namespace osgViewer
{
    class Viewer;
}

namespace Misc
{
    //! Represents the relative pose in space of some object
    struct Pose
    {
        //! Position in space
        osg::Vec3 position{ 0,0,0 };
        //! Orientation in space.
        osg::Quat orientation{ 0,0,0,1 };

        //! Add one pose to another
        Pose operator+(const Pose& rhs);
        const Pose& operator+=(const Pose& rhs);

        //! Scale a pose (does not affect orientation)
        Pose operator*(float scalar);
        const Pose& operator*=(float scalar);
        Pose operator/(float scalar);
        const Pose& operator/=(float scalar);

        bool operator==(const Pose& rhs) const;

        osg::Matrix viewMatrix(bool useGLConventions);
    };

    //! Fov that defines all 4 angles from center
    struct FieldOfView {
        float    angleLeft{ -osg::PI_2 };
        float    angleRight{ osg::PI_2 };
        float    angleDown{ -osg::PI_2 };
        float    angleUp{ osg::PI_2 };

        bool operator==(const FieldOfView& rhs) const;

        //! Generate a perspective matrix from this fov
        osg::Matrix perspectiveMatrix(float near, float far);
    };

    //! Represents an eye including both pose and fov.
    struct View
    {
        Pose pose;
        FieldOfView fov;
        bool operator==(const View& rhs) const;
    };

    //! Represent two eyes. The eyes are in relative terms, and are assumed to lie on the horizon plane.
    struct StereoView : public osg::Group
    {
        struct UpdateViewCallback
        {
            //! Called during the update traversal of every frame to source updated stereo values.
            virtual void updateView(View& left, View& right, double& near, double& far) = 0;
        };

        //! Default implementation of UpdateViewCallback that just provides some hardcoded values for debugging purposes
        struct DefaultUpdateViewCallback : public UpdateViewCallback
        {
            virtual void updateView(View& left, View& right, double& near, double& far);
        };

        enum class Technique
        {
            None = 0, //!< Stereo disabled (do nothing).
            BruteForce, //!< Two slave cameras culling and drawing everything.
            GeometryShader_IndexedViewports, //!< Frustum camera culls and draws stereo into indexed viewports using an automatically generated geometry shader.
        };

        //! Adds two cameras in stereo to the mainCamera.
        //! All nodes matching the mask are rendered in stereo using brute force via two camera transforms, the rest are rendered in stereo via a geometry shader.
        //! \param geometryShaderMask should mask in all nodes that use shaders.
        //! \param noShaderMask mask in all nodes that do not use shaders and must be rendered brute force.
        //! \note the masks apply only to the GeometryShader_IndexdViewports technique and can be 0 for the BruteForce technique.
        StereoView(osgViewer::Viewer* viewer, Technique technique, osg::Node::NodeMask geometryShaderMask, osg::Node::NodeMask noShaderMask);

        //! Updates uniforms with the view and projection matrices of each stereo view, and replaces the camera's view and projection matrix
        //! with a view and projection that closely envelopes the frustums of the two eyes.
        void update();
        void updateStateset(osg::StateSet* stateset);

        //! Callback that updates stereo configuration during the update pass
        void setUpdateViewCallback(std::shared_ptr<UpdateViewCallback> cb);

    private:
        void setupBruteForceTechnique();
        void setupGeometryShaderIndexedViewportTechnique();

        osg::ref_ptr<osgViewer::Viewer> mViewer;
        osg::ref_ptr<osg::Camera>       mMainCamera;
        osg::ref_ptr<osg::Group>        mRoot;
        osg::ref_ptr<osg::Group>        mScene;
        Technique                       mTechnique;

        // Keeps state relevant to doing stereo via the geometry shader
        osg::ref_ptr<osg::Group>    mStereoGeometryShaderRoot{ new osg::Group };
        osg::Node::NodeMask         mGeometryShaderMask;
        osg::Node::NodeMask         mNoShaderMask;

        // Keeps state and cameras relevant to doing stereo via brute force
        osg::ref_ptr<osg::Group>    mStereoBruteForceRoot{ new osg::Group };
        osg::ref_ptr<osg::Camera>   mLeftCamera{ new osg::Camera };
        osg::ref_ptr<osg::Camera>   mRightCamera{ new osg::Camera };

        // Camera viewports
        bool flipViewOrder{ true };

        // Updates stereo configuration during the update pass
        std::shared_ptr<UpdateViewCallback> cb{ new DefaultUpdateViewCallback };
    };

    //! Overrides all stereo-related states/uniforms to disable stereo for the scene rendered by camera
    void disableStereoForCamera(osg::Camera* camera);

    //! Overrides all stereo-related states/uniforms to enable stereo for the scene rendered by camera
    void enableStereoForCamera(osg::Camera* camera, bool horizontalSplit);

    //! Reads settings to determine stereo technique
    StereoView::Technique getStereoTechnique(void);
}

#endif
