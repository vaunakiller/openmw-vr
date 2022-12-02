#include "trackingsource.hpp"
#include "trackingmanager.hpp"
#include "frame.hpp"
#include "session.hpp"

#include <components/debug/debuglog.hpp>
#include <components/misc/constants.hpp>
#include "trackingtransform.hpp"

namespace VR
{
    TrackingTransform::TrackingTransform(VRPath path)
        : mPath(path)
    {
    }

    void TrackingTransform::onTrackingUpdated(TrackingManager& manager, DisplayTime predictedDisplayTime)
    {
        auto pose = manager.locate(mPath, predictedDisplayTime);
        if (!!pose.status)
        {
            _matrix.makeIdentity();
            _matrix.preMultTranslate(pose.pose.position.asMWUnits());
            _matrix.preMultRotate(pose.pose.orientation);
            _inverseDirty = true;
        }
    }
}
