#include "vrviewer.hpp"

#include "vrenvironment.hpp"
#include "vrframebuffer.hpp"
#include "vrinputmanager.hpp"
#include "openxrinput.hpp"

#include "../mwrender/vismask.hpp"

#include <osgViewer/Renderer>
#include <osg/StateAttribute>
#include <osg/BufferObject>
#include <osg/VertexArrayState>

#include <components/debug/gldebug.hpp>

#include <components/sceneutil/mwshadowtechnique.hpp>

#include <components/misc/stringops.hpp>
#include <components/misc/stereo.hpp>
#include <components/misc/callbackmanager.hpp>
#include <components/misc/constants.hpp>

#include <components/vr/layer.hpp>
#include <components/vr/session.hpp>
#include <components/vr/trackingmanager.hpp>
#include <components/xr/instance.hpp>
#include <components/xr/session.hpp>
#include <components/xr/swapchain.hpp>

#include <components/sdlutil/sdlgraphicswindow.hpp>

namespace MWVR
{
    // Callback to do construction with a graphics context
    class RealizeOperation : public osg::GraphicsOperation
    {
    public:
        RealizeOperation() : osg::GraphicsOperation("VRRealizeOperation", false) {};
        void operator()(osg::GraphicsContext* gc) override;
        bool realized();

    private:
    };

    VRViewer::VRViewer(
        osg::ref_ptr<osgViewer::Viewer> viewer)
        : mTrackingManager(std::make_unique<VR::TrackingManager>())
        , mViewer(viewer)
        , mPreDraw(new PredrawCallback(this))
        , mPostDraw(new PostdrawCallback(this))
        , mFinalDraw(new FinaldrawCallback(this))
        , mUpdateViewCallback(new UpdateViewCallback(this))
        , mMsaaResolveTexture{}
        , mMirrorTexture{ nullptr }
        , mOpenXRConfigured(false)
        , mCallbacksConfigured(false)
    {
        mViewer->setRealizeOperation(new RealizeOperation());
    }

    VRViewer::~VRViewer(void)
    {
    }

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

    static VRViewer::MirrorTextureEye mirrorTextureEyeFromString(const std::string& str)
    {
        if (Misc::StringUtils::ciEqual(str, "left"))
            return VRViewer::MirrorTextureEye::Left;
        if (Misc::StringUtils::ciEqual(str, "right"))
            return VRViewer::MirrorTextureEye::Right;
        if (Misc::StringUtils::ciEqual(str, "both"))
            return VRViewer::MirrorTextureEye::Both;
        return VRViewer::MirrorTextureEye::Both;
    }

