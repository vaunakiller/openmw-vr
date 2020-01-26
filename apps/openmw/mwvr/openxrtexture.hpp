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
        OpenXRTextureBuffer(osg::ref_ptr<osg::State> state, std::size_t width, std::size_t height, uint32_t msaaSamples);
        ~OpenXRTextureBuffer();

        void destroy(osg::State* state);

        auto width() const { return mWidth; }
        auto height() const { return mHeight; }
        auto msaaSamples() const { return mSamples; }

        void beginFrame(osg::GraphicsContext* gc);
        void endFrame(osg::GraphicsContext* gc, uint32_t blitTarget);

        void writeToJpg(osg::State& state, std::string filename);

        uint32_t fbo(void) const { return mFBO; }

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
        uint32_t mColorBuffer = 0;
        uint32_t mSamples = 0;
        uint32_t mTextureTarget = 0;
    };
}

#endif
