#ifndef OPENXRWORLDVIEW_HPP
#define OPENXRWORLDVIEW_HPP

#include "openxrview.hpp"

namespace MWVR
{
    class OpenXRWorldView : public OpenXRView
    {
    public:
        enum SubView
        {
            LEFT_VIEW = 0,
            RIGHT_VIEW = 1,
            SUBVIEW_MAX = RIGHT_VIEW, //!< Used to size subview arrays. Not a valid input.
        };

        class UpdateSlaveCallback : public osg::View::Slave::UpdateSlaveCallback
        {
        public:
            UpdateSlaveCallback(osg::ref_ptr<OpenXRManager> XR, osg::ref_ptr<OpenXRWorldView> view, osg::GraphicsContext* gc)
                : mXR(XR), mView(view), mGC(gc)
            {}

            void updateSlave(osg::View& view, osg::View::Slave& slave) override;

        private:
            osg::ref_ptr<OpenXRManager> mXR;
            osg::ref_ptr<OpenXRWorldView> mView;
            osg::ref_ptr<osg::GraphicsContext> mGC;
        };

    public:
        OpenXRWorldView(osg::ref_ptr<OpenXRManager> XR, std::string name, osg::ref_ptr<osg::State> state, float metersPerUnit, SubView view);
        ~OpenXRWorldView();

        //! Prepare for render (update matrices)
        void prerenderCallback(osg::RenderInfo& renderInfo) override;
        //! Projection offset for this view
        osg::Matrix projectionMatrix();
        //! View offset for this view
        osg::Matrix viewMatrix();
        //! Which view this is
        SubView view() { return mView; }

        float mMetersPerUnit = 1.f;
        SubView mView;
    };
}

#endif
