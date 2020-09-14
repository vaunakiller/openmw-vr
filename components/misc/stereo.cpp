#include "stereo.hpp"

#include <osg/io_utils>
#include <osg/ViewportIndexed>

#include <osgUtil/CullVisitor>

#include <osgViewer/Viewer>

#include <iostream>

#include <components/debug/debuglog.hpp>
#include <components/sceneutil/statesetupdater.hpp>
#include <components/sceneutil/visitor.hpp>

namespace Misc
{
    Pose Pose::operator+(const Pose& rhs)
    {
        Pose pose = *this;
        pose.position += this->orientation * rhs.position;
        pose.orientation = rhs.orientation * this->orientation;
        return pose;
    }

    const Pose& Pose::operator+=(const Pose& rhs)
    {
        *this = *this + rhs;
        return *this;
    }

    Pose Pose::operator*(float scalar)
    {
        Pose pose = *this;
        pose.position *= scalar;
        return pose;
    }

    const Pose& Pose::operator*=(float scalar)
    {
        *this = *this * scalar;
        return *this;
    }

    Pose Pose::operator/(float scalar)
    {
        Pose pose = *this;
        pose.position /= scalar;
        return pose;
    }
    const Pose& Pose::operator/=(float scalar)
    {
        *this = *this / scalar;
        return *this;
    }

    bool Pose::operator==(const Pose& rhs) const
    {
        return position == rhs.position && orientation == rhs.orientation;
    }

    osg::Matrix Pose::viewMatrix(bool useGLConventions)
    {
        if (useGLConventions)
        {
            // When applied as an offset to an existing view matrix,
            // that view matrix will already convert points to a camera space
            // with opengl conventions. So we need to convert offsets to opengl 
            // conventions.
            float y = position.y();
            float z = position.z();
            position.y() = z;
            position.z() = -y;

            y = orientation.y();
            z = orientation.z();
            orientation.y() = z;
            orientation.z() = -y;

            osg::Matrix viewMatrix;
            viewMatrix.setTrans(-position);
            viewMatrix.postMultRotate(orientation.conj());
            return viewMatrix;
        }
        else
        {
            osg::Vec3d forward = orientation * osg::Vec3d(0, 1, 0);
            osg::Vec3d up = orientation * osg::Vec3d(0, 0, 1);
            osg::Matrix viewMatrix;
            viewMatrix.makeLookAt(position, position + forward, up);

            return viewMatrix;
        }
    }

    bool FieldOfView::operator==(const FieldOfView& rhs) const
    {
        return angleDown == rhs.angleDown
            && angleUp == rhs.angleUp
            && angleLeft == rhs.angleLeft
            && angleRight == rhs.angleRight;
    }

    // near and far named with an underscore because of windows' headers galaxy brain defines.
    osg::Matrix FieldOfView::perspectiveMatrix(float near_, float far_)
    {
        const float tanLeft = tanf(angleLeft);
        const float tanRight = tanf(angleRight);
        const float tanDown = tanf(angleDown);
        const float tanUp = tanf(angleUp);

        const float tanWidth = tanRight - tanLeft;
        const float tanHeight = tanUp - tanDown;

        const float offset = near_;

        float matrix[16] = {};

        matrix[0] = 2 / tanWidth;
        matrix[4] = 0;
        matrix[8] = (tanRight + tanLeft) / tanWidth;
        matrix[12] = 0;

        matrix[1] = 0;
        matrix[5] = 2 / tanHeight;
        matrix[9] = (tanUp + tanDown) / tanHeight;
        matrix[13] = 0;

        if (far_ <= near_) {
            matrix[2] = 0;
            matrix[6] = 0;
            matrix[10] = -1;
            matrix[14] = -(near_ + offset);
        }
        else {
            matrix[2] = 0;
            matrix[6] = 0;
            matrix[10] = -(far_ + offset) / (far_ - near_);
            matrix[14] = -(far_ * (near_ + offset)) / (far_ - near_);
        }

        matrix[3] = 0;
        matrix[7] = 0;
        matrix[11] = -1;
        matrix[15] = 0;

        return osg::Matrix(matrix);
    }

    bool View::operator==(const View& rhs) const
    {
        return pose == rhs.pose && fov == rhs.fov;
    }

    std::ostream& operator <<(
        std::ostream& os,
        const Pose& pose)
    {
        os << "position=" << pose.position << ", orientation=" << pose.orientation;
        return os;
    }

    std::ostream& operator <<(
        std::ostream& os,
        const FieldOfView& fov)
    {
        os << "left=" << fov.angleLeft << ", right=" << fov.angleRight << ", down=" << fov.angleDown << ", up=" << fov.angleUp;
        return os;
    }

