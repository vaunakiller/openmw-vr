#include "vr.hpp"

namespace VR
{
    namespace
    {
        bool sVRMode = false;
    }

    bool getVR()
    {
        return sVRMode;
    }

    void setVR(bool VR)
    {
        sVRMode = VR;
    }
}
