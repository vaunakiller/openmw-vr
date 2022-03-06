#include "viewer.hpp"

#include <osgViewer/Renderer>
#include <osg/StateAttribute>
#include <osg/BufferObject>
#include <osg/VertexArrayState>
#include <osg/Texture2DArray>

#include <components/debug/gldebug.hpp>

#include <components/misc/stringops.hpp>
#include <components/misc/callbackmanager.hpp>
#include <components/misc/constants.hpp>

#include <components/stereo/stereomanager.hpp>
#include <components/stereo/multiview.hpp>

#include <components/vr/layer.hpp>
#include <components/vr/session.hpp>
#include <components/vr/trackingmanager.hpp>

#include <components/sceneutil/depth.hpp>
#include <components/sceneutil/color.hpp>
#include <components/sceneutil/util.hpp>
#include <components/sdlutil/sdlgraphicswindow.hpp>

#include <cassert>

namespace VR
{
    int parseResolution(std::string conf, int recommended, int max)
    {
        if (Misc::StringUtils::isNumber(conf))
        {
            int res = std::atoi(conf.c_str());
            if (res <= 0)
                return recommended;
            if (res > max)
                return max;
            return res;
        }
        conf = Misc::StringUtils::lowerCase(conf);
        if (conf == "auto" || conf == "recommended")
        {
            return recommended;
        }
        if (conf == "max")
        {
            return max;
        }
        return recommended;
    }

    struct UpdateViewCallback : public Stereo::Manager::UpdateViewCallback
    {
        UpdateViewCallback(Viewer* viewer) : mViewer(viewer) {};

        //! Called during the update traversal of every frame to source updated stereo values.
        virtual void updateView(Stereo::View& left, Stereo::View& right) override
        {
            mViewer->updateView(left, right);
        }

        Viewer* mViewer;
    };

    struct SwapBuffersCallback : public osg::GraphicsContext::SwapCallback
    {
    public:
        SwapBuffersCallback(Viewer* viewer) : mViewer(viewer) {};
        void swapBuffersImplementation(osg::GraphicsContext* gc) override
        {
            mViewer->swapBuffersCallback(gc);
        }

    private:
        Viewer* mViewer;
    };

    struct InitialDrawCallback : public Misc::CallbackManager::MwDrawCallback
    {
    public:
        InitialDrawCallback(Viewer* viewer)
            : mViewer(viewer)
        {}

        bool operator()(osg::RenderInfo& info, Misc::CallbackManager::View view) const override
        { 
            mViewer->initialDrawCallback(info, view);
            return true;
        };

    private:

        Viewer* mViewer;
    };

    struct FinaldrawCallback : public Misc::CallbackManager::MwDrawCallback
    {
    public:
        FinaldrawCallback(Viewer* viewer)
            : mViewer(viewer)
        {}

        bool operator()(osg::RenderInfo& info, Misc::CallbackManager::View view) const override 
        {
            mViewer->finalDrawCallback(info, view); 
            return true;
        };

    private:

        Viewer* mViewer;
    };

    Viewer* sViewer = nullptr;

    Viewer& Viewer::instance()
    {
        assert(sViewer);
        return *sViewer;
    }

