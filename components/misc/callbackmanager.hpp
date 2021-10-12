#ifndef MISC_CALLBACKMANAGER_H
#define MISC_CALLBACKMANAGER_H

#include <osg/Camera>
#include <osgUtil/CullVisitor>

#include <vector>
#include <array>
#include <mutex>
#include <condition_variable>
#include <memory>

namespace osgViewer
{
    class Viewer;
}

namespace Misc
{
    struct CallbackInfo;

    /// Manager of DrawCallbacks on an OSG camera.
    /// The motivation behind this class is that OSG's draw callbacks are inherently thread unsafe.
    /// Primarily, as the main thread has no way to synchronize with the draw thread when adding/removing draw callbacks, 
    /// when adding a draw callback the callback must manually compare frame numbers to make sure it doesn't fire for the 
    /// previous frame's draw traversals. This class automates this.
    /// Secondly older versions of OSG that are still supported by openmw do not support nested callbacks. And the ones that do
    /// make no attempt at synchronizing adding/removing nested callbacks, causing a potentially fatal race condition when needing
    /// to dynamically add or remove a nested callback.
    class CallbackManager
    {

    public:
        enum class DrawStage
        {
            Initial = 0, 
            PreDraw = 1, 
            PostDraw = 2, 
            Final = 3,
            StagesMax = 4
        };
        enum class View
        {
            Left, Right, NotApplicable
        };

        //! A draw callback that adds stereo information to the operator by means of the StereoDrawCallback::View enumerator.
        //! When not rendering stereo, this enum will always be "NotApplicable".
        //! 
        //! When rendering stereo, this enum will correspond to the current draw traversal.
        //! If the stereo method requires two traversals, one callback will be issued for each (one Left, and one Right).
        //! If the stereo method has only one traversal (geometry shaders, ovr multiview, instancing, etc.) then the enum will always be "NotApplicable".
        //! 
        //! Note that every callback that changes behaviour with stereo or number of traversals should use the MwDrawCallback and vary behaviour by View,
        //! even if it does not care about Stereo.
        //! For example, a common pattern is to use the FinalDrawCallback to sync on end of frame rendering. If activated on View::Left, this will be activated
        //! when only one of two views has completed rendering.

        struct MwDrawCallback
        {
        public:
            virtual ~MwDrawCallback() = default;

            //! The callback operator to be overriden.
            //! \return A bool that is true if the callback was run, and false if it was ignored. Oneshot callbacks are only consumed when the returned value is true.
            virtual bool operator()(osg::RenderInfo& info, View view) const = 0;
        };

        static CallbackManager& instance();

        CallbackManager(osg::ref_ptr<osgViewer::Viewer> viewer);
        ~CallbackManager();

        /// Internal
        void callback(DrawStage stage, View view, osg::RenderInfo& info);

        /// Add a callback to a specific stage
        void addCallback(DrawStage stage, std::shared_ptr<MwDrawCallback> cb);

        /// Remove a callback from a specific stage
        void removeCallback(DrawStage stage, std::shared_ptr<MwDrawCallback> cb);

        /// Add a callback that will only fire once before being automatically removed.
        void addCallbackOneshot(DrawStage stage, std::shared_ptr<MwDrawCallback> cb);

        /// Waits for a oneshot callback to complete. Returns immediately if already complete or no such callback exists
        void waitCallbackOneshot(DrawStage stage, std::shared_ptr<MwDrawCallback> cb);

        /// Determine which view the cull visitor belongs to
        View getView(const osgUtil::CullVisitor* cv) const;

    private:
        bool hasOneshot(DrawStage stage, std::shared_ptr<MwDrawCallback> cb);

        std::array<std::vector<CallbackInfo>, static_cast<int>(DrawStage::StagesMax) > mUserCallbacks;
        std::mutex mMutex;
        std::condition_variable mCondition;

        osg::ref_ptr<osgViewer::Viewer> mViewer;

        using Identifier = osgUtil::CullVisitor::Identifier;
        osg::ref_ptr<Identifier> mIdentifierMain = new Identifier();
        osg::ref_ptr<Identifier> mIdentifierLeft = new Identifier();
        osg::ref_ptr<Identifier> mIdentifierRight = new Identifier();
    };
}

#endif
