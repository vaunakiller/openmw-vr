#include "session.hpp"

#include <components/debug/debuglog.hpp>
#include <components/sdlutil/sdlgraphicswindow.hpp>
#include <components/misc/stringops.hpp>
#include <components/misc/constants.hpp>
#include <components/vr/trackingpath.hpp>
#include <components/vr/trackingsource.hpp>
#include <components/vr/vr.hpp>

#include <osg/Camera>

#include <algorithm>
#include <vector>
#include <array>
#include <iostream>
#include <time.h>
#include <thread>
#include <chrono>
#include <cassert>

namespace VR
{
    Session* sSession = nullptr;

    Session& Session::instance()
    {
        assert(sSession);
        return *sSession;
    }

    Session::Session()
    {
        if (!sSession)
            sSession = this;
        else
            throw std::logic_error("Duplicated VR::Session singleton");

        mSeatedPlay = Settings::Manager::getBool("seated play", "VR");
        mHandDirectedMovement = Settings::Manager::getBool("hand directed movement", "VR");


        auto stageUserHeadPath = VR::stringToVRPath("/stage/user/head/input/pose");
        auto worldUserPath = VR::stringToVRPath("/world/user");
        auto worldUserHeadPath = VR::stringToVRPath("/world/user/head/input/pose");
        mTrackerToWorldBinding = std::make_unique<VR::StageToWorldBinding>(worldUserPath, stageUserHeadPath);
        mTrackerToWorldBinding->bindPaths(worldUserHeadPath, stageUserHeadPath);
    }

    Session::~Session()
    {
    }

    void Session::processChangedSettings(const std::set<std::pair<std::string, std::string>>& changed)
    {
        setSeatedPlay(Settings::Manager::getBool("seated play", "VR"));
        mHandDirectedMovement = Settings::Manager::getBool("hand directed movement", "VR");
    }

    VR::Frame Session::newFrame()
    {
        static uint64_t frameNo = 0;
        VR::Frame frame;
        frame.frameNumber = frameNo++;
        newFrame(frame.frameNumber, frame.shouldSyncFrameLoop, frame.shouldSyncInput);
        return frame;
    }

    void Session::frameBeginUpdate(VR::Frame& frame)
    {
        if (frame.shouldSyncFrameLoop)
            syncFrameUpdate(frame.frameNumber, frame.shouldRender, frame.predictedDisplayTime, frame.predictedDisplayPeriod);
    }

    void Session::frameBeginRender(VR::Frame& frame)
    {
        if (frame.shouldSyncFrameLoop)
            syncFrameRender(frame);
    }

    void Session::frameEnd(osg::GraphicsContext* gc, VR::Frame& frame)
    {
        gc->swapBuffersImplementation();
        if (frame.shouldSyncFrameLoop)
            syncFrameEnd(frame);
    }

    void Session::setSeatedPlay(bool seatedPlay)
    {
        std::swap(mSeatedPlay, seatedPlay);
        if (mSeatedPlay != seatedPlay)
            requestRecenter(true);
    }

    void Session::computePlayerScale()
    {
        mPlayerScale = mCharHeight / Settings::Manager::getFloat("player height", "VR");
        Log(Debug::Verbose) << "Calculated player scale: " << mPlayerScale;
        requestRecenter(true);
    }

    void Session::setCharHeight(float height)
    {
        mCharHeight = height;
        computePlayerScale();
    }

    void Session::requestRecenter(bool recenterZ)
    {
        mTrackerToWorldBinding->recenter(recenterZ);
        mTrackerToWorldBinding->setSeatedPlay(seatedPlay());
        mTrackerToWorldBinding->setEyeLevel(charHeight() * Constants::UnitsPerMeter);
    }

    void Session::instantTransition()
    {
        if (mTrackerToWorldBinding)
            mTrackerToWorldBinding->instantTransition();
    }

    VR::StageToWorldBinding& Session::stageToWorldBinding()
    {
        return *mTrackerToWorldBinding;
    }

    void Session::setInteractionProfileActive(VRPath topLevelPath, bool active)
    {
        if (topLevelPath == stringToVRPath("/user/hand/left"))
            setLeftControllerActive(active);
        if (topLevelPath == stringToVRPath("/user/hand/right"))
            setRightControllerActive(active);

        if (active)
            mActiveInteractionProfiles.insert(topLevelPath);
        else
            mActiveInteractionProfiles.erase(topLevelPath);
    }

    bool Session::getInteractionProfileActive(VRPath topLevelPath) const
    {
        return !!mActiveInteractionProfiles.count(topLevelPath);
    }
}

