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

    public:
        OpenXRWorldView(osg::ref_ptr<OpenXRManager> XR, osg::ref_ptr<osg::State> state, float metersPerUnit, SubView view);
        ~OpenXRWorldView();

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
