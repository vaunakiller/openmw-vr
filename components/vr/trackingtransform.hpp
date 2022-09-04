#ifndef VR_TRACKING_TRANFORM_H
#define VR_TRACKING_TRANFORM_H

#include <osg/MatrixTransform>
#include "trackinglistener.hpp"

namespace VR
{
    //! Acts as a osg::MatrixTransform using a matrix build from tracking data from the provided path
    class TrackingTransform : public osg::MatrixTransform, public TrackingListener
    {
    public:
        TrackingTransform(VRPath path);

        virtual void onTrackingUpdated(TrackingManager& manager, DisplayTime predictedDisplayTime);

        VRPath path() const { return mPath; };

    protected:
        VRPath mPath;
    };
}

#endif
