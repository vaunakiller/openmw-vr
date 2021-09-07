#ifndef MISC_CALLBACKMANAGER_H
#define MISC_CALLBACKMANAGER_H

#include <osg/Camera>
#include <osgViewer/Viewer>

#include <vector>
#include <map>
#include <mutex>
#include <condition_variable>
#include <memory>

namespace Misc
{
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
            Initial, PreDraw, PostDraw, Final
        };
        enum class View
        {
            Left, Right, Both, NotStereo
        };

        //! A draw callback that adds stereo information to the operator by means of the StereoDrawCallback::View enumerator.
        //! When not rendering stereo, this enum will always be "NotStereo".
        //! 
        //! When rendering stereo, this enum will correspond to the current draw traversal.
        //! If the stereo method requires two traversals, one callback will be issued for each (one Left, and one Right).
        //! If the stereo method has only one traversal (geometry shaders, ovr multiview, instancing, etc.) then the enum will always be "Both".
        //! 
        //! Note that every callback that changes behaviour with stereo or number of traversals should use the StereoDrawCallback and vary behaviour by View,
        //! even if it does not care about Stereo.
        //! For example, a common pattern is to use the FinalDrawCallback to sync on end of frame rendering. If activated on View::Left, this will be activated
        //! when only one of two views has completed rendering.
        struct DrawCallback
        {
        public:
        public:

            virtual void run(osg::RenderInfo& info, View view) const = 0;

        private:
        };

        struct CallbackInfo
        {
            std::shared_ptr<DrawCallback> callback = nullptr;
            unsigned int frame = 0;
            bool oneshot = false;
        };

        static CallbackManager& instance();

        CallbackManager(osg::ref_ptr<osgViewer::Viewer> viewer);

        /// Internal
        void callback(DrawStage stage, View view, osg::RenderInfo& info);

        /// Add a callback to a specific stage
        void addCallback(DrawStage stage, std::shared_ptr<DrawCallback> cb);

        /// Remove a callback from a specific stage
        void removeCallback(DrawStage stage, std::shared_ptr<DrawCallback> cb);

        /// Add a callback that will only fire once before being automatically removed.
        void addCallbackOneshot(DrawStage stage, std::shared_ptr<DrawCallback> cb);

        /// Waits for a oneshot callback to complete. Returns immediately if already complete or no such callback exists
        void waitCallbackOneshot(DrawStage stage, std::shared_ptr<DrawCallback> cb);


    private:

        bool hasOneshot(DrawStage stage, std::shared_ptr<DrawCallback> cb);

        std::map<DrawStage, std::vector<CallbackInfo> > mUserCallbacks;
        std::mutex mMutex;
        std::condition_variable mCondition;

        uint32_t mFrame;
        osg::ref_ptr<osgViewer::Viewer> mViewer;
    };
}

#endif
