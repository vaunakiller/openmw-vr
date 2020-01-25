#ifndef OPENXR_VIEW_HPP
#define OPENXR_VIEW_HPP

#include "openxrmanager.hpp"
#include "openxrswapchain.hpp"

struct XrSwapchainSubImage;

namespace MWVR
{
    class OpenXRView : public osg::Referenced
    {
    public:
        class PredrawCallback : public osg::Camera::DrawCallback
        {
        public:
            PredrawCallback(osg::Camera* camera, OpenXRView* view)
                : mCamera(camera)
                , mView(view)
            {}

            void operator()(osg::RenderInfo& info) const override;

        private:

            osg::observer_ptr<osg::Camera> mCamera;
            OpenXRView* mView;
        };
        class PostdrawCallback : public osg::Camera::DrawCallback
        {
        public:
            PostdrawCallback(osg::Camera* camera, OpenXRView* view)
                : mCamera(camera)
                , mView(view)
            {}

            void operator()(osg::RenderInfo& info) const override;

        private:

            osg::observer_ptr<osg::Camera> mCamera;
            OpenXRView* mView;
        };

        class InitialDrawCallback : public osg::Camera::DrawCallback
        {
        public:
            virtual void operator()(osg::RenderInfo& renderInfo) const;
        };

    protected:
        OpenXRView(osg::ref_ptr<OpenXRManager> XR);
        virtual ~OpenXRView();
        void setWidth(int width);
        void setHeight(int height);
        void setSamples(int samples);

    public:
        //! Prepare for render (set FBO)
        virtual void prerenderCallback(osg::RenderInfo& renderInfo);
        //! Finalize render
        virtual void postrenderCallback(osg::RenderInfo& renderInfo);
        //! Create camera for this view
        osg::Camera* createCamera(int order, const osg::Vec4& clearColor, osg::GraphicsContext* gc);
        //! Get the view surface
        OpenXRSwapchain& swapchain(void) { return *mSwapchain; }
        //! Create the view surface
        bool realize(osg::ref_ptr<osg::State> state);
        //! Current frame being rendered
        long long frameIndex() { return mFrameIndex; };

    protected:
        osg::ref_ptr<OpenXRManager> mXR;
        std::unique_ptr<OpenXRSwapchain> mSwapchain;
        OpenXRSwapchain::Config mSwapchainConfig;
        long long mFrameIndex{ 0 };
    };
}

#endif
