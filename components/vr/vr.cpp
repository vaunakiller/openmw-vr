#include "vr.hpp"

namespace VR
{
    namespace
    {
        bool sVRMode = false;
        bool sLeftControllerActive = false;
        bool sRightControllerActive = false;
    }

    bool getVR()
    {
        return sVRMode;
    }

    bool getLeftControllerActive()
    {
        return sLeftControllerActive;
    }

    bool getRightControllerActive()
    {
        return sRightControllerActive;
    }

    void setVR(bool VR)
    {
        sVRMode = VR;
    }
    void setLeftControllerActive(bool active)
    {
        sLeftControllerActive = active;
    }
    void setRightControllerActive(bool active)
    {
        sRightControllerActive = active;
    }
}
