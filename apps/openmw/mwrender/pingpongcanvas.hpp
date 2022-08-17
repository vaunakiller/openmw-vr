#ifndef OPENMW_MWRENDER_PINGPONGCANVAS_H
#define OPENMW_MWRENDER_PINGPONGCANVAS_H

#include <array>
#include <optional>

#include <osg/Geometry>
#include <osg/Texture2D>
#include <osg/FrameBufferObject>

#include <components/fx/technique.hpp>

#include "postprocessor.hpp"
#include "luminancecalculator.hpp"

namespace Shader
{
    class ShaderManager;
}

namespace MWRender
{
    class PingPongCanvas : public osg::Geometry
    {
    public:
        class PingPongCallback
        {
        public:
            virtual ~PingPongCallback() {};

            virtual void pingPongBegin(size_t frameId, osg::State& state, const PingPongCanvas& canvas) = 0;
            virtual void pingPongEnd(size_t frameId, osg::State& state, const PingPongCanvas& canvas) = 0;
        };

    public:
        PingPongCanvas(Shader::ShaderManager& shaderManager);

        void drawImplementation(osg::RenderInfo& renderInfo) const override;

        void dirty(size_t frameId) { mBufferData[frameId].dirty = true; }

        const fx::DispatchArray& getCurrentFrameData(size_t frame) { return mBufferData[frame % 2].data; }

        // Sets current frame pass data and stores copy of dispatch array to apply to next frame data
        void setCurrentFrameData(size_t frameId, fx::DispatchArray&& data);

        void setMask(size_t frameId, bool underwater, bool exterior);

        void setSceneTexture(size_t frameId, osg::ref_ptr<osg::Texture> tex) { mBufferData[frameId].sceneTex = tex; }

        void setLDRSceneTexture(size_t frameId, osg::ref_ptr<osg::Texture> tex) { mBufferData[frameId].sceneTexLDR = tex; }

        void setDepthTexture(size_t frameId, osg::ref_ptr<osg::Texture> tex) { mBufferData[frameId].depthTex = tex; }

        void setNormalsTexture(size_t frameId, osg::ref_ptr<osg::Texture> tex) { mBufferData[frameId].normalsTex = tex; }

        void setHDR(size_t frameId, bool hdr) { mBufferData[frameId].hdr = hdr; }

        void setPostProcessing(size_t frameId, bool postprocessing) { mBufferData[frameId].postprocessing = postprocessing; }

        const osg::ref_ptr<osg::Texture>& getSceneTexture(size_t frameId) const { return mBufferData[frameId].sceneTex; }

        void drawGeometry(osg::RenderInfo& renderInfo) const;

        void setDestinationFbo(size_t frameId, osg::ref_ptr<osg::FrameBufferObject> fbo) const { mBufferData[frameId].destination = fbo; }
        void setDestinationViewport(size_t frameId, osg::ref_ptr<osg::Viewport> viewport) const { mBufferData[frameId].destinationViewport = viewport; }

        osg::ref_ptr<osg::Texture> getDepthTexture(size_t frameId) const { return mBufferData[frameId].depthTex; }

        void setPingPongCallback(std::unique_ptr<PingPongCallback> cb);

    private:
        void copyNewFrameData(size_t frameId) const;

        mutable LuminanceCalculator mLuminanceCalculator;

        osg::ref_ptr<osg::Program> mFallbackProgram;
        osg::ref_ptr<osg::Program> mMultiviewResolveProgram;
        osg::ref_ptr<osg::StateSet> mFallbackStateSet;
        osg::ref_ptr<osg::StateSet> mMultiviewResolveStateSet;
        mutable osg::ref_ptr<osg::FrameBufferObject> mMultiviewResolveFramebuffer;

        struct BufferData
        {
            bool dirty = false;
            bool hdr = false;
            bool postprocessing = true;

            fx::DispatchArray data;
            fx::FlagsType mask;

            osg::ref_ptr<osg::FrameBufferObject> destination;
            osg::ref_ptr<osg::Viewport> destinationViewport;

            osg::ref_ptr<osg::Texture> sceneTex;
            osg::ref_ptr<osg::Texture> depthTex;
            osg::ref_ptr<osg::Texture> sceneTexLDR;
            osg::ref_ptr<osg::Texture> normalsTex;
        };

        mutable std::array<BufferData, 2> mBufferData;
        mutable std::array<osg::ref_ptr<osg::FrameBufferObject>, 3> mFbos;
        mutable osg::ref_ptr<osg::Viewport> mRenderViewport;

        std::unique_ptr<PingPongCallback> mPingPongCallback;
    };
}

#endif
