#ifndef VR_FRAME_H
#define VR_FRAME_H

#include <cstdint>
#include <vector>
#include <memory>

namespace VR
{
    struct Layer;

    struct Frame
    {
    public:
        Frame();
        ~Frame();

        bool     shouldSyncFrameLoop = false;
        bool     shouldRender = false;
        uint64_t runtimePredictedDisplayTime = 0;
        uint64_t runtimePredictedDisplayPeriod = 0;
        uint64_t frameNumber = 0;

        std::vector<std::shared_ptr<Layer>> layers;
    };
}

#endif
