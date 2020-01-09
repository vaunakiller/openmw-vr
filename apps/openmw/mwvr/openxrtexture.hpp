#ifndef MWVR_OPENRXTEXTURE_H
#define MWVR_OPENRXTEXTURE_H

#include <memory>
#include <osg/Group>
#include <osg/Camera>
#include <osgViewer/Viewer>

#include "openxrmanager.hpp"

namespace MWVR
{
    class OpenXRTextureBuffer : public osg::Referenced
    {
    public:
        OpenXRTextureBuffer(osg::ref_ptr<OpenXRManager> XR, osg::ref_ptr<osg::State> state, uint32_t XRColorBuffer, std::size_t width, std::size_t height, uint32_t msaaSamples);
        ~OpenXRTextureBuffer();

        void destroy(osg::State* state);

        auto width() const { return mWidth; }
        auto height() const { return mHeight; }
        auto msaaSamples() const { return mMSAASamples; }

        void beginFrame(osg::RenderInfo& renderInfo);
        void endFrame(osg::RenderInfo& renderInfo);

        void writeToJpg(osg::State& state, std::string filename);

    private:
        osg::observer_ptr<OpenXRManager> mXR;

        // Set aside a weak pointer to the constructor state to use when freeing FBOs, if no state is given to destroy()
        osg::observer_ptr<osg::State> mState;

        // Metadata
        std::size_t mWidth = 0;
        std::size_t mHeight = 0;

        // Swapchain buffer
        uint32_t mXRColorBuffer = 0;

        // FBO target for swapchain buffer
        uint32_t mDepthBuffer = 0;
        uint32_t mFBO = 0;

        // Render targets for MSAA
        uint32_t mMSAASamples = 0;
        uint32_t mMSAAFBO = 0;
        uint32_t mMSAAColorBuffer = 0;
        uint32_t mMSAADepthBuffer = 0;
    };
}

#endif