    std::ostream& operator <<(
        std::ostream& os,
        const View& view)
    {
        os << "pose=< " << view.pose << " >, fov=< " << view.fov << " >";
        return os;
    }

    // Update stereo view/projection during update
    class StereoUpdateCallback : public osg::Callback
    {
    public:
        StereoUpdateCallback(StereoView* stereoView) : stereoView(stereoView) {}

        bool run(osg::Object* object, osg::Object* data) override
        {
            auto b = traverse(object, data);
            stereoView->update();
            return b;
        }

        StereoView* stereoView;
    };

    // Update states during cull
    class StereoStatesetUpdateCallback : public SceneUtil::StateSetUpdater
    {
    public:
        StereoStatesetUpdateCallback(StereoView* view)
            : stereoView(view)
        {
        }

    protected:
        virtual void setDefaults(osg::StateSet* stateset)
        {
            auto stereoViewMatrixUniform = new osg::Uniform(osg::Uniform::FLOAT_MAT4, "stereoViewMatrices", 2);
            stateset->addUniform(stereoViewMatrixUniform, osg::StateAttribute::OVERRIDE);
            auto stereoViewProjectionsUniform = new osg::Uniform(osg::Uniform::FLOAT_MAT4, "stereoViewProjections", 2);
            stateset->addUniform(stereoViewProjectionsUniform);
            auto geometryPassthroughUniform = new osg::Uniform("geometryPassthrough", false);
            stateset->addUniform(geometryPassthroughUniform);
        }

        virtual void apply(osg::StateSet* stateset, osg::NodeVisitor* /*nv*/)
        {
            stereoView->updateStateset(stateset);
        }

    private:
        StereoView* stereoView;
    };

    StereoView::StereoView(osgViewer::Viewer* viewer, osg::Node::NodeMask geometryShaderMask, osg::Node::NodeMask bruteForceMask)
        : osg::Group()
        , mViewer(viewer)
        , mMainCamera(mViewer->getCamera())
        , mRoot(viewer->getSceneData()->asGroup())
        , mGeometryShaderMask(geometryShaderMask)
        , mBruteForceMask(bruteForceMask)
    {
        SceneUtil::FindByNameVisitor findScene("Scene Root");
        mRoot->accept(findScene);
        mScene = findScene.mFoundNode;
        if (!mScene)
            throw std::logic_error("Couldn't find scene root");

        setName("Sky Root");
        mRoot->setDataVariance(osg::Object::STATIC);
        setDataVariance(osg::Object::STATIC);
        mLeftCamera->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
        mLeftCamera->setProjectionResizePolicy(osg::Camera::FIXED);
        mLeftCamera->setProjectionMatrix(osg::Matrix::identity());
        mLeftCamera->setViewMatrix(osg::Matrix::identity());
        mLeftCamera->setRenderOrder(osg::Camera::NESTED_RENDER);
        mLeftCamera->setClearMask(GL_NONE);
        mLeftCamera->setCullMask(bruteForceMask);
        mLeftCamera->setName("Stereo Left");
        mLeftCamera->setDataVariance(osg::Object::STATIC);
        mRightCamera->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
        mRightCamera->setProjectionResizePolicy(osg::Camera::FIXED);
        mRightCamera->setProjectionMatrix(osg::Matrix::identity());
        mRightCamera->setViewMatrix(osg::Matrix::identity());
        mRightCamera->setRenderOrder(osg::Camera::NESTED_RENDER);
        mRightCamera->setClearMask(GL_NONE);
        mRightCamera->setCullMask(bruteForceMask);
        mRightCamera->setName("Stereo Right");
        mRightCamera->setDataVariance(osg::Object::STATIC);

        mMainCamera->setCullMask(geometryShaderMask);

        // Inject self as the root of the scene graph, and split into geometry-shader stereo and brute force stereo.
        addChild(mStereoGeometryShaderRoot);
        mStereoGeometryShaderRoot->addChild(mRoot);
        addChild(mStereoBruteForceRoot);
        mStereoBruteForceRoot->addChild(mLeftCamera);
        mLeftCamera->addChild(mScene); // Use scene directly to avoid redundant shadow computation.
        mStereoBruteForceRoot->addChild(mRightCamera);
        mRightCamera->addChild(mScene);
        viewer->setSceneData(this);

        // Do a blank double buffering of camera statesets on update. Actual stateset updates are performed in StereoView::Update()
        mLeftCamera->setUpdateCallback(new SceneUtil::StateSetUpdater());
        mRightCamera->setUpdateCallback(new SceneUtil::StateSetUpdater());

        // Update stereo statesets/matrices, but after the main camera updates.
        auto mainCameraCB = mMainCamera->getUpdateCallback();
        mMainCamera->removeUpdateCallback(mainCameraCB);
        mMainCamera->addUpdateCallback(new StereoUpdateCallback(this));
        mMainCamera->addUpdateCallback(mainCameraCB);

        addCullCallback(new StereoStatesetUpdateCallback(this));
    }

