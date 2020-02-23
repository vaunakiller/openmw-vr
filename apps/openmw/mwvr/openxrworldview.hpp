#ifndef OPENXRWORLDVIEW_HPP
#define OPENXRWORLDVIEW_HPP

#include "openxrview.hpp"
#include "openxrsession.hpp"

namespace MWVR
{
    class OpenXRWorldView : public OpenXRView
    {
    public:
        class UpdateSlaveCallback : public osg::View::Slave::UpdateSlaveCallback
        {
        public:
            UpdateSlaveCallback(osg::ref_ptr<OpenXRManager> XR, OpenXRSession* session, osg::ref_ptr<OpenXRWorldView> view, osg::GraphicsContext* gc)
                : mXR(XR), mSession(session), mView(view), mGC(gc)
            {}

            void updateSlave(osg::View& view, osg::View::Slave& slave) override;

        private:
            osg::ref_ptr<OpenXRManager> mXR;
            OpenXRSession* mSession;
            osg::ref_ptr<OpenXRWorldView> mView;
            osg::ref_ptr<osg::GraphicsContext> mGC;
        };

    public:
        OpenXRWorldView(osg::ref_ptr<OpenXRManager> XR, std::string name, osg::ref_ptr<osg::State> state, OpenXRSwapchain::Config config, float unitsPerMeter);
        ~OpenXRWorldView();

        //! Prepare for render (update matrices)
        void prerenderCallback(osg::RenderInfo& renderInfo) override;
        //! Projection offset for this view
        osg::Matrix projectionMatrix();
        //! View offset for this view
        osg::Matrix viewMatrix();

        float mUnitsPerMeter = 1.f;
    };
}

#endif
