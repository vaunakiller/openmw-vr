#include "viewer.hpp"

#include <osgViewer/Renderer>
#include <osg/StateAttribute>
#include <osg/BufferObject>
#include <osg/VertexArrayState>
#include <osg/Texture2DArray>

#include <components/debug/gldebug.hpp>

#include <components/misc/stringops.hpp>
#include <components/misc/stereo.hpp>
#include <components/misc/callbackmanager.hpp>
#include <components/misc/constants.hpp>

#include <components/vr/layer.hpp>
#include <components/vr/session.hpp>
#include <components/vr/trackingmanager.hpp>

#include <components/sdlutil/sdlgraphicswindow.hpp>

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

    struct UpdateViewCallback : public Misc::StereoView::UpdateViewCallback
    {
        UpdateViewCallback(Viewer* viewer) : mViewer(viewer) {};

        //! Called during the update traversal of every frame to source updated stereo values.
        virtual void updateView(Misc::View& left, Misc::View& right) override
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

    struct PredrawCallback : public Misc::CallbackManager::MwDrawCallback
    {
    public:
        PredrawCallback(Viewer* viewer)
            : mViewer(viewer)
        {}

        bool operator()(osg::RenderInfo& info, Misc::CallbackManager::View view) const override 
        { 
            mViewer->preDrawCallback(info, view); 
            return true;
        };

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

    Viewer::Viewer(
        std::unique_ptr<VR::Session> session,
        osg::ref_ptr<osgViewer::Viewer> viewer)
        : mSession(std::move(session))
        , mViewer(viewer)
        , mSwapBuffersCallback(new SwapBuffersCallback(this))
        , mInitialDraw(new InitialDrawCallback(this))
        , mPreDraw(new PredrawCallback(this))
        , mFinalDraw(new FinaldrawCallback(this))
        , mUpdateViewCallback(new UpdateViewCallback(this))
        , mCallbacksConfigured(false)
    {
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

            // Note: Multisampling is applied to the OSG framebuffers' renderbuffers, and is resolved during post-processing.
            // So we request 1 sample to get non-multisampled framebuffers from OpenXR.
            auto samples = 1;

            Log(Debug::Verbose) << viewNames[i] << " resolution: Recommended x=" << swapchainConfigs[i].recommendedWidth << ", y=" << swapchainConfigs[i].recommendedHeight;
            Log(Debug::Verbose) << viewNames[i] << " resolution: Max x=" << swapchainConfigs[i].maxWidth << ", y=" << swapchainConfigs[i].maxHeight;
            Log(Debug::Verbose) << viewNames[i] << " resolution: Selected x=" << width << ", y=" << height;

            mColorSwapchain[i].reset(VR::Session::instance().createSwapchain(width, height, samples, VR::SwapchainUse::Color, viewNames[i]));
            mDepthSwapchain[i].reset(VR::Session::instance().createSwapchain(width, height, samples, VR::SwapchainUse::Depth, viewNames[i]));

            mSubImages[i].width = width;
            mSubImages[i].height = height;
            mSubImages[i].x = mSubImages[i].y = 0;
        }

        // Determine samples and dimensions of framebuffers.
        auto renderbufferSamples = Settings::Manager::getInt("antialiasing", "Video");
        mFramebufferWidth = mSubImages[0].width;
        mFramebufferHeight = mSubImages[0].height;

        if (mSubImages[0].width != mSubImages[1].width || mSubImages[0].height != mSubImages[1].height)
            Log(Debug::Warning) << "Not implemented";

        mStereoFramebuffer.reset(new Misc::StereoFramebuffer(mFramebufferWidth, mFramebufferHeight, renderbufferSamples));
        mStereoFramebuffer->attachColorComponent(GL_RGBA);
        mStereoFramebuffer->attachDepthComponent(GL_DEPTH_COMPONENT);
        Misc::StereoView::instance().setStereoFramebuffer(mStereoFramebuffer);
        mViewer->getCamera()->setViewport(0, 0, mFramebufferWidth, mFramebufferHeight);

        // The msaa-resolve framebuffer needs a texture, so we can sample it while applying gamma.
        mMsaaResolveFramebuffer = new osg::FrameBufferObject();
        mMsaaResolveTexture = new osg::Texture2D();
        mMsaaResolveTexture->setTextureSize(mFramebufferWidth, mFramebufferHeight);
        mMsaaResolveTexture->setInternalFormat(GL_RGB);
        mMsaaResolveFramebuffer->setAttachment(osg::Camera::COLOR_BUFFER, osg::FrameBufferAttachment(mMsaaResolveTexture));
        mMsaaResolveFramebuffer->setAttachment(osg::Camera::DEPTH_BUFFER, osg::FrameBufferAttachment(new osg::RenderBuffer(mFramebufferWidth, mFramebufferHeight, GL_DEPTH_COMPONENT24, 0)));

        // The gamma resolve framebuffer will be used to write the result of gamma post-processing.
        mGammaResolveFramebuffer = new osg::FrameBufferObject();
        mGammaResolveFramebuffer->setAttachment(osg::Camera::COLOR_BUFFER, osg::FrameBufferAttachment(new osg::RenderBuffer(mFramebufferWidth, mFramebufferHeight, GL_RGB, 0)));

        // TODO: Needed?
        //SceneUtil::attachAlphaToCoverageFriendlyFramebufferToCamera(mViewer->getCamera(), osg::Camera::COLOR_BUFFER, colorBuffer);

        mViewer->setReleaseContextAtEndOfFrameHint(false);
        mViewer->getCamera()->getGraphicsContext()->setSwapCallback(mSwapBuffersCallback);
        mViewer->getCamera()->setViewport(0, 0, mFramebufferWidth, mFramebufferHeight);

        setupMirrorTexture();
    }

    Viewer::~Viewer(void)
    {
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
        Misc::StereoView::instance().setUpdateViewCallback(mUpdateViewCallback);
        Misc::CallbackManager::instance().addCallback(Misc::CallbackManager::DrawStage::Initial, mInitialDraw);
        Misc::CallbackManager::instance().addCallback(Misc::CallbackManager::DrawStage::PreDraw, mPreDraw);
        Misc::CallbackManager::instance().addCallback(Misc::CallbackManager::DrawStage::Final, mFinalDraw);

        mCallbacksConfigured = true;
    }

    void Viewer::setupMirrorTexture()
    {
        mMirrorTextureEnabled = Settings::Manager::getBool("mirror texture", "VR");
        mMirrorTextureEye = mirrorTextureEyeFromString(Settings::Manager::getString("mirror texture eye", "VR"));
        mFlipMirrorTextureOrder = Settings::Manager::getBool("flip mirror texture order", "VR");
        mMirrorTextureShouldBeCleanedUp = true;

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

    bool Viewer::applyGamma(osg::RenderInfo& info)
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

            stateset->setTextureAttributeAndModes(0, mMsaaResolveTexture, osg::StateAttribute::PROTECTED);
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
        auto colorImage = mColorSwapchain[view]->beginFrame(state->getGraphicsContext());
        auto depthImage = mDepthSwapchain[view]->beginFrame(state->getGraphicsContext());
        auto it = mSwapchainFramebuffers.find(colorImage);
        if (it == mSwapchainFramebuffers.end())
        {
            // Wrap subimage textures in osg framebuffer objects
            auto colorTexture = new osg::Texture2D();
            colorTexture->setTextureSize(mFramebufferWidth, mFramebufferHeight);
            colorTexture->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR);
            colorTexture->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
            auto colorTextureObject = new osg::Texture::TextureObject(colorTexture, colorImage, GL_TEXTURE_2D);
            colorTexture->setTextureObject(state->getContextID(), colorTextureObject);

            auto depthTexture = new osg::Texture2D();
            depthTexture->setTextureSize(mFramebufferWidth, mFramebufferHeight);
            depthTexture->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR);
            depthTexture->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
            auto depthTextureObject = new osg::Texture::TextureObject(depthTexture, depthImage, GL_TEXTURE_2D);
            depthTexture->setTextureObject(state->getContextID(), depthTextureObject);

            osg::ref_ptr<osg::FrameBufferObject> fbo = new osg::FrameBufferObject();
            fbo->setAttachment(osg::FrameBufferObject::BufferComponent::COLOR_BUFFER, osg::FrameBufferAttachment(colorTexture));
            fbo->setAttachment(osg::FrameBufferObject::BufferComponent::DEPTH_BUFFER, osg::FrameBufferAttachment(depthTexture));

            it = mSwapchainFramebuffers.emplace(colorImage, fbo).first;
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
        mMsaaResolveFramebuffer->apply(*state, osg::FrameBufferObject::READ_FRAMEBUFFER);
        gl->glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, GL_DEPTH_BUFFER_BIT, GL_NEAREST);

        mColorSwapchain[i]->endFrame(state->getGraphicsContext());
        mDepthSwapchain[i]->endFrame(state->getGraphicsContext());
        gl->glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
    }

    void Viewer::blitMirrorTexture(osg::State* state, int i)
    {
        auto* gl = osg::GLExtensions::Get(state->getContextID(), false);

        if (mMirrorTextureShouldBeCleanedUp)
        {
            // If mirror texture configuration has changed, just delete the existing mirror texture.
            if (mMirrorFramebuffer)
                mMirrorFramebuffer->releaseGLObjects(state);
            mMirrorFramebuffer = nullptr;
            mMirrorTextureShouldBeCleanedUp = false;
        }

        auto* traits = SDLUtil::GraphicsWindowSDL2::findContext(*mViewer)->getTraits();
        int screenWidth = traits->width;
        int screenHeight = traits->height;

        if (mMirrorTextureEnabled)
        {
            // Since OpenXR does not include native support for mirror textures, we have to generate them ourselves
            if (!mMirrorFramebuffer)
            {
                // Create the mirror texture framebuffer if it doesn't exist yet
                mMirrorFramebuffer = new osg::FrameBufferObject();
                mMirrorFramebuffer->setAttachment(osg::Camera::COLOR_BUFFER, osg::FrameBufferAttachment(new osg::RenderBuffer(screenWidth, screenHeight, GL_RGB, 0)));
            }

            // Compute the dimensions of each eye on the mirror texture.
            int dstWidth = screenWidth / mMirrorTextureViews.size();

            // Blit each eye
            // Which eye is blitted left/right is determined by which order left/right was added to mMirrorTextureViews
            int dstX = 0;
            mMirrorFramebuffer->apply(*state, osg::FrameBufferObject::DRAW_FRAMEBUFFER);
            mGammaResolveFramebuffer->apply(*state, osg::FrameBufferObject::READ_FRAMEBUFFER);
            for (auto viewId : mMirrorTextureViews)
            {
                if(viewId == static_cast<unsigned int>(i))
                    gl->glBlitFramebuffer(0, 0, mFramebufferWidth, mFramebufferHeight, dstX, 0, dstX + dstWidth, screenHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
                dstX += dstWidth;
            }

            // Blit mirror texture to back buffer / window
            gl->glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
            mMirrorFramebuffer->apply(*state, osg::FrameBufferObject::READ_FRAMEBUFFER);
            gl->glBlitFramebuffer(0, 0, screenWidth, screenHeight, 0, 0, screenWidth, screenHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        }
    }

    void Viewer::resolveMSAA(osg::State* state, osg::FrameBufferObject* fbo)
    {
        auto* gl = osg::GLExtensions::Get(state->getContextID(), false);
            
        // Resolve MSAA by blitting color to a non-multisampled framebuffer
        mMsaaResolveFramebuffer->apply(*state, osg::FrameBufferObject::DRAW_FRAMEBUFFER);
        fbo->apply(*state, osg::FrameBufferObject::READ_FRAMEBUFFER);
        gl->glBlitFramebuffer(0, 0, mFramebufferWidth, mFramebufferHeight, 0, 0, mFramebufferWidth, mFramebufferHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        gl->glBlitFramebuffer(0, 0, mFramebufferWidth, mFramebufferHeight, 0, 0, mFramebufferWidth, mFramebufferHeight, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    }

    void Viewer::resolveGamma(osg::RenderInfo& info)
    {
        auto* state = info.getState();
        auto* gl = osg::GLExtensions::Get(state->getContextID(), false);

        // Apply gamma by running a shader, sampling the colors we just blitted
        mGammaResolveFramebuffer->apply(*state, osg::FrameBufferObject::DRAW_FRAMEBUFFER);
        bool shouldDoGamma = Settings::Manager::getBool("gamma postprocessing", "VR Debug");
        if (!shouldDoGamma || !applyGamma(info))
        {
            mMsaaResolveFramebuffer->apply(*state, osg::FrameBufferObject::READ_FRAMEBUFFER);
            // Gamma should not / failed to be applied. Blit the colors unmodified
            gl->glBlitFramebuffer(0, 0, mFramebufferWidth, mFramebufferHeight, 0, 0, mFramebufferWidth, mFramebufferHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        }
    }

    void Viewer::blit(osg::RenderInfo& info)
    {
        auto* state = info.getState();
        auto* gl = osg::GLExtensions::Get(state->getContextID(), false);

        for (auto i = 0; i < 2; i++)
        {
            auto src = mStereoFramebuffer->unlayeredFbo(i);
            resolveMSAA(state, src);
            resolveGamma(info);
            blitMirrorTexture(state, i);
            blitXrFramebuffer(state, i);
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

        if (Misc::StereoView::instance().getTechnique() == Misc::StereoView::Technique::BruteForce)
        {
            osg::GraphicsOperation* graphicsOperation = info.getCurrentCamera()->getRenderer();
            osgViewer::Renderer* renderer = dynamic_cast<osgViewer::Renderer*>(graphicsOperation);
            if (renderer != nullptr)
            {
                // Disable normal OSG FBO camera setup
                renderer->setCameraRequiresSetUp(false);
                // OSG's internal stereo offers no option to let you control the viewport, and recomputes the render stage's viewport every time without exception.
                // So i have to override it in this callback so so that the viewport of each eye will encompass the entire texture.
                for (int i = 0; i < 2; i++)
                {
                    if (auto* viewport = renderer->getSceneView(i)->getRenderStage()->getViewport())
                        viewport->setViewport(0, 0, mFramebufferWidth, mFramebufferHeight);
                    if (auto* viewport = renderer->getSceneView(i)->getRenderStageLeft()->getViewport())
                        viewport->setViewport(0, 0, mFramebufferWidth, mFramebufferHeight);
                    if (auto* viewport = renderer->getSceneView(i)->getRenderStageRight()->getViewport())
                        viewport->setViewport(0, 0, mFramebufferWidth, mFramebufferHeight);
                }
            }
        }
    }

    void Viewer::preDrawCallback(osg::RenderInfo& info, Misc::CallbackManager::View view)
    {
        if (Misc::StereoView::instance().getTechnique() == Misc::StereoView::Technique::BruteForce)
        {
            mStereoFramebuffer->unlayeredFbo(static_cast<int>(view))->apply(*info.getState());
        }
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

    void Viewer::updateView(Misc::View& left, Misc::View& right)
    {
        auto& session = VR::Session::instance();

        auto frame = session.newFrame();
        session.frameBeginUpdate(frame);

        VR::TrackingManager::instance().updateTracking(frame);

        auto stageViews = VR::Session::instance().getPredictedViews(frame.predictedDisplayTime, VR::ReferenceSpace::Stage);
        auto views = VR::Session::instance().getPredictedViews(frame.predictedDisplayTime, VR::ReferenceSpace::View);

        if (frame.shouldRender)
        {
            left = views[VR::Side_Left];
            left.pose.position *= Constants::UnitsPerMeter * session.playerScale();
            right = views[VR::Side_Right];
            right.pose.position *= Constants::UnitsPerMeter * session.playerScale();

            std::shared_ptr<VR::ProjectionLayer> layer = std::make_shared<VR::ProjectionLayer>();
            for (uint32_t i = 0; i < 2; i++)
            {
                layer->views[i].colorSwapchain = mColorSwapchain[i];
                layer->views[i].depthSwapchain = mDepthSwapchain[i];
                layer->views[i].subImage.width = mColorSwapchain[i]->width();
                layer->views[i].subImage.height = mColorSwapchain[i]->height();
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