    Viewer::Viewer(
        std::shared_ptr<VR::Session> session,
        osg::ref_ptr<osgViewer::Viewer> viewer)
        : mSession(session)
        , mViewer(viewer)
        , mSwapBuffersCallback(new SwapBuffersCallback(this))
        , mInitialDraw(new InitialDrawCallback(this))
        , mFinalDraw(new FinaldrawCallback(this))
        , mUpdateViewCallback(new UpdateViewCallback(this))
        , mCallbacksConfigured(false)
    {
        if (!sViewer)
            sViewer = this;
        else
            throw std::logic_error("Duplicated VR::Viewer singleton");

        // Read swapchain configs
        std::array<std::string, 2> xConfString;
        std::array<std::string, 2> yConfString;
        auto swapchainConfigs = VR::Session::instance().getRecommendedSwapchainConfig();
        xConfString[0] = Settings::Manager::getString("left eye resolution x", "VR");
        yConfString[0] = Settings::Manager::getString("left eye resolution y", "VR");
        xConfString[1] = Settings::Manager::getString("right eye resolution x", "VR");
        yConfString[1] = Settings::Manager::getString("right eye resolution y", "VR");

        // Instantiate swapchains for each view
        std::array<const char*, 2> viewNames = {
                "LeftEye",
                "RightEye"
        };
        for (unsigned i = 0; i < viewNames.size(); i++)
        {
            auto width = parseResolution(xConfString[i], swapchainConfigs[i].recommendedWidth, swapchainConfigs[i].maxWidth);
            auto height = parseResolution(yConfString[i], swapchainConfigs[i].recommendedHeight, swapchainConfigs[i].maxHeight);

            Log(Debug::Verbose) << viewNames[i] << " resolution: Recommended x=" << swapchainConfigs[i].recommendedWidth << ", y=" << swapchainConfigs[i].recommendedHeight;
            Log(Debug::Verbose) << viewNames[i] << " resolution: Max x=" << swapchainConfigs[i].maxWidth << ", y=" << swapchainConfigs[i].maxHeight;
            Log(Debug::Verbose) << viewNames[i] << " resolution: Selected x=" << width << ", y=" << height;

            mSubImages[i].width = width;
            mSubImages[i].height = height;
            mSubImages[i].x = mSubImages[i].y = 0;
        }

        // Determine samples and dimensions of framebuffers.
        mFramebufferWidth = mSubImages[0].width;
        mFramebufferHeight = mSubImages[0].height;

        if (mSubImages[0].width != mSubImages[1].width || mSubImages[0].height != mSubImages[1].height)
            Log(Debug::Warning) << "Warning: Eyes have differing resolutions. This case is not implemented";

        mViewer->getCamera()->setViewport(0, 0, mFramebufferWidth, mFramebufferHeight);

        // The gamma resolve framebuffer will be used to write the result of gamma post-processing.
        mGammaResolveFramebuffer = new osg::FrameBufferObject();
        mGammaResolveFramebuffer->setAttachment(osg::Camera::COLOR_BUFFER, osg::FrameBufferAttachment(new osg::RenderBuffer(mFramebufferWidth, mFramebufferHeight, SceneUtil::Color::colorInternalFormat(), 0)));

        mViewer->setReleaseContextAtEndOfFrameHint(false);
        mViewer->getCamera()->getGraphicsContext()->setSwapCallback(mSwapBuffersCallback);
        mViewer->getCamera()->setViewport(0, 0, mFramebufferWidth, mFramebufferHeight);
        Stereo::Manager::instance().overrideEyeResolution(osg::Vec2i(mFramebufferWidth, mFramebufferHeight));

        setupMirrorTexture();

        for (int i : {0, 1})
        {
            mColorSwapchain[i].reset(VR::Session::instance().createSwapchain(mFramebufferWidth, mFramebufferHeight, 1, 1, VR::SwapchainUse::Color, i == 0 ? "LeftEye" : "RightEye"));
            if (mSession->appShouldShareDepthInfo())
            {
                // Depth support is buggy or just not supported on some runtimes and has to be guarded.
                try {
                    mDepthSwapchain[i].reset(VR::Session::instance().createSwapchain(mFramebufferWidth, mFramebufferHeight, 1, 1, VR::SwapchainUse::Depth, i == 0 ? "LeftEye" : "RightEye"));
                }
                catch (...)
                {

                }
                if (!mDepthSwapchain[i])
                {
                    Log(Debug::Warning) << "XR_KHR_composition_layer_depth was enabled, but a depth attachment swapchain could not be created. Depth information will not be submitted.";
                    mSession->setAppShouldShareDepthBuffer(false);
                    mDepthSwapchain[0] = mDepthSwapchain[1] = nullptr;
                }
            }
        }
    }

    Viewer::~Viewer(void)
    {
        sViewer = nullptr;
    }

    static Viewer::MirrorTextureEye mirrorTextureEyeFromString(const std::string& str)
    {
        if (Misc::StringUtils::ciEqual(str, "left"))
            return Viewer::MirrorTextureEye::Left;
        if (Misc::StringUtils::ciEqual(str, "right"))
            return Viewer::MirrorTextureEye::Right;
        if (Misc::StringUtils::ciEqual(str, "both"))
            return Viewer::MirrorTextureEye::Both;
        return Viewer::MirrorTextureEye::Both;
    }