    void VRViewer::configureXR(osg::GraphicsContext* gc)
    {
        std::unique_lock<std::mutex> lock(mMutex);

        if (mOpenXRConfigured)
        {
            return;
        }

        mXrInstance = std::make_unique<XR::Instance>(gc);

        // Set up swapchain config
        auto swapchainConfigs = XR::Instance::instance().getRecommendedSwapchainConfig();

        std::array<std::string, 2> xConfString;
        std::array<std::string, 2> yConfString;
        xConfString[0] = Settings::Manager::getString("left eye resolution x", "VR");
        yConfString[0] = Settings::Manager::getString("left eye resolution y", "VR");

        xConfString[1] = Settings::Manager::getString("right eye resolution x", "VR");
        yConfString[1] = Settings::Manager::getString("right eye resolution y", "VR");

        std::array<const char*, 2> viewNames = {
                "LeftEye",
                "RightEye"
        };
        for (unsigned i = 0; i < viewNames.size(); i++)
        {
            auto name = viewNames[i];

            auto width = parseResolution(xConfString[i], swapchainConfigs[i].recommendedWidth, swapchainConfigs[i].maxWidth);
            auto height = parseResolution(yConfString[i], swapchainConfigs[i].recommendedHeight, swapchainConfigs[i].maxHeight);

            auto samples =
                std::max(1, // OpenXR requires a non-zero value
                    std::min(swapchainConfigs[i].maxSamples,
                        Settings::Manager::getInt("antialiasing", "Video")
                    )
                );

            Log(Debug::Verbose) << name << " resolution: Recommended x=" << swapchainConfigs[i].recommendedWidth << ", y=" << swapchainConfigs[i].recommendedHeight;
            Log(Debug::Verbose) << name << " resolution: Max x=" << swapchainConfigs[i].maxWidth << ", y=" << swapchainConfigs[i].maxHeight;
            Log(Debug::Verbose) << name << " resolution: Selected x=" << width << ", y=" << height;

            //swapchainConfigs[i].name = name;
            //if (i > 0)
                //swapchainConfigs[i].offsetWidth = width + mSwapchainConfig[i].offsetWidth;
            //XrSwapchain xrColorSwapchain = ;
            //XrSwapchain xrDepthSwapchain = xr->impl().createXrSwapchain(mSwapchainConfig[i], OpenXRManagerImpl::SwapchainUse::Depth);
            
            mColorSwapchain[i].reset(XR::Instance::instance().platform().createSwapchain(width, height, samples, XR::Platform::SwapchainUse::Color, viewNames[i]));
            mDepthSwapchain[i].reset(XR::Instance::instance().platform().createSwapchain(width, height, samples, XR::Platform::SwapchainUse::Depth, viewNames[i]));
            mSubImages[i].width = width;
            mSubImages[i].height = height;
            mSubImages[i].x = mSubImages[i].y = 0;
            //mSubImages[i].colorSwapchain = mColorSwapchain[i].get();
            //mSubImages[i].depthSwapchain = mDepthSwapchain[i].get();
        }

        int width = mSubImages[0].width + mSubImages[1].width;
        int height = std::max(mSubImages[0].height, mSubImages[1].height);
        int samples = std::max(mColorSwapchain[0]->samples(), mColorSwapchain[1]->samples());

        mFramebuffer.reset(new VRFramebuffer(gc->getState(), width, height, samples));
        mFramebuffer->createColorBuffer(gc);
        mFramebuffer->createDepthBuffer(gc);
        mMsaaResolveTexture.reset(new VRFramebuffer(gc->getState(), width, height, 0));
        mMsaaResolveTexture->createColorBuffer(gc);
        mGammaResolveTexture.reset(new VRFramebuffer(gc->getState(), width, height, 0));
        mGammaResolveTexture->createColorBuffer(gc);
        mGammaResolveTexture->createDepthBuffer(gc);

        mViewer->setReleaseContextAtEndOfFrameHint(false);
        mViewer->getCamera()->getGraphicsContext()->setSwapCallback(new VRViewer::SwapBuffersCallback(this));

        setupMirrorTexture();
        Log(Debug::Verbose) << "XR configured";
        mOpenXRConfigured = true;
    }

    void VRViewer::configureCallbacks()
    {
        if (mCallbacksConfigured)
            return;

        // Give the main camera an initial draw callback that disables camera setup (we don't want it)
        Misc::StereoView::instance().setUpdateViewCallback(mUpdateViewCallback);
        Misc::CallbackManager::instance().addCallback(Misc::CallbackManager::DrawStage::Initial, new InitialDrawCallback(this));
        Misc::CallbackManager::instance().addCallback(Misc::CallbackManager::DrawStage::PreDraw, mPreDraw);
        Misc::CallbackManager::instance().addCallback(Misc::CallbackManager::DrawStage::PostDraw, mPostDraw);
        Misc::CallbackManager::instance().addCallback(Misc::CallbackManager::DrawStage::Final, mFinalDraw);
        auto cullMask = ~(MWRender::VisMask::Mask_UpdateVisitor | MWRender::VisMask::Mask_SimpleWater);
        cullMask &= ~MWRender::VisMask::Mask_GUI;
        cullMask |= MWRender::VisMask::Mask_3DGUI;
        Misc::StereoView::instance().setCullMask(cullMask);

        mCallbacksConfigured = true;
    }

