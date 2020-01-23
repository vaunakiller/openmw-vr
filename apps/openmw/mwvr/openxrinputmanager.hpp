#ifndef OPENXR_INPUT_MANAGER_HPP
#define OPENXR_INPUT_MANAGER_HPP

#include "openxrmanager.hpp"
#include "../mwinput/inputmanagerimp.hpp"

#include <vector>
#include <array>
#include <iostream>

namespace MWVR
{
    //struct OpenXRPoseImpl;
    //// TODO: Make this an OSG node/group and attach posed elements to it ?
    //struct OpenXRPose
    //{
    //public:
    //    enum 
    //    {

    //    };

    //protected:
    //    OpenXRPose(osg::ref_ptr<OpenXRManager> XR, TrackedLimb limb, TrackingMode mode);
    //    ~OpenXRPose();

    //public:

    //    bool isActive();
    //    void update();

    //    TrackedLimb limb() const;
    //    TrackingMode trackingMode() const;
    //    

    //    OpenXRPoseImpl& impl() { return *mPrivate; }

    //private:
    //    std::unique_ptr<OpenXRPoseImpl> mPrivate;
    //};

    struct OpenXRInputManagerImpl;
    struct OpenXRInputManager
    {
        OpenXRInputManager(osg::ref_ptr<OpenXRManager> XR);
        ~OpenXRInputManager();

        void updateControls();

        OpenXRInputManagerImpl& impl() { return *mPrivate; }

        std::unique_ptr<OpenXRInputManagerImpl> mPrivate;
    };
}

#endif