    void Viewer::configureCallbacks()
    {
        if (mCallbacksConfigured)
            return;

        // Give the main camera an initial draw callback that disables camera setup (we don't want it)
        Stereo::Manager::instance().setUpdateViewCallback(mUpdateViewCallback);
        Misc::CallbackManager::instance().addCallback(Misc::CallbackManager::DrawStage::Initial, mInitialDraw);
        Misc::CallbackManager::instance().addCallback(Misc::CallbackManager::DrawStage::Final, mFinalDraw);

        mCallbacksConfigured = true;
    }

    void Viewer::setupMirrorTexture()
    {
        mMirrorTextureEnabled = Settings::Manager::getBool("mirror texture", "VR");
        mMirrorTextureEye = mirrorTextureEyeFromString(Settings::Manager::getString("mirror texture eye", "VR"));
        mFlipMirrorTextureOrder = Settings::Manager::getBool("flip mirror texture order", "VR");

        mMirrorTextureViews.clear();
        if (mMirrorTextureEye == MirrorTextureEye::Left || mMirrorTextureEye == MirrorTextureEye::Both)
            mMirrorTextureViews.push_back(VR::Side_Left);
        if (mMirrorTextureEye == MirrorTextureEye::Right || mMirrorTextureEye == MirrorTextureEye::Both)
            mMirrorTextureViews.push_back(VR::Side_Right);
        if (mFlipMirrorTextureOrder)
            std::reverse(mMirrorTextureViews.begin(), mMirrorTextureViews.end());
    }

    void Viewer::processChangedSettings(const std::set<std::pair<std::string, std::string>>& changed)
    {
        bool mirrorTextureChanged = false;
        for (Settings::CategorySettingVector::const_iterator it = changed.begin(); it != changed.end(); ++it)
        {
            if (it->first == "VR" && it->second == "mirror texture")
            {
                mirrorTextureChanged = true;
            }
            if (it->first == "VR" && it->second == "mirror texture eye")
            {
                mirrorTextureChanged = true;
            }
            if (it->first == "VR" && it->second == "flip mirror texture order")
            {
                mirrorTextureChanged = true;
            }
        }

        if (mirrorTextureChanged)
            setupMirrorTexture();
    }