    void VRViewer::setupMirrorTexture()
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

    void VRViewer::processChangedSettings(const std::set<std::pair<std::string, std::string>>& changed)
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

    //SubImage VRViewer::subImage(VR::Side side)
    //{
    //    return mSubImages[static_cast<int>(side)];
    //}

    static GLuint createShader(osg::GLExtensions* gl, const char* source, GLenum type)
    {
        GLint len = strlen(source);
        GLuint shader = gl->glCreateShader(type);
        gl->glShaderSource(shader, 1, &source, &len);
        gl->glCompileShader(shader);
        GLint isCompiled = 0;
        gl->glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
        if (isCompiled == GL_FALSE)
        {
            GLint maxLength = 0;
            gl->glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);
            std::vector<GLchar> infoLog(maxLength);
            gl->glGetShaderInfoLog(shader, maxLength, &maxLength, &infoLog[0]);
            gl->glDeleteShader(shader);

            Log(Debug::Error) << "Failed to compile shader: " << infoLog.data();

            return 0;
        }
        return shader;
    }

    static bool applyGamma(osg::RenderInfo& info, VRFramebuffer& target, VRFramebuffer& source)
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
        static osg::ref_ptr<osg::Texture2D> texture = nullptr;
        static osg::ref_ptr<osg::Texture::TextureObject> textureObject = nullptr;

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

            texture = new osg::Texture2D();
            texture->setName("diffuseMap");
            textureObject = new osg::Texture::TextureObject(texture, source.colorBuffer(), GL_TEXTURE_2D);
            texture->setTextureObject(state->getContextID(), textureObject);
            stateset->setTextureAttributeAndModes(0, texture, osg::StateAttribute::PROTECTED);
            stateset->setTextureMode(0, GL_TEXTURE_2D, osg::StateAttribute::PROTECTED);

            gammaUniform = new osg::Uniform("gamma", Settings::Manager::getFloat("gamma", "Video"));
            contrastUniform = new osg::Uniform("contrast", Settings::Manager::getFloat("contrast", "Video"));
            stateset->addUniform(gammaUniform);
            stateset->addUniform(contrastUniform);

            geometry->compileGLObjects(info);

            first = false;
        }

        target.bindFramebuffer(state->getGraphicsContext(), GL_FRAMEBUFFER_EXT);

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

            if(!viewport)
                viewport = new osg::Viewport(0, 0, target.width(), target.height());
            viewport->setViewport(0, 0, target.width(), target.height());
            viewport->apply(*state);

            glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
            geometry->draw(info);

            state->popStateSet();
            return true;
        }
        return false;
    }

    void VRViewer::blit(osg::RenderInfo& info)
    {
        if (mMirrorTextureShouldBeCleanedUp)
        {
            mMirrorTexture = nullptr;
            mMirrorTextureShouldBeCleanedUp = false;
        }

        auto* state = info.getState();
        auto* gc = state->getGraphicsContext();
        auto* gl = osg::GLExtensions::Get(state->getContextID(), false);

        auto* traits = SDLUtil::GraphicsWindowSDL2::findContext(*mViewer)->getTraits();
        int screenWidth = traits->width;
        int screenHeight = traits->height;

        mMsaaResolveTexture->bindFramebuffer(gc, GL_FRAMEBUFFER_EXT);

        mFramebuffer->blit(gc, 0, 0, mFramebuffer->width(), mFramebuffer->height(), 0, 0, mMsaaResolveTexture->width(), mMsaaResolveTexture->height(), GL_COLOR_BUFFER_BIT, GL_NEAREST);

        bool shouldDoGamma = Settings::Manager::getBool("gamma postprocessing", "VR Debug");
        if (!shouldDoGamma || !applyGamma(info, *mGammaResolveTexture, *mMsaaResolveTexture))
        {
            mGammaResolveTexture->bindFramebuffer(gc, GL_FRAMEBUFFER_EXT);
            mMsaaResolveTexture->blit(gc, 0, 0, mMsaaResolveTexture->width(), mMsaaResolveTexture->height(), 0, 0, mGammaResolveTexture->width(), mGammaResolveTexture->height(), GL_COLOR_BUFFER_BIT, GL_NEAREST);
        }

        mGammaResolveTexture->bindFramebuffer(gc, GL_FRAMEBUFFER_EXT);
        mFramebuffer->blit(gc, 0, 0, mFramebuffer->width(), mFramebuffer->height(), 0, 0, mGammaResolveTexture->width(), mGammaResolveTexture->height(), GL_DEPTH_BUFFER_BIT, GL_NEAREST);

        //// Since OpenXR does not include native support for mirror textures, we have to generate them ourselves
        if (mMirrorTextureEnabled)
        {
            if (!mMirrorTexture)
            {
                mMirrorTexture.reset(new VRFramebuffer(state,
                    screenWidth,
                    screenHeight,
                    0));
                mMirrorTexture->createColorBuffer(gc);
            }

            int dstWidth = screenWidth / mMirrorTextureViews.size();
            int srcWidth = mGammaResolveTexture->width() / 2;
            int dstX = 0;
            mMirrorTexture->bindFramebuffer(gc, GL_FRAMEBUFFER_EXT);
            for (auto viewId : mMirrorTextureViews)
            {
                int srcX = static_cast<int>(viewId) * srcWidth;
                mGammaResolveTexture->blit(gc, srcX, 0, srcX + srcWidth, mGammaResolveTexture->height(), dstX, 0, dstX + dstWidth, screenHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
                dstX += dstWidth;
            }

            gl->glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
            mMirrorTexture->blit(gc, 0, 0, screenWidth, screenHeight, 0, 0, screenWidth, screenHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        }

        uint32_t x0 = 0;
        for (unsigned i = 0; i < 2; i++)
        {
            auto colorImage = mColorSwapchain[i]->beginFrame(gc);
            auto depthImage = mDepthSwapchain[i]->beginFrame(gc);
            auto x = mSubImages[i].x + x0;
            auto y = mSubImages[i].y;
            auto w = mSubImages[i].width;
            auto h = mSubImages[i].height;
            //blit(state, mGammaResolveTexture.get(), colorImage, x, y, w, h, GL_COLOR_BUFFER_BIT, mColorSwapchain[i]->mustFlipVertical());
            //blit(state, mGammaResolveTexture.get(), depthImage, x, y, w, h, GL_DEPTH_BUFFER_BIT, mDepthSwapchain[i]->mustFlipVertical());

            auto it = mSwapchainFramebuffers.find(colorImage);
            if (it == mSwapchainFramebuffers.end())
            {
                mSwapchainFramebuffers[colorImage] = std::make_unique<VRFramebuffer>(state, w, h, 0);
                it = mSwapchainFramebuffers.find(colorImage);
                it->second->setColorBuffer(gc, colorImage, false);
                it->second->setDepthBuffer(gc, depthImage, false);
            }
            it->second->bindFramebuffer(gc, GL_FRAMEBUFFER_EXT);
            uint32_t y0 = 0;
            uint32_t y1 = h;
            if (mColorSwapchain[i]->mustFlipVertical())
                std::swap(y0, y1);
            mGammaResolveTexture->blit(gc, x, y, x + w, y + h, 0, y0, w, y1, GL_COLOR_BUFFER_BIT, GL_NEAREST);
            gl->glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);

            x0 += w;
            mColorSwapchain[i]->endFrame(gc);
            mDepthSwapchain[i]->endFrame(gc);
        }

        gl->glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
    }

    void VRViewer::blit(osg::State* state, VRFramebuffer* src, uint32_t target, uint32_t src_x, uint32_t src_y, uint32_t w, uint32_t h, uint32_t bits, bool flipVertical)
    {
        auto gc = state->getGraphicsContext();

        auto it = mSwapchainFramebuffers.find(target);
        if (it == mSwapchainFramebuffers.end())
        {
            mSwapchainFramebuffers[target] = std::make_unique<VRFramebuffer>(state, w, h, 0);
            it = mSwapchainFramebuffers.find(target);
            it->second->setColorBuffer(gc, target, false);
        }

        it->second->bindFramebuffer(gc, GL_FRAMEBUFFER_EXT);
        uint32_t y0 = 0;
        uint32_t y1 = h;
        if(flipVertical)
            std::swap(y0, y1);
        //src->blit(gc, src_x, src_y, src_x + w, src_y + h, 0, y0, w, y1, bits, GL_NEAREST);
    }

    void
        VRViewer::SwapBuffersCallback::swapBuffersImplementation(
            osg::GraphicsContext* gc)
    {
        mViewer->swapBuffersCallback(gc);
    }

    void
        RealizeOperation::operator()(
            osg::GraphicsContext* gc)
    {
        if (Debug::shouldDebugOpenGL())
            Debug::EnableGLDebugOperation()(gc);

        Environment::get().getViewer()->configureXR(gc);
    }

    bool
        RealizeOperation::realized()
    {
        return Environment::get().getViewer()->xrConfigured();
    }

    void VRViewer::initialDrawCallback(osg::RenderInfo& info)
    {
        if (mRenderingReady)
            return;

        {
            std::scoped_lock lock(mMutex);
            mDrawFrame = mReadyFrames.front();
            mReadyFrames.pop();
        }

        VR::Session::instance().frameBeginRender(mDrawFrame);

        mViewer->getCamera()->setViewport(0, 0, mFramebuffer->width(), mFramebuffer->height());

        osg::GraphicsOperation* graphicsOperation = info.getCurrentCamera()->getRenderer();
        osgViewer::Renderer* renderer = dynamic_cast<osgViewer::Renderer*>(graphicsOperation);
        if (renderer != nullptr)
        {
            // Disable normal OSG FBO camera setup
            renderer->setCameraRequiresSetUp(false);
        }

        mRenderingReady = true;
    }

    void VRViewer::preDrawCallback(osg::RenderInfo& info)
    {
        if(mDrawFrame.shouldRender)
        {
            mFramebuffer->bindFramebuffer(info.getState()->getGraphicsContext(), GL_FRAMEBUFFER_EXT);
        }
    }

    void VRViewer::postDrawCallback(osg::RenderInfo& info)
    {
    }

    void VRViewer::finalDrawCallback(osg::RenderInfo& info)
    {
        if(mDrawFrame.shouldRender)
        {
            blit(info);
        }
    }

    void VRViewer::swapBuffersCallback(osg::GraphicsContext* gc)
    {
        mRenderingReady = false;

        VR::Session::instance().frameEnd(gc, mDrawFrame);
    }

    void VRViewer::updateView(Misc::View& left, Misc::View& right)
    {
        auto& session = VR::Session::instance();

        auto frame = session.newFrame();
        session.frameBeginUpdate(frame);

        // TODO:
        if(frame.shouldSyncInput)
            Environment::get().getInputManager()->xrInput().getActionSet(MWActionSet::Tracking).updateControls();
        VR::TrackingManager::instance().updateTracking(frame);

        auto stageViews = XR::Session::instance().getPredictedViews(frame.predictedDisplayTime, VR::ReferenceSpace::Stage);
        auto views = XR::Session::instance().getPredictedViews(frame.predictedDisplayTime, VR::ReferenceSpace::View);

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

    void VRViewer::UpdateViewCallback::updateView(Misc::View& left, Misc::View& right)
    {
        mViewer->updateView(left, right);
    }

    void VRViewer::FinaldrawCallback::operator()(osg::RenderInfo& info, Misc::StereoView::StereoDrawCallback::View view) const
    {
        if (view != Misc::StereoView::StereoDrawCallback::View::Left)
            mViewer->finalDrawCallback(info);
    }
}
