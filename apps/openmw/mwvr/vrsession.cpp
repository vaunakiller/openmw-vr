#include "vrsession.hpp"
#include "vrgui.hpp"
#include "vrenvironment.hpp"
#include "vrinputmanager.hpp"
#include "openxrmanager.hpp"
#include "openxrswapchain.hpp"
#include "../mwinput/inputmanagerimp.hpp"
#include "../mwbase/environment.hpp"
#include "../mwbase/statemanager.hpp"

#include <components/debug/debuglog.hpp>
#include <components/sdlutil/sdlgraphicswindow.hpp>
#include <components/misc/stringops.hpp>
#include <components/misc/constants.hpp>

#include <osg/Camera>

#include <algorithm>
#include <vector>
#include <array>
#include <iostream>
#include <time.h>
#include <thread>

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

namespace MWVR
{
    static void swapConvention(osg::Vec3& v3)
    {

        float y = v3.y();
        float z = v3.z();
        v3.y() = z;
        v3.z() = -y;
    }
    static void swapConvention(osg::Quat& q)
    {
        float y = q.y();
        float z = q.z();
        q.y() = z;
        q.z() = -y;
    }

    VRSession::VRSession()
        : mXrSyncPhase{FramePhase::Cull}
    {
        mHandDirectedMovement = Settings::Manager::getBool("hand directed movement", "VR");
        auto syncPhase = Settings::Manager::getString("openxr sync phase", "VR Debug");
        syncPhase = Misc::StringUtils::lowerCase(syncPhase);
        if (syncPhase == "update")
            mXrSyncPhase = FramePhase::Update;
        else if (syncPhase == "cull")
            mXrSyncPhase = FramePhase::Cull;
        else if (syncPhase == "draw")
            mXrSyncPhase = FramePhase::Draw;
        else if (syncPhase == "swap")
            mXrSyncPhase = FramePhase::Swap;
        else
        {
            Log(Debug::Verbose) << "Invalid openxr sync phase " << syncPhase << ", defaulting to cull";
            return;
        }
        Log(Debug::Verbose) << "Using openxr sync phase " << syncPhase;
    }

    VRSession::~VRSession()
    {
    }

    osg::Matrix VRSession::projectionMatrix(FramePhase phase, Side side)
    {
        assert(((int)side) < 2);
        auto fov = predictedPoses(VRSession::FramePhase::Update).view[(int)side].fov;
        float near_ = Settings::Manager::getFloat("near clip", "Camera");
        float far_ = Settings::Manager::getFloat("viewing distance", "Camera");
        return fov.perspectiveMatrix(near_, far_);
    }

    void VRSession::processChangedSettings(const std::set<std::pair<std::string, std::string>>& changed)
    {
        for (Settings::CategorySettingVector::const_iterator it = changed.begin(); it != changed.end(); ++it)
        {
            if (it->first == "VR" && it->second == "hand directed movement")
            {
                mHandDirectedMovement = Settings::Manager::getBool("hand directed movement", "VR");
            }
        }
    }

    osg::Matrix VRSession::viewMatrix(osg::Vec3 position, osg::Quat orientation)
    {
        position = position * Constants::UnitsPerMeter;

        swapConvention(position);
        swapConvention(orientation);

        osg::Matrix viewMatrix;
        viewMatrix.setTrans(-position);
        viewMatrix.postMultRotate(orientation.conj());
        return viewMatrix;
    }

    osg::Matrix VRSession::viewMatrix(FramePhase phase, Side side, bool offset, bool glConvention)
    {
        if (offset)
        {
            MWVR::Pose pose = predictedPoses(phase).view[(int)side].pose;
            auto position = pose.position * Constants::UnitsPerMeter;
            auto orientation = pose.orientation;

            if (glConvention)
            {
                swapConvention(position);
                swapConvention(orientation);
            }

            osg::Matrix viewMatrix;
            viewMatrix.setTrans(-position);
            viewMatrix.postMultRotate(orientation.conj());
            return viewMatrix;
        }
        else
        {
            MWVR::Pose pose = predictedPoses(phase).eye[(int)side];
            osg::Vec3 position = pose.position * Constants::UnitsPerMeter;
            osg::Quat orientation = pose.orientation;
            osg::Vec3 forward = orientation * osg::Vec3(0, 1, 0);
            osg::Vec3 up = orientation * osg::Vec3(0, 0, 1);

            if (glConvention)
            {
                swapConvention(position);
                swapConvention(orientation);
                swapConvention(forward);
                swapConvention(up);
            }

            osg::Matrix viewMatrix;
            viewMatrix.makeLookAt(position, position + forward, up);

            return viewMatrix;
        }
    }