    bool Viewer::applyGamma(osg::RenderInfo& info, int i)
    {
        osg::State* state = info.getState();
        static const char* vSource = "#version 120\n varying vec2 uv; void main(){ gl_Position = vec4(gl_Vertex.xy*2.0 - 1, 0, 1); uv = gl_Vertex.xy;}";
        static const char* fSource = "#version 120\n varying vec2 uv; uniform sampler2D t; uniform float gamma; uniform float contrast;"
            "void main() {"
            "vec4 color1 = texture2D(t, uv);"
            "vec3 rgb = color1.rgb;"
            "rgb = (rgb - 0.5f) * contrast + 0.5f;"
            "rgb = pow(rgb, vec3(1.0/gamma));"
            "gl_FragColor = vec4(rgb, color1.a);"
            "}";

        static bool first = true;
        static osg::ref_ptr<osg::Program> program = nullptr;
        static osg::ref_ptr<osg::Shader> vShader = nullptr;
        static osg::ref_ptr<osg::Shader> fShader = nullptr;
        static osg::ref_ptr<osg::Uniform> gammaUniform = nullptr;
        static osg::ref_ptr<osg::Uniform> contrastUniform = nullptr;
        osg::Viewport* viewport = nullptr;
        static osg::ref_ptr<osg::StateSet> stateset = nullptr;
        static osg::ref_ptr<osg::Geometry> geometry = nullptr;

        static std::vector<osg::Vec4> vertices =
        {
            {0, 0, 0, 0},
            {1, 0, 0, 0},
            {1, 1, 0, 0},
            {0, 0, 0, 0},
            {1, 1, 0, 0},
            {0, 1, 0, 0}
        };
        static osg::ref_ptr<osg::Vec4Array> vertexArray = new osg::Vec4Array(vertices.begin(), vertices.end());

        if (first)
        {
            geometry = new osg::Geometry();
            geometry->setVertexArray(vertexArray);
            geometry->addPrimitiveSet(new osg::DrawArrays(GL_TRIANGLES, 0, 6));
            geometry->setUseDisplayList(false);
            stateset = geometry->getOrCreateStateSet();

            vShader = new osg::Shader(osg::Shader::Type::VERTEX, vSource);
            fShader = new osg::Shader(osg::Shader::Type::FRAGMENT, fSource);
            program = new osg::Program();
            program->addShader(vShader);
            program->addShader(fShader);
            program->compileGLObjects(*state);
            stateset->setAttributeAndModes(program, osg::StateAttribute::ON);
            stateset->setTextureMode(0, GL_TEXTURE_2D, osg::StateAttribute::PROTECTED);

            gammaUniform = new osg::Uniform("gamma", Settings::Manager::getFloat("gamma", "Video"));
            contrastUniform = new osg::Uniform("contrast", Settings::Manager::getFloat("contrast", "Video"));
            stateset->addUniform(gammaUniform);
            stateset->addUniform(contrastUniform);

            geometry->compileGLObjects(info);

            first = false;
        }

        if (program != nullptr)
        {
            // OSG does not pop statesets until after the final draw callback. Unrelated statesets may therefore still be on the stack at this point.
            // Pop these to avoid inheriting arbitrary state from these. They will not be used more in this frame.
            state->popAllStateSets();
            state->apply();

            gammaUniform->set(Settings::Manager::getFloat("gamma", "Video"));
            contrastUniform->set(Settings::Manager::getFloat("contrast", "Video"));
            stateset->setTextureAttributeAndModes(0, Stereo::Manager::instance().multiviewFramebuffer()->layerColorBuffer(i), osg::StateAttribute::PROTECTED);

            state->pushStateSet(stateset);
            state->apply();

            if (!viewport)
                viewport = new osg::Viewport(0, 0, mFramebufferWidth, mFramebufferHeight);
            viewport->setViewport(0, 0, mFramebufferWidth, mFramebufferHeight);
            viewport->apply(*state);

            glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
            geometry->draw(info);

            state->popStateSet();
            return true;
        }
        return false;
    }