    void StereoView::update()
    {
        auto viewMatrix = mViewer->getCamera()->getViewMatrix();
        auto projectionMatrix = mViewer->getCamera()->getProjectionMatrix();

        View left{};
        View right{};
        double near = 1.f;
        double far = 10000.f;
        if (!cb)
        {
            Log(Debug::Error) << "No update view callback. Stereo rendering will not work.";
        }
        cb->updateView(left, right, near, far);

        osg::Vec3d leftEye = left.pose.position;
        osg::Vec3d rightEye = right.pose.position;

        osg::Matrix leftViewOffset = left.pose.viewMatrix(true);
        osg::Matrix rightViewOffset = right.pose.viewMatrix(true);

        osg::Matrix leftViewMatrix = viewMatrix * leftViewOffset;
        osg::Matrix rightViewMatrix = viewMatrix * rightViewOffset;

        osg::Matrix leftProjectionMatrix = left.fov.perspectiveMatrix(near, far);
        osg::Matrix rightProjectionMatrix = right.fov.perspectiveMatrix(near, far);

        mRightCamera->setViewMatrix(rightViewMatrix);
        mLeftCamera->setViewMatrix(leftViewMatrix);
        mRightCamera->setProjectionMatrix(rightProjectionMatrix);
        mLeftCamera->setProjectionMatrix(leftProjectionMatrix);

        // The persepctive frustum will be computed from a position P slightly behind the eyes L and R
        // where it creates the minimum frustum encompassing both eyes' frustums.
        // NOTE: I make an assumption that the eyes lie in a horizontal plane relative to the base view,
        // and lie mirrored around the Y axis (straight ahead).
        // Re-think this if that turns out to be a bad assumption
        View frustumView;

        // Compute Frustum angles. A simple min/max.
        /* Example values for reference:
            Left:
            angleLeft   -0.767549932    float
            angleRight   0.620896876    float
            angleDown   -0.837898076    float
            angleUp	     0.726982594    float

            Right:
            angleLeft   -0.620896876    float
            angleRight   0.767549932    float
            angleDown   -0.837898076    float
            angleUp	     0.726982594    float
        */
        frustumView.fov.angleLeft = std::min(left.fov.angleLeft, right.fov.angleLeft);
        frustumView.fov.angleRight = std::max(left.fov.angleRight, right.fov.angleRight);
        frustumView.fov.angleDown = std::min(left.fov.angleDown, right.fov.angleDown);
        frustumView.fov.angleUp = std::max(left.fov.angleUp, right.fov.angleUp);

        // Check that the case works for this approach
        auto maxAngle = std::max(frustumView.fov.angleRight - frustumView.fov.angleLeft, frustumView.fov.angleUp - frustumView.fov.angleDown);
        if (maxAngle > osg::PI)
        {
            Log(Debug::Error) << "Total FOV exceeds 180 degrees. Case cannot be culled in single-pass VR. Disabling culling to cope. Consider switching to dual-pass VR.";
            mMainCamera->setCullingActive(false);
            return;
            // TODO: An explicit frustum projection could cope, so implement that later. Guarantee you there will be VR headsets with total fov > 180 in the future. Maybe already.
        }

        // Use the law of sines on the triangle spanning PLR to determine P
        double angleLeft = std::abs(frustumView.fov.angleLeft);
        double angleRight = std::abs(frustumView.fov.angleRight);
        double lengthRL = (rightEye - leftEye).length();
        double ratioRL = lengthRL / std::sin(osg::PI - angleLeft - angleRight);
        double lengthLP = ratioRL * std::sin(angleRight);

        osg::Vec3d directionLP = osg::Vec3(std::cos(-angleLeft), std::sin(-angleLeft), 0);
        osg::Vec3d LP = directionLP * lengthLP;
        frustumView.pose.position = leftEye + LP;
        //frustumView.pose.position.x() += 1000;

        // Base view position is 0.0, by definition.
        // The length of the vector P is therefore the required offset to near/far.
        auto nearFarOffset = frustumView.pose.position.length();

        // Generate the frustum matrices
        auto frustumViewMatrix = viewMatrix * frustumView.pose.viewMatrix(true);
        auto frustumProjectionMatrix = frustumView.fov.perspectiveMatrix(near + nearFarOffset, far + nearFarOffset);

        // Update camera with frustum matrices
        mMainCamera->setViewMatrix(frustumViewMatrix);
        mMainCamera->setProjectionMatrix(frustumProjectionMatrix);

        auto width = mMainCamera->getViewport()->width();
        auto height = mMainCamera->getViewport()->height();
        mLeftCamera->getOrCreateStateSet()->setAttribute(new osg::ViewportIndexed(0, 0, 0, width / 2, height), osg::StateAttribute::OVERRIDE);
        mRightCamera->getOrCreateStateSet()->setAttribute(new osg::ViewportIndexed(0, width / 2, 0, width / 2, height), osg::StateAttribute::OVERRIDE);
    }

