#ifndef VR_FRAME_H
#define VR_FRAME_H

#include <cstdint>

namespace VR
{
    //class Frame;

    using Frame = uint64_t;

    class Session
    {
    public:
        Session();
        ~Session();

        Frame newFrame();
        void frameBeginUpdate(Frame& frame);
        void frameBeginRender(Frame& frame);
        void frameEnd(Frame& frame);

    protected:


    private:
        Frame mUpdateFrame = 0;
        Frame mRenderFrame = 0;
    };
}

#endif
