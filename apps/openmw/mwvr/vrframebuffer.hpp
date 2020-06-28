#ifndef MWVR_OPENRXTEXTURE_H
#define MWVR_OPENRXTEXTURE_H

#include <memory>
#include <osg/Group>
#include <osg/Camera>
#include <osgViewer/Viewer>

#include "openxrmanager.hpp"

namespace MWVR
{
    /// \brief Manages an opengl framebuffer
    ///
    /// Intended for managing the vr swapchain, but is also use to manage the mirror texture as a convenience.
    class VRFramebuffer : public osg::Referenced
    {
    public:
        VRFramebuffer(osg::ref_ptr<osg::State> state, std::size_t width, std::size_t height, uint32_t msaaSamples, uint32_t colorBuffer = 0, uint32_t depthBuffer = 0);
        ~VRFramebuffer();

        void destroy(osg::State* state);

        auto width() const { return mWidth; }
        auto height() const { return mHeight; }
        auto msaaSamples() const { return mSamples; }

        void bindFramebuffer(osg::GraphicsContext* gc, uint32_t target);

        uint32_t fbo(void) const { return mFBO; }
        uint32_t colorBuffer(void) const { return mColorBuffer; }

        //! Blit to region in currently bound draw fbo
        void blit(osg::GraphicsContext* gc, int x, int y, int w, int h);

    private:
        // Set aside a weak pointer to the constructor state to use when freeing FBOs, if no state is given to destroy()
        osg::observer_ptr<osg::State> mState;

        // Metadata
        std::size_t mWidth = 0;
        std::size_t mHeight = 0;

        // Render Target
        uint32_t mFBO = 0;
        uint32_t mBlitFBO = 0;
        uint32_t mDepthBuffer = 0;
        bool     mOwnDepthBuffer = false;
        uint32_t mColorBuffer = 0;
        bool     mOwnColorBuffer = false;
        uint32_t mSamples = 0;
        uint32_t mTextureTarget = 0;
    };
}

#endif
