#ifndef OPENXR_VIEW_HPP
#define OPENXR_VIEW_HPP

#include <cassert>
#include "openxrmanager.hpp"
#include "openxrswapchain.hpp"

struct XrSwapchainSubImage;

namespace MWVR
{

    class OpenXRView : public osg::Referenced
    {
    public:

        class InitialDrawCallback : public osg::Camera::DrawCallback
        {
        public:
            virtual void operator()(osg::RenderInfo& renderInfo) const;
        };
        class FinalDrawCallback : public osg::Camera::DrawCallback
        {
        public:
            virtual void operator()(osg::RenderInfo& renderInfo) const;
        };

    protected:
        OpenXRView(osg::ref_ptr<OpenXRManager> XR, std::string name, OpenXRSwapchain::Config config, osg::ref_ptr<osg::State> state);
        virtual ~OpenXRView();

    public:
        //! Prepare for render (set FBO)
        virtual void prerenderCallback(osg::RenderInfo& renderInfo);
        //! Finalize render
        virtual void postrenderCallback(osg::RenderInfo& renderInfo);
        //! Create camera for this view
        osg::Camera* createCamera(int order, const osg::Vec4& clearColor, osg::GraphicsContext* gc);
        //! Get the view surface
        OpenXRSwapchain& swapchain(void) { return *mSwapchain; }

        void swapBuffers(osg::GraphicsContext* gc);

        void  setPredictedPose(const Pose& pose);
        Pose& predictedPose() { return mPredictedPose; };

    public:
        osg::ref_ptr<OpenXRManager> mXR;
        OpenXRSwapchain::Config mSwapchainConfig;
        std::unique_ptr<OpenXRSwapchain> mSwapchain;
        std::string mName{};
        bool mRendering{ false };
        Timer mTimer;

        Pose mPredictedPose{ {}, {0,0,0,1}, {} };
    };
}

#endif
