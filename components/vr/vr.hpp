#ifndef VR_H
#define VR_H

namespace VR
{
    bool getVR();
    bool getLeftControllerActive();
    bool getRightControllerActive();

    void setVR(bool VR);
    void setLeftControllerActive(bool active);
    void setRightControllerActive(bool active);
}

#endif
