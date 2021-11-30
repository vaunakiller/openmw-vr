#ifndef STEREO_TYPES_H
#define STEREO_TYPES_H

#include <osg/Matrix>
#include <osg/Vec3>


namespace Stereo
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
        float    angleLeft{ 0.f };
        float    angleRight{ 0.f };
        float    angleUp{ 0.f };
        float    angleDown{ 0.f };

        bool operator==(const FieldOfView& rhs) const;

        //! Generate a perspective matrix from this fov
        osg::Matrix perspectiveMatrix(float near, float far, bool reverseZ) const;
    };

    //! Represents an eye including both pose and fov.
    struct View
    {
        Pose pose;
        FieldOfView fov;
        bool operator==(const View& rhs) const;
    };
}

#endif
