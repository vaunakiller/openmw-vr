#include "session.hpp"

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
    }

    Session::~Session()
    {
    }

    void Session::processChangedSettings(const std::set<std::pair<std::string, std::string>>& changed)
    {
        setSeatedPlay(Settings::Manager::getBool("seated play", "VR"));
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
        {
            // TODO:
            //Environment::get().getInputManager()->requestRecenter(true);
        }
    }
}