    void VRSession::swapBuffers(osg::GraphicsContext* gc, VRViewer& viewer)
    {
        auto* xr = Environment::get().getManager();

        beginPhase(FramePhase::Swap);

        auto* frameMeta = getFrame(FramePhase::Swap).get();

        if (frameMeta->mShouldSyncFrameLoop)
        {
            if (frameMeta->mShouldRender)
            {
                viewer.blitEyesToMirrorTexture(gc);
                gc->swapBuffersImplementation();
                std::array<CompositionLayerProjectionView, 2> layerStack{};
                layerStack[(int)Side::LEFT_SIDE].subImage = viewer.subImage(Side::LEFT_SIDE);
                layerStack[(int)Side::RIGHT_SIDE].subImage = viewer.subImage(Side::RIGHT_SIDE);
                layerStack[(int)Side::LEFT_SIDE].pose = frameMeta->mPredictedPoses.eye[(int)Side::LEFT_SIDE] / mPlayerScale;
                layerStack[(int)Side::RIGHT_SIDE].pose = frameMeta->mPredictedPoses.eye[(int)Side::RIGHT_SIDE] / mPlayerScale;
                layerStack[(int)Side::LEFT_SIDE].fov = frameMeta->mPredictedPoses.view[(int)Side::LEFT_SIDE].fov;
                layerStack[(int)Side::RIGHT_SIDE].fov = frameMeta->mPredictedPoses.view[(int)Side::RIGHT_SIDE].fov;
                xr->endFrame(frameMeta->mFrameInfo, &layerStack);
            }
            else
            {
                gc->swapBuffersImplementation();
                xr->endFrame(frameMeta->mFrameInfo, nullptr);
            }

            xr->xrResourceReleased();
        }

        {
            std::unique_lock<std::mutex> lock(mMutex);
            mLastRenderedFrame = frameMeta->mFrameNo;
            getFrame(FramePhase::Swap) = nullptr;
        }

        mCondition.notify_one();
    }

    void VRSession::beginPhase(FramePhase phase)
    {
        Log(Debug::Debug) << "beginPhase(" << ((int)phase) << ") " << std::this_thread::get_id();

        auto& frame = getFrame(phase);
        if (frame)
        {
            // Happens once during startup but can be ignored that time.
            // TODO: This issue would be cleaned up if beginPhase(Update) was called at a more appropriate location.
            Log(Debug::Warning) << "advanceFramePhase called with a frame alreay in the target phase";
            return;
        }


        if (phase == FramePhase::Update)
        {
            prepareFrame();
        }
        else
        {
            std::unique_lock<std::mutex> lock(mMutex);
            FramePhase previousPhase = static_cast<FramePhase>((int)phase - 1);
            if (!getFrame(previousPhase))
                throw std::logic_error("beginPhase called without a frame");
            frame = std::move(getFrame(previousPhase));
        }
        if (phase == mXrSyncPhase && frame->mShouldSyncFrameLoop)
        {
            // We may reach this point before xrEndFrame of the previous frame
            // Must wait or openxr will interpret another call to xrBeginFrame() as skipping a frame
            std::unique_lock<std::mutex> lock(mMutex);
            while (mLastRenderedFrame != frame->mFrameNo - 1)
            {
                mCondition.wait(lock);
            }

            Environment::get().getManager()->beginFrame();
        }
    }

    std::unique_ptr<VRSession::VRFrameMeta>& VRSession::getFrame(FramePhase phase)
    {
        if ((unsigned int)phase >= mFrame.size())
            throw std::logic_error("Invalid frame phase");
        return mFrame[(int)phase];
    }

