#ifndef VR_H
#define VR_H

namespace VR
{
    bool getVR();
    bool getKBMouseModeActive();
    bool getLeftControllerActive();
    bool getRightControllerActive();
    bool getSteamVR();

    void setVR(bool VR);
    void setLeftControllerActive(bool active);
    void setRightControllerActive(bool active);
    void setSteamVR(bool steamVR);
}

#endif
