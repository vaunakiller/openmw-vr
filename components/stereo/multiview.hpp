#ifndef STEREO_MULTIVIEW_H
#define STEREO_MULTIVIEW_H

#include <osg/ref_ptr>

namespace osg
{
    class Texture2D;
    class Texture2DArray;
}

namespace Stereo
{
    //! Check if MultiView is supported. If the first call to this method does not use a valid contextID, the result is undefined.
    //! Subsequent calls should pass 0.
    bool                  getMultiview(unsigned int contextID = 0);

    //! Creates a Texture2D as a texture view into a Texture2DArray
    osg::ref_ptr<osg::Texture2D> createTextureView_Texture2DFromTexture2DArray(osg::Texture2DArray* textureArray, int layer);
}

#endif
