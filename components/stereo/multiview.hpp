#ifndef STEREO_MULTIVIEW_H
#define STEREO_MULTIVIEW_H

namespace Stereo
{
    //! Check if MultiView is supported. If the first call to this method does not use a valid contextID, the result is undefined.
    //! Subsequent calls should pass 0.
    bool                  getMultiview(unsigned int contextID = 0);
}

#endif
