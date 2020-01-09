#ifndef MWVR_OPENRXVIEWER_H
#define MWVR_OPENRXVIEWER_H

#include <memory>
#include <array>
#include <osg/Group>
#include <osg/Camera>
#include <osgViewer/Viewer>

#include "openxrmanager.hpp"
#include "openxrview.hpp"
#include "openxrinputmanager.hpp"


namespace MWVR
{
    class OpenXRViewer : public osg::Group
    {
    public:
        class UpdateSlaveCallback : public osg::View::Slave::UpdateSlaveCallback
        {
        public:
            UpdateSlaveCallback(osg::ref_ptr<OpenXRManager> XR, osg::ref_ptr<OpenXRView> view)
                : mXR(XR), mView(view)
            {}

            void updateSlave(osg::View& view, osg::View::Slave& slave) override;

        private:
            osg::ref_ptr<OpenXRManager> mXR;
            osg::ref_ptr<OpenXRView> mView;
        };

    public:
        enum Views
        {
            LEFT_VIEW = 0,
            RIGHT_VIEW = 1
        };

    public:
        //! Create an OpenXR manager based on the graphics context from the given window.
        //! The OpenXRManager will make its own context with shared resources.
        OpenXRViewer(
            osg::ref_ptr<OpenXRManager> XR,
            osg::ref_ptr<OpenXRManager::RealizeOperation> realizeOperation,
            osg::ref_ptr<osgViewer::Viewer> viewer,
            float metersPerUnit = 1.f);

        ~OpenXRViewer(void);

        virtual void traverse(osg::NodeVisitor& visitor) override;

    protected:
        virtual bool configure();

        osg::observer_ptr<OpenXRManager> mXR = nullptr;
        osg::observer_ptr<OpenXRManager::RealizeOperation> mRealizeOperation = nullptr;
        osg::observer_ptr<osgViewer::Viewer> mViewer = nullptr;
        std::unique_ptr<MWVR::OpenXRInputManager> mXRInput = nullptr;
        std::array<osg::ref_ptr<OpenXRView>, 2> mViews{};
        float mMetersPerUnit = 1.f;
        bool mConfigured = false;
    };
}

#endif