    osg::ref_ptr<osg::FrameBufferObject> Viewer::getXrFramebuffer(uint32_t view, osg::State* state)
    {
        uint64_t colorImage = mColorSwapchain[view]->image();
        uint64_t depthImage = 0;
        uint32_t textureTarget = mColorSwapchain[view]->textureTarget();
        uint32_t arraySize = mColorSwapchain[view]->arraySize();

        if(mSession->appShouldShareDepthInfo())
            depthImage = mDepthSwapchain[view]->image();

        auto it = mSwapchainFramebuffers.find(std::pair{ colorImage, depthImage });
        if (it == mSwapchainFramebuffers.end())
        {
            osg::ref_ptr<osg::FrameBufferObject> fbo = new osg::FrameBufferObject();

            // Wrap subimage textures in texture objects, and attach them to a framebuffer object
            if (textureTarget == GL_TEXTURE_2D)
            {
                auto colorTexture = new osg::Texture2D();
                colorTexture->setTextureSize(mFramebufferWidth, mFramebufferHeight);
                colorTexture->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR);
                colorTexture->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);

                auto colorTextureObject = new osg::Texture::TextureObject(colorTexture, colorImage, GL_TEXTURE_2D);
                colorTexture->setTextureObject(state->getContextID(), colorTextureObject);
                fbo->setAttachment(osg::FrameBufferObject::BufferComponent::COLOR_BUFFER, osg::FrameBufferAttachment(colorTexture));
            }
            else
            {
                auto colorTexture = new osg::Texture2DArray();
                colorTexture->setTextureSize(mFramebufferWidth, mFramebufferHeight, arraySize);
                colorTexture->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR);
                colorTexture->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);

                auto colorTextureObject = new osg::Texture::TextureObject(colorTexture, colorImage, GL_TEXTURE_2D_ARRAY);
                colorTexture->setTextureObject(state->getContextID(), colorTextureObject);
                fbo->setAttachment(osg::FrameBufferObject::BufferComponent::COLOR_BUFFER, osg::FrameBufferAttachment(colorTexture, view));
            }

            if (depthImage != 0)
            {
                if (textureTarget == GL_TEXTURE_2D)
                {
                    auto depthTexture = new osg::Texture2D();
                    depthTexture->setTextureSize(mFramebufferWidth, mFramebufferHeight);
                    depthTexture->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR);
                    depthTexture->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
                    auto depthTextureObject = new osg::Texture::TextureObject(depthTexture, depthImage, GL_TEXTURE_2D);
                    depthTexture->setTextureObject(state->getContextID(), depthTextureObject);
                    fbo->setAttachment(osg::FrameBufferObject::BufferComponent::DEPTH_BUFFER, osg::FrameBufferAttachment(depthTexture));
                }
                else
                {
                    auto depthTexture = new osg::Texture2DArray();
                    depthTexture->setTextureSize(mFramebufferWidth, mFramebufferHeight, arraySize);
                    depthTexture->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR);
                    depthTexture->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
                    auto depthTextureObject = new osg::Texture::TextureObject(depthTexture, depthImage, GL_TEXTURE_2D_ARRAY);
                    depthTexture->setTextureObject(state->getContextID(), depthTextureObject);
                    fbo->setAttachment(osg::FrameBufferObject::BufferComponent::DEPTH_BUFFER, osg::FrameBufferAttachment(depthTexture, view));
                }
            }

            it = mSwapchainFramebuffers.emplace(std::pair{ colorImage, depthImage }, fbo).first;
        }
        return it->second;
    }

    void Viewer::blitXrFramebuffer(osg::State* state, int i)
    {
        auto* gl = osg::GLExtensions::Get(state->getContextID(), false);

        auto dst = getXrFramebuffer(i, state);
        dst->apply(*state, osg::FrameBufferObject::DRAW_FRAMEBUFFER);

        auto width = mSubImages[i].width;
        auto height = mSubImages[i].height;
        bool flip = mColorSwapchain[i]->mustFlipVertical();

        uint32_t srcX0 = 0;
        uint32_t srcX1 = srcX0 + width;
        uint32_t srcY0 = flip ? height : 0;
        uint32_t srcY1 = flip ? 0 : height;
        uint32_t dstX0 = mSubImages[i].x;
        uint32_t dstX1 = dstX0 + width;
        uint32_t dstY0 = mSubImages[i].y;
        uint32_t dstY1 = dstY0 + height;

        mGammaResolveFramebuffer->apply(*state, osg::FrameBufferObject::READ_FRAMEBUFFER);
        gl->glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        if (mSession->appShouldShareDepthInfo())
        {
            Stereo::Manager::instance().multiviewFramebuffer()->layerFbo(i)->apply(*state, osg::FrameBufferObject::READ_FRAMEBUFFER);
            gl->glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
        }

        gl->glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
    }

    void Viewer::blitMirrorTexture(osg::State* state, int i)
    {
        auto* gl = osg::GLExtensions::Get(state->getContextID(), false);

        auto* traits = SDLUtil::GraphicsWindowSDL2::findContext(*mViewer)->getTraits();
        int screenWidth = traits->width;
        int screenHeight = traits->height;

        // Compute the dimensions of each eye on the mirror texture.
        int dstWidth = screenWidth / mMirrorTextureViews.size();

        // Blit each eye
        // Which eye is blitted left/right is determined by which order left/right was added to mMirrorTextureViews
        int dstX = 0;
        gl->glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
        mGammaResolveFramebuffer->apply(*state, osg::FrameBufferObject::READ_FRAMEBUFFER);
        for (auto viewId : mMirrorTextureViews)
        {
            if(viewId == static_cast<unsigned int>(i))
                gl->glBlitFramebuffer(0, 0, mFramebufferWidth, mFramebufferHeight, dstX, 0, dstX + dstWidth, screenHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
            dstX += dstWidth;
        }
    }

    void Viewer::resolveGamma(osg::RenderInfo& info, int i)
    {
        auto* state = info.getState();
        auto* gl = osg::GLExtensions::Get(state->getContextID(), false);

        // Apply gamma by running a shader, sampling the colors that were rendered
        mGammaResolveFramebuffer->apply(*state, osg::FrameBufferObject::DRAW_FRAMEBUFFER);
        bool shouldDoGamma = Settings::Manager::getBool("gamma postprocessing", "VR Debug");
        if (!shouldDoGamma || !applyGamma(info, i))
        {
            // Gamma should not / failed to be applied. Blit the colors unmodified
            Stereo::Manager::instance().multiviewFramebuffer()->layerFbo(i)->apply(*state, osg::FrameBufferObject::READ_FRAMEBUFFER);
            gl->glBlitFramebuffer(0, 0, mFramebufferWidth, mFramebufferHeight, 0, 0, mFramebufferWidth, mFramebufferHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        }
    }

    void Viewer::blit(osg::RenderInfo& info)
    {
        auto* state = info.getState();
        auto* gl = osg::GLExtensions::Get(state->getContextID(), false);

        for (auto i = 0; i < 2; i++)
        {
            mColorSwapchain[i]->beginFrame(state->getGraphicsContext());
            if (mSession->appShouldShareDepthInfo())
                mDepthSwapchain[i]->beginFrame(state->getGraphicsContext());

            resolveGamma(info, i);
            if (mMirrorTextureEnabled)
                blitMirrorTexture(state, i);
            blitXrFramebuffer(state, i);

            mColorSwapchain[i]->endFrame(state->getGraphicsContext());
            if (mSession->appShouldShareDepthInfo())
                mDepthSwapchain[i]->endFrame(state->getGraphicsContext());
        }

        // Undo all framebuffer bindings we have done.
        gl->glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
    }

    void Viewer::initialDrawCallback(osg::RenderInfo& info, Misc::CallbackManager::View view)
    {
        // Should only activate on the first callback
        if (view == Misc::CallbackManager::View::Right)
            return;

        {
            std::scoped_lock lock(mMutex);
            mDrawFrame = mReadyFrames.front();
            mReadyFrames.pop();
        }
        VR::Session::instance().frameBeginRender(mDrawFrame);
    }

    void Viewer::finalDrawCallback(osg::RenderInfo& info, Misc::CallbackManager::View view)
    {
        // Should only activate on the last callback
        if (view == Misc::CallbackManager::View::Left)
            return;
        if (mDrawFrame.shouldRender)
        {
            blit(info);
        }
    }

    void Viewer::swapBuffersCallback(osg::GraphicsContext* gc)
    {
        VR::Session::instance().frameEnd(gc, mDrawFrame);
    }

    void Viewer::updateView(Stereo::View& left, Stereo::View& right)
    {
        auto frame = mSession->newFrame();
        mSession->frameBeginUpdate(frame);

        VR::TrackingManager::instance().updateTracking(frame);

        auto stageViews = VR::Session::instance().getPredictedViews(frame.predictedDisplayTime, VR::ReferenceSpace::Stage);
        auto views = VR::Session::instance().getPredictedViews(frame.predictedDisplayTime, VR::ReferenceSpace::View);

        if (frame.shouldRender && frame.shouldSyncFrameLoop)
        {
            left = views[VR::Side_Left];
            left.pose.position *= Constants::UnitsPerMeter * mSession->playerScale();
            right = views[VR::Side_Right];
            right.pose.position *= Constants::UnitsPerMeter * mSession->playerScale();

            std::shared_ptr<VR::ProjectionLayer> layer = std::make_shared<VR::ProjectionLayer>();
            for (uint32_t i = 0; i < 2; i++)
            {
                layer->views[i].colorSwapchain = mColorSwapchain[i];
                if (mSession->appShouldShareDepthInfo())
                {
                    layer->views[i].depthSwapchain = mDepthSwapchain[i];
                }

                layer->views[i].subImage.index = 0;
                layer->views[i].subImage.width = mFramebufferWidth;
                layer->views[i].subImage.height = mFramebufferHeight;
                layer->views[i].subImage.x = 0;
                layer->views[i].subImage.y = 0;
                layer->views[i].view = stageViews[i];
            }
            frame.layers.push_back(layer);
        }

        std::unique_lock<std::mutex> lock(mMutex);
        mReadyFrames.push(frame);
    }
}