    void VRSession::prepareFrame()
    {
        mFrames++;

        auto* xr = Environment::get().getManager();
        xr->handleEvents();
        auto& frame = getFrame(FramePhase::Update);
        frame.reset(new VRFrameMeta);

        frame->mFrameNo = mFrames;
        frame->mShouldSyncFrameLoop = xr->appShouldSyncFrameLoop();
        frame->mShouldRender = xr->appShouldRender();
        if (frame->mShouldSyncFrameLoop)
        {
            frame->mFrameInfo = xr->waitFrame();
            frame->mShouldRender |= frame->mFrameInfo.runtimeRequestsRender;
            xr->xrResourceAcquired();

            if (frame->mShouldRender)
            {
                frame->mPredictedDisplayTime = frame->mFrameInfo.runtimePredictedDisplayTime;

                PoseSet predictedPoses{};

                xr->enablePredictions();
                predictedPoses.head = xr->getPredictedHeadPose(frame->mPredictedDisplayTime, ReferenceSpace::STAGE) * mPlayerScale;
                auto hmdViews = xr->getPredictedViews(frame->mPredictedDisplayTime, ReferenceSpace::VIEW);
                predictedPoses.view[(int)Side::LEFT_SIDE].pose = hmdViews[(int)Side::LEFT_SIDE].pose * mPlayerScale;
                predictedPoses.view[(int)Side::RIGHT_SIDE].pose = hmdViews[(int)Side::RIGHT_SIDE].pose * mPlayerScale;
                predictedPoses.view[(int)Side::LEFT_SIDE].fov = hmdViews[(int)Side::LEFT_SIDE].fov;
                predictedPoses.view[(int)Side::RIGHT_SIDE].fov = hmdViews[(int)Side::RIGHT_SIDE].fov;
                auto stageViews = xr->getPredictedViews(frame->mPredictedDisplayTime, ReferenceSpace::STAGE);
                predictedPoses.eye[(int)Side::LEFT_SIDE] = stageViews[(int)Side::LEFT_SIDE].pose * mPlayerScale;
                predictedPoses.eye[(int)Side::RIGHT_SIDE] = stageViews[(int)Side::RIGHT_SIDE].pose * mPlayerScale;

                auto* input = Environment::get().getInputManager();
                if (input)
                {
                    predictedPoses.hands[(int)Side::LEFT_SIDE] = input->getLimbPose(frame->mPredictedDisplayTime, TrackedLimb::LEFT_HAND) * mPlayerScale;
                    predictedPoses.hands[(int)Side::RIGHT_SIDE] = input->getLimbPose(frame->mPredictedDisplayTime, TrackedLimb::RIGHT_HAND) * mPlayerScale;
                }
                xr->disablePredictions();

                frame->mPredictedPoses = predictedPoses;
            }
        }
    }

    const PoseSet& VRSession::predictedPoses(FramePhase phase)
    {
        auto& frame = getFrame(phase);

        // TODO: Manage execution order properly instead of this hack
        if (phase == FramePhase::Update && !frame)
            beginPhase(FramePhase::Update);

        if (!frame)
            throw std::logic_error("Attempted to get poses from a phase with no current pose");
        return frame->mPredictedPoses;
    }

    // OSG doesn't provide API to extract euler angles from a quat, but i need it.
    // Credits goes to Dennis Bunfield, i just copied his formula https://narkive.com/v0re6547.4
    void getEulerAngles(const osg::Quat& quat, float& yaw, float& pitch, float& roll)
    {
        // Now do the computation
        osg::Matrixd m2(osg::Matrixd::rotate(quat));
        double* mat = (double*)m2.ptr();
        double angle_x = 0.0;
        double angle_y = 0.0;
        double angle_z = 0.0;
        double D, C, tr_x, tr_y;
        angle_y = D = asin(mat[2]); /* Calculate Y-axis angle */
        C = cos(angle_y);
        if (fabs(C) > 0.005) /* Test for Gimball lock? */
        {
            tr_x = mat[10] / C; /* No, so get X-axis angle */
            tr_y = -mat[6] / C;
            angle_x = atan2(tr_y, tr_x);
            tr_x = mat[0] / C; /* Get Z-axis angle */
            tr_y = -mat[1] / C;
            angle_z = atan2(tr_y, tr_x);
        }
        else /* Gimball lock has occurred */
        {
            angle_x = 0; /* Set X-axis angle to zero
            */
            tr_x = mat[5]; /* And calculate Z-axis angle
            */
            tr_y = mat[4];
            angle_z = atan2(tr_y, tr_x);
        }

        yaw = angle_z;
        pitch = angle_x;
        roll = angle_y;
    }

    void VRSession::movementAngles(float& yaw, float& pitch)
    {
        if (!getFrame(FramePhase::Update))
            beginPhase(FramePhase::Update);
        
        if (mHandDirectedMovement)
        {
            auto frameMeta = getFrame(FramePhase::Update).get();

            float headYaw = 0.f;
            float headPitch = 0.f;
            float headsWillRoll = 0.f;

            float handYaw = 0.f;
            float handPitch = 0.f;
            float handRoll = 0.f;

            getEulerAngles(frameMeta->mPredictedPoses.head.orientation, headYaw, headPitch, headsWillRoll);
            getEulerAngles(frameMeta->mPredictedPoses.hands[(int)Side::LEFT_SIDE].orientation, handYaw, handPitch, handRoll);

            yaw = handYaw - headYaw;
            pitch = handPitch - headPitch;
        }
    }

}