    void StereoView::updateStateset(osg::StateSet * stateset)
    {
        // Manage viewports in update to automatically catch window/resolution changes.
        auto width = mMainCamera->getViewport()->width();
        auto height = mMainCamera->getViewport()->height();
        stateset->setAttribute(new osg::ViewportIndexed(0, 0, 0, width / 2, height));
        stateset->setAttribute(new osg::ViewportIndexed(1, width / 2, 0, width / 2, height));

        // Update stereo uniforms
        auto frustumViewMatrixInverse = osg::Matrix::inverse(mMainCamera->getViewMatrix());
        //auto frustumViewProjectionMatrixInverse = osg::Matrix::inverse(mMainCamera->getProjectionMatrix()) * osg::Matrix::inverse(mMainCamera->getViewMatrix());
        auto* stereoViewMatrixUniform = stateset->getUniform("stereoViewMatrices");
        auto* stereoViewProjectionsUniform = stateset->getUniform("stereoViewProjections");

        stereoViewMatrixUniform->setElement(0, frustumViewMatrixInverse * mLeftCamera->getViewMatrix());
        stereoViewMatrixUniform->setElement(1, frustumViewMatrixInverse * mRightCamera->getViewMatrix());
        stereoViewProjectionsUniform->setElement(0, frustumViewMatrixInverse * mLeftCamera->getViewMatrix() * mLeftCamera->getProjectionMatrix());
        stereoViewProjectionsUniform->setElement(1, frustumViewMatrixInverse * mRightCamera->getViewMatrix() * mRightCamera->getProjectionMatrix());
    }

    void StereoView::setUpdateViewCallback(std::shared_ptr<UpdateViewCallback> cb)
    {
        this->cb = cb;
    }

    void StereoView::useSlaveCameraAtIndex(int index)
    {
        if (mViewer->getNumSlaves() <= index)
        {
            Log(Debug::Error) << "Requested slave at index " << index << " but no such slave exists";
            return;
        }

        mMainCamera = mViewer->getSlave(index)._camera;
        mMainCamera->setCullMask(mGeometryShaderMask);
    }

    void disableStereoForCamera(osg::Camera* camera)
    {
        auto* viewport = camera->getViewport();
        camera->getOrCreateStateSet()->setAttribute(new osg::ViewportIndexed(0, viewport->x(), viewport->y(), viewport->width(), viewport->height()), osg::StateAttribute::OVERRIDE);
        camera->getOrCreateStateSet()->addUniform(new osg::Uniform("geometryPassthrough", true), osg::StateAttribute::OVERRIDE);
    }

    void enableStereoForCamera(osg::Camera* camera, bool horizontalSplit)
    {
        auto* viewport = camera->getViewport();
        auto x1 = viewport->x();
        auto y1 = viewport->y();
        auto width = viewport->width();
        auto height = viewport->height();

        auto x2 = x1;
        auto y2 = y1;

        if (horizontalSplit)
        {
            width /= 2;
            x2 += width;
        }
        else
        {
            height /= 2;
            y2 += height;
        }

        camera->getOrCreateStateSet()->setAttribute(new osg::ViewportIndexed(0, x1, y1, width, height));
        camera->getOrCreateStateSet()->setAttribute(new osg::ViewportIndexed(1, x2, y2, width, height));
        camera->getOrCreateStateSet()->addUniform(new osg::Uniform("geometryPassthrough", false));
    }
    void StereoView::DefaultUpdateViewCallback::updateView(View& left, View& right, double& near, double& far)
    {
        left.pose.position = osg::Vec3(-2.2, 0, 0);
        right.pose.position = osg::Vec3(2.2, 0, 0);
        left.fov = { -0.767549932, 0.620896876, -0.837898076, 0.726982594 };
        right.fov = { -0.620896876, 0.767549932, -0.837898076, 0.726982594 };
        near = 1;
        far = 10000;
    }
}
