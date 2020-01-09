#ifndef OPENXR_VIEW_HPP
#define OPENXR_VIEW_HPP

#include "openxrmanager.hpp"
#include "openxrtexture.hpp"

namespace MWVR
{


    class OpenXRViewImpl;

    class OpenXRView : public osg::Referenced
    {
    public:
        class PredrawCallback : public osg::Camera::DrawCallback
        {
        public:
            PredrawCallback(osg::Camera* camera, OpenXRViewImpl* view)
                : mCamera(camera)
                , mView(view)
            {}

            void operator()(osg::RenderInfo& info) const override;

        private:

            osg::observer_ptr<osg::Camera> mCamera;
            OpenXRViewImpl* mView;
        };
        class PostdrawCallback : public osg::Camera::DrawCallback
        {
        public:
            PostdrawCallback(osg::Camera* camera, OpenXRViewImpl* view)
                : mCamera(camera)
                , mView(view)
            {}

            void operator()(osg::RenderInfo& info) const override;

        private:

            osg::observer_ptr<osg::Camera> mCamera;
            OpenXRViewImpl* mView;
        };

        class InitialDrawCallback : public osg::Camera::DrawCallback
        {
        public:
            virtual void operator()(osg::RenderInfo& renderInfo) const;
        };

    public:
        OpenXRView(osg::ref_ptr<OpenXRManager> XR, osg::ref_ptr<osg::State> state, float metersPerUnit, unsigned int viewIndex);
        ~OpenXRView();

        //! Get the next color buffer.
        //! \return The GL texture ID of the now current swapchain image
        osg::ref_ptr<OpenXRTextureBuffer> prepareNextSwapchainImage();
        //! Release current color buffer. Do not forget to call this after rendering to the color buffer.
        void   releaseSwapchainImage();
        //! Prepare for render
        void prerenderCallback(osg::RenderInfo& renderInfo);
        //! Finalize render
        void postrenderCallback(osg::RenderInfo& renderInfo);
        //! Create camera for this view
        osg::Camera* createCamera(int eye, const osg::Vec4& clearColor, osg::GraphicsContext* gc);
        //! Projection offset for this view
        osg::Matrix projectionMatrix();
        //! View offset for this view
        osg::Matrix viewMatrix();

        std::unique_ptr<OpenXRViewImpl> mPrivate;
    };
}

#endif
