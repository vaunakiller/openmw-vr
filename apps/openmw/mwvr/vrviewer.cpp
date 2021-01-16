#include "vrviewer.hpp"

#include "openxrmanagerimpl.hpp"
#include "openxrswapchain.hpp"
#include "vrenvironment.hpp"
#include "vrsession.hpp"
#include "vrframebuffer.hpp"
#include "vrview.hpp"

#include "../mwrender/vismask.hpp"

#include <osgViewer/Renderer>

#include <components/debug/gldebug.hpp>

#include <components/sceneutil/mwshadowtechnique.hpp>

#include <components/misc/stringops.hpp>
#include <components/misc/stereo.hpp>

#include <components/sdlutil/sdlgraphicswindow.hpp>

namespace MWVR
{

    const std::array<const char*, 2> VRViewer::sViewNames = {
            "LeftEye",
            "RightEye"
    };

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
        : mViewer(viewer)
        , mPreDraw(new PredrawCallback(this))
        , mPostDraw(new PostdrawCallback(this))
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

    void VRViewer::traversals()
    {
        mViewer->updateTraversal();
        mViewer->renderingTraversals();
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


        auto* xr = Environment::get().getManager();
        xr->realize(gc);

        // Run through initial events to start session
        // For the rest of runtime this is handled by vrsession
        xr->handleEvents();

        // Set up swapchain config
        mSwapchainConfig = xr->getRecommendedSwapchainConfig();

        std::array<std::string, 2> xConfString;
        std::array<std::string, 2> yConfString;
        xConfString[0] = Settings::Manager::getString("left eye resolution x", "VR");
        yConfString[0] = Settings::Manager::getString("left eye resolution y", "VR");

        xConfString[1] = Settings::Manager::getString("right eye resolution x", "VR");
        yConfString[1] = Settings::Manager::getString("right eye resolution y", "VR");

        for (unsigned i = 0; i < sViewNames.size(); i++)
        {
            auto name = sViewNames[i];

            mSwapchainConfig[i].selectedWidth = parseResolution(xConfString[i], mSwapchainConfig[i].recommendedWidth, mSwapchainConfig[i].maxWidth);
            mSwapchainConfig[i].selectedHeight = parseResolution(yConfString[i], mSwapchainConfig[i].recommendedHeight, mSwapchainConfig[i].maxHeight);

            mSwapchainConfig[i].selectedSamples = 
                std::max(1, // OpenXR requires a non-zero value
                    std::min(mSwapchainConfig[i].maxSamples, 
                        Settings::Manager::getInt("antialiasing", "Video")
                    )
                );
            
            Log(Debug::Verbose) << name << " resolution: Recommended x=" << mSwapchainConfig[i].recommendedWidth << ", y=" << mSwapchainConfig[i].recommendedHeight;
            Log(Debug::Verbose) << name << " resolution: Max x=" << mSwapchainConfig[i].maxWidth << ", y=" << mSwapchainConfig[i].maxHeight;
            Log(Debug::Verbose) << name << " resolution: Selected x=" << mSwapchainConfig[i].selectedWidth << ", y=" << mSwapchainConfig[i].selectedHeight;

            mSwapchainConfig[i].name = sViewNames[i];
            if (i > 0)
                mSwapchainConfig[i].offsetWidth = mSwapchainConfig[i].selectedWidth + mSwapchainConfig[i].offsetWidth;

            mSwapchain[i].reset(new OpenXRSwapchain(gc->getState(), mSwapchainConfig[i]));
            mSubImages[i].width = mSwapchainConfig[i].selectedWidth;
            mSubImages[i].height = mSwapchainConfig[i].selectedHeight;
            mSubImages[i].x = mSubImages[i].y = 0;
            mSubImages[i].swapchain = mSwapchain[i].get();
        }

        int width = mSubImages[0].width + mSubImages[1].width;
        int height = std::max(mSubImages[0].height, mSubImages[1].height);
        int samples = std::max(mSwapchainConfig[0].selectedSamples, mSwapchainConfig[1].selectedSamples);

        mFramebuffer.reset(new VRFramebuffer(gc->getState(),width,height, samples));
        mFramebuffer->createColorBuffer(gc);
        mFramebuffer->createDepthBuffer(gc);
        mMsaaResolveTexture.reset(new VRFramebuffer(gc->getState(),width,height,0));
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
        Misc::StereoView::instance().setInitialDrawCallback(new InitialDrawCallback(this));
        Misc::StereoView::instance().setPredrawCallback(mPreDraw);
        Misc::StereoView::instance().setPostdrawCallback(mPostDraw);
        //auto cullMask = Misc::StereoView::instance().getCullMask();
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
            mMirrorTextureViews.push_back(sViewNames[(int)Side::LEFT_SIDE]);
        if (mMirrorTextureEye == MirrorTextureEye::Right || mMirrorTextureEye == MirrorTextureEye::Both)
            mMirrorTextureViews.push_back(sViewNames[(int)Side::RIGHT_SIDE]);
        if (mFlipMirrorTextureOrder)
            std::reverse(mMirrorTextureViews.begin(), mMirrorTextureViews.end());
        // TODO: If mirror is false either hide the window or paste something meaningful into it.
        // E.g. Fanart of Dagoth UR wearing a VR headset
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

    SubImage VRViewer::subImage(Side side)
    {
        return mSubImages[static_cast<int>(side)];
    }

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

    static bool applyGamma(osg::GraphicsContext* gc, VRFramebuffer& target, VRFramebuffer& source)
    {
        // TODO: Temporary solution for applying gamma and contrast modifications
        // When OpenMW implements post processing, this will be performed there instead.
        // I'm just throwing things into static locals since this is temporary code that will be trashed later.
        static bool first = true;
        static GLuint vShader = 0;
        static GLuint fShader = 0;
        static GLuint program = 0;
        static GLuint vbo = 0;
        static GLuint gammaUniform = 0;
        static GLuint contrastUniform = 0;

        auto* state = gc->getState();
        auto* gl = osg::GLExtensions::Get(state->getContextID(), false);

        if (first)
        {
            first = false;

            const char* vSource = "#version 120\n varying vec2 uv; void main(){ gl_Position = vec4(gl_Vertex.xy*2.0 - 1, 0, 1); uv = gl_Vertex.xy;}";
            const char* fSource = "#version 120\n varying vec2 uv; uniform sampler2D t; uniform float gamma; uniform float contrast;"
                "void main() {"
                "vec4 color1 = texture2D(t, uv);"
                "vec3 rgb = color1.rgb;"
                "rgb = (rgb - 0.5f) * contrast + 0.5f;"
                "rgb = pow(rgb, vec3(1.0/gamma));"
                "gl_FragColor = vec4(rgb, color1.a);"
                "}";

            vShader = createShader(gl, vSource, GL_VERTEX_SHADER);
            fShader = createShader(gl, fSource, GL_FRAGMENT_SHADER);

            program = gl->glCreateProgram();
            gl->glAttachShader(program, vShader);
            gl->glAttachShader(program, fShader);
            gl->glLinkProgram(program);

            GLint isCompiled = 0;
            gl->glGetProgramiv(program, GL_LINK_STATUS, &isCompiled);
            if (isCompiled == GL_FALSE)
            {
                GLint maxLength = 0;
                gl->glGetProgramInfoLog(program, 0, &maxLength, nullptr);
                std::vector<GLchar> infoLog(maxLength);
                gl->glGetProgramInfoLog(program, maxLength, &maxLength, &infoLog[0]);
                gl->glDeleteProgram(program);
                program = 0;
                Log(Debug::Error) << "Failed to link program: " << infoLog.data();
            }

            if (program)
            {
                GLfloat vertices[] =
                {
                    0, 0, 0, 0,
                    1, 0, 0, 0,
                    1, 1, 0, 0,
                    0, 0, 0, 0,
                    1, 1, 0, 0,
                    0, 1, 0, 0
                };

                gl->glGenBuffers(1, &vbo);
                gl->glBindBuffer(GL_ARRAY_BUFFER_ARB, vbo);
                gl->glBufferData(GL_ARRAY_BUFFER_ARB, sizeof(vertices), vertices, GL_STATIC_DRAW);

                gammaUniform = gl->glGetUniformLocation(program, "gamma");
                contrastUniform = gl->glGetUniformLocation(program, "contrast");
            }
        }

        target.bindFramebuffer(gc, GL_FRAMEBUFFER_EXT);

        if (program > 0)
        {
            gl->glUseProgram(program);
            gl->glBindVertexArray(0);
            glViewport(0, 0, target.width(), target.height());

            gl->glUniform1f(gammaUniform, Settings::Manager::getFloat("gamma", "Video"));
            gl->glUniform1f(contrastUniform, Settings::Manager::getFloat("contrast", "Video"));

            gl->glBindBuffer(GL_ARRAY_BUFFER_ARB, vbo);
            glVertexPointer(4, GL_FLOAT, 0, 0);
            gl->glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, source.colorBuffer());
            glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
            glDisable(GL_BLEND);

            glDrawArrays(GL_TRIANGLES, 0, 6);

            gl->glUseProgram(0);
            glBindTexture(GL_TEXTURE_2D, 0);
            glEnable(GL_BLEND);
            gl->glBindBuffer(GL_ARRAY_BUFFER_ARB, 0);
            return true;
        }
        return false;
    }

    void VRViewer::blit(osg::GraphicsContext* gc)
    {
        if (mMirrorTextureShouldBeCleanedUp)
        {
            mMirrorTexture = nullptr;
            mMirrorTextureShouldBeCleanedUp = false;
        }
        if (!mMirrorTextureEnabled)
            return;

        auto* traits = SDLUtil::GraphicsWindowSDL2::findContext(*mViewer)->getTraits();
        int screenWidth = traits->width;
        int screenHeight = traits->height;
        if (!mMirrorTexture)
        {
            ;
            mMirrorTexture.reset(new VRFramebuffer(gc->getState(), 
                screenWidth,
                screenHeight,
                0));
            mMirrorTexture->createColorBuffer(gc);
        }

        auto* state = gc->getState();
        auto* gl = osg::GLExtensions::Get(state->getContextID(), false);

        int mirrorWidth = screenWidth / mMirrorTextureViews.size();

        //// Since OpenXR does not include native support for mirror textures, we have to generate them ourselves
        //// which means resolving msaa twice.
        mMsaaResolveTexture->bindFramebuffer(gc, GL_FRAMEBUFFER_EXT);

        mFramebuffer->blit(gc, 0, 0, mFramebuffer->width(), mFramebuffer->height(), 0, 0, mMsaaResolveTexture->width(), mMsaaResolveTexture->height(), GL_COLOR_BUFFER_BIT, GL_NEAREST);

        if(!applyGamma(gc, *mGammaResolveTexture, *mMsaaResolveTexture))
            mMsaaResolveTexture->blit(gc, 0, 0, mMsaaResolveTexture->width(), mMsaaResolveTexture->height(), 0, 0, mGammaResolveTexture->width(), mGammaResolveTexture->height(), GL_COLOR_BUFFER_BIT, GL_NEAREST);

        mFramebuffer->blit(gc, 0, 0, mFramebuffer->width(), mFramebuffer->height(), 0, 0, mGammaResolveTexture->width(), mGammaResolveTexture->height(), GL_DEPTH_BUFFER_BIT, GL_NEAREST);

        if (mMirrorTexture)
        {
            mMirrorTexture->bindFramebuffer(gc, GL_FRAMEBUFFER_EXT);
            mMsaaResolveTexture->blit(gc, 0, 0, mMsaaResolveTexture->width(), mMsaaResolveTexture->height(), 0, 0, screenWidth, screenHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
            gl->glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
            mMirrorTexture->blit(gc, 0, 0, screenWidth, screenHeight, 0, 0, screenWidth, screenHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        }

        mSwapchain[0]->endFrame(gc, *mGammaResolveTexture);
        mSwapchain[1]->endFrame(gc, *mGammaResolveTexture);
        gl->glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
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

        Environment::get().getSession()->beginPhase(VRSession::FramePhase::Draw);
        if (Environment::get().getSession()->getFrame(VRSession::FramePhase::Draw)->mShouldRender)
        {
            mSwapchain[0]->beginFrame(info.getState()->getGraphicsContext());
            mSwapchain[1]->beginFrame(info.getState()->getGraphicsContext());
        }
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
        if (Environment::get().getSession()->getFrame(VRSession::FramePhase::Draw)->mShouldRender)
        {
            mFramebuffer->bindFramebuffer(info.getState()->getGraphicsContext(), GL_FRAMEBUFFER_EXT);
            //mSwapchain->framebuffer()->bindFramebuffer(info.getState()->getGraphicsContext(), GL_FRAMEBUFFER_EXT);
        }
    }

    void VRViewer::postDrawCallback(osg::RenderInfo& info)
    {
        auto* camera = info.getCurrentCamera();
        auto name = camera->getName();

        // This happens sometimes, i've not been able to catch it when as happens
        // to see why and how i can stop it.
        if (camera->getPreDrawCallback() != mPreDraw)
        {
            camera->setPreDrawCallback(mPreDraw);
            Log(Debug::Warning) << ("osg overwrote predraw");
        }
    }

    void VRViewer::swapBuffersCallback(osg::GraphicsContext* gc)
    {
        auto* session = Environment::get().getSession();
        session->swapBuffers(gc, *this);
        mRenderingReady = false;
    }

    void VRViewer::updateView(Misc::View& left, Misc::View& right)
    {
        auto phase = VRSession::FramePhase::Update;
        auto session = Environment::get().getSession();
        auto& frame = session->getFrame(phase);

        if (frame->mShouldRender)
        {
            left = frame->mPredictedPoses.view[static_cast<int>(Side::LEFT_SIDE)];
            right = frame->mPredictedPoses.view[static_cast<int>(Side::RIGHT_SIDE)];
        }
    }

    void VRViewer::UpdateViewCallback::updateView(Misc::View& left, Misc::View& right)
    {
        mViewer->updateView(left, right);
    }
}
