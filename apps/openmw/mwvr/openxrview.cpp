#include "openxrview.hpp"
#include "openxrmanager.hpp"
#include "openxrmanagerimpl.hpp"
#include "../mwinput/inputmanagerimp.hpp"

#include <components/debug/debuglog.hpp>
#include <components/sdlutil/sdlgraphicswindow.hpp>

#include <Windows.h>

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <openxr/openxr_platform_defines.h>
#include <openxr/openxr_reflection.h>


#include <osg/Camera>
#include <osgViewer/Renderer>

#include <vector>
#include <array>
#include <iostream>

namespace osg {
    Quat fromXR(XrQuaternionf quat)
    {
        return Quat{ quat.x, quat.y, quat.z, quat.w };
    }
}

namespace MWVR
{
    class OpenXRViewImpl
    {
    public:
        enum SubView
        {
            LEFT_VIEW = 0,
            RIGHT_VIEW = 1,
            SUBVIEW_MAX = RIGHT_VIEW, //!< Used to size subview arrays. Not a valid input.
        };

        OpenXRViewImpl(osg::ref_ptr<OpenXRManager> XR, osg::ref_ptr<osg::State> state, float metersPerUnit, unsigned int viewIndex);
        ~OpenXRViewImpl();

        osg::ref_ptr<OpenXRTextureBuffer> prepareNextSwapchainImage();
        void   releaseSwapchainImage();
        void prerenderCallback(osg::RenderInfo& renderInfo);
        void postrenderCallback(osg::RenderInfo& renderInfo);
        osg::Camera* createCamera(int eye, const osg::Vec4& clearColor, osg::GraphicsContext* gc);
        osg::Matrix projectionMatrix();
        osg::Matrix viewMatrix();

        osg::ref_ptr<OpenXRManager> mXR;
        XrSwapchain mSwapchain = XR_NULL_HANDLE;
        std::vector<XrSwapchainImageOpenGLKHR> mSwapchainImageBuffers{};
        std::vector<osg::ref_ptr<OpenXRTextureBuffer> > mTextureBuffers{};
        int32_t mWidth = -1;
        int32_t mHeight = -1;
        int64_t mSwapchainColorFormat = -1;
        XrViewConfigurationView mConfig{ XR_TYPE_VIEW_CONFIGURATION_VIEW };
        XrSwapchainSubImage mSubImage{};
        OpenXRTextureBuffer* mCurrentBuffer = nullptr;
        float mMetersPerUnit = 1.f;
        int mViewIndex = -1;
    };

    OpenXRViewImpl::OpenXRViewImpl(
        osg::ref_ptr<OpenXRManager> XR, 
        osg::ref_ptr<osg::State> state,
        float metersPerUnit,
        unsigned int viewIndex)
        : mXR(XR)
        , mConfig()
        , mMetersPerUnit(metersPerUnit)
        , mViewIndex(viewIndex)
    {
        auto& pXR = *(mXR->mPrivate);
        if (viewIndex >= pXR.mConfigViews.size())
            throw std::logic_error("viewIndex out of range");
        mConfig = pXR.mConfigViews[viewIndex];

        // Select a swapchain format.
        uint32_t swapchainFormatCount;
        CHECK_XRCMD(xrEnumerateSwapchainFormats(pXR.mSession, 0, &swapchainFormatCount, nullptr));
        std::vector<int64_t> swapchainFormats(swapchainFormatCount);
        CHECK_XRCMD(xrEnumerateSwapchainFormats(pXR.mSession, (uint32_t)swapchainFormats.size(), &swapchainFormatCount, swapchainFormats.data()));

        // List of supported color swapchain formats.
        constexpr int64_t SupportedColorSwapchainFormats[] = {
            GL_RGBA8,
            GL_RGBA8_SNORM,
        };

        auto swapchainFormatIt =
            std::find_first_of(swapchainFormats.begin(), swapchainFormats.end(), std::begin(SupportedColorSwapchainFormats),
                std::end(SupportedColorSwapchainFormats));
        if (swapchainFormatIt == swapchainFormats.end()) {
            Log(Debug::Error) << "No swapchain format supported at runtime";
        }

        mSwapchainColorFormat = *swapchainFormatIt;

        Log(Debug::Verbose) << "Creating swapchain with dimensions Width=" << mConfig.recommendedImageRectWidth << " Heigh=" << mConfig.recommendedImageRectHeight << " SampleCount=" << mConfig.recommendedSwapchainSampleCount;

        // Create the swapchain.
        XrSwapchainCreateInfo swapchainCreateInfo{ XR_TYPE_SWAPCHAIN_CREATE_INFO };
        swapchainCreateInfo.arraySize = 1;
        swapchainCreateInfo.format = mSwapchainColorFormat;
        swapchainCreateInfo.width = mConfig.recommendedImageRectWidth;
        swapchainCreateInfo.height = mConfig.recommendedImageRectHeight;
        swapchainCreateInfo.mipCount = 1;
        swapchainCreateInfo.faceCount = 1;
        swapchainCreateInfo.sampleCount = mConfig.recommendedSwapchainSampleCount;
        swapchainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
        mWidth = mConfig.recommendedImageRectWidth;
        mHeight = mConfig.recommendedImageRectHeight;
        CHECK_XRCMD(xrCreateSwapchain(pXR.mSession, &swapchainCreateInfo, &mSwapchain));

        uint32_t imageCount = 0;
        CHECK_XRCMD(xrEnumerateSwapchainImages(mSwapchain, 0, &imageCount, nullptr));

        mSwapchainImageBuffers.resize(imageCount, { XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR });
        CHECK_XRCMD(xrEnumerateSwapchainImages(mSwapchain, imageCount, &imageCount, reinterpret_cast<XrSwapchainImageBaseHeader*>(mSwapchainImageBuffers.data())));
        for (const auto& swapchainImage : mSwapchainImageBuffers)
            mTextureBuffers.push_back(new OpenXRTextureBuffer(mXR, state, swapchainImage.image, mWidth, mHeight, 0));

        mSubImage.swapchain = mSwapchain;
        mSubImage.imageRect.offset = { 0, 0 };
        mSubImage.imageRect.extent = { mWidth, mHeight };
        mXR->setViewSubImage(viewIndex, mSubImage);
    }

    OpenXRViewImpl::~OpenXRViewImpl()
    {
        if (mSwapchain)
            CHECK_XRCMD(xrDestroySwapchain(mSwapchain));
    }

    osg::ref_ptr<OpenXRTextureBuffer>
        OpenXRViewImpl::prepareNextSwapchainImage()
    {
        if (!mXR->sessionRunning())
            return nullptr;

        XrSwapchainImageAcquireInfo acquireInfo{ XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
        uint32_t swapchainImageIndex = 0;
        CHECK_XRCMD(xrAcquireSwapchainImage(mSwapchain, &acquireInfo, &swapchainImageIndex));

        XrSwapchainImageWaitInfo waitInfo{ XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
        waitInfo.timeout = XR_INFINITE_DURATION;
        CHECK_XRCMD(xrWaitSwapchainImage(mSwapchain, &waitInfo));

        return mTextureBuffers[swapchainImageIndex];
    }

    void
        OpenXRViewImpl::releaseSwapchainImage()
    {
        if (!mXR->sessionRunning())
            return;

        XrSwapchainImageReleaseInfo releaseInfo{ XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
        CHECK_XRCMD(xrReleaseSwapchainImage(mSwapchain, &releaseInfo));
    }

    void OpenXRViewImpl::prerenderCallback(osg::RenderInfo& renderInfo)
    {
        mCurrentBuffer = prepareNextSwapchainImage();
        if(mCurrentBuffer)
            mCurrentBuffer->beginFrame(renderInfo);
    }

    void OpenXRViewImpl::postrenderCallback(osg::RenderInfo& renderInfo)
    {
        if (mCurrentBuffer)
            mCurrentBuffer->endFrame(renderInfo);
        releaseSwapchainImage();
    }

// Some headers like to define these.
#ifdef near
#undef near
#endif

#ifdef far
#undef far
#endif

    static osg::Matrix
        perspectiveFovMatrix(float near, float far, XrFovf fov)
    {
        const float tanLeft = tanf(fov.angleLeft);
        const float tanRight = tanf(fov.angleRight);
        const float tanDown = tanf(fov.angleDown);
        const float tanUp = tanf(fov.angleUp);

        const float tanWidth = tanRight - tanLeft;
        const float tanHeight = tanUp - tanDown;

        // Set to nearZ for a [-1,1] Z clip space (OpenGL / OpenGL ES).
        // Set to zero for a [0,1] Z clip space (Vulkan / D3D / Metal).
        const float offset = near;
        
        float matrix[16] = {};

        if (far <= near) {
            // place the far plane at infinity
            matrix[0] = 2 / tanWidth;
            matrix[4] = 0;
            matrix[8] = (tanRight + tanLeft) / tanWidth;
            matrix[12] = 0;

            matrix[1] = 0;
            matrix[5] = 2 / tanHeight;
            matrix[9] = (tanUp + tanDown) / tanHeight;
            matrix[13] = 0;

            matrix[2] = 0;
            matrix[6] = 0;
            matrix[10] = -1;
            matrix[14] = -(near + offset);

            matrix[3] = 0;
            matrix[7] = 0;
            matrix[11] = -1;
            matrix[15] = 0;
        }
        else {
            // normal projection
            matrix[0] = 2 / tanWidth;
            matrix[4] = 0;
            matrix[8] = (tanRight + tanLeft) / tanWidth;
            matrix[12] = 0;

            matrix[1] = 0;
            matrix[5] = 2 / tanHeight;
            matrix[9] = (tanUp + tanDown) / tanHeight;
            matrix[13] = 0;

            matrix[2] = 0;
            matrix[6] = 0;
            matrix[10] = -(far + offset) / (far - near);
            matrix[14] = -(far * (near + offset)) / (far - near);

            matrix[3] = 0;
            matrix[7] = 0;
            matrix[11] = -1;
            matrix[15] = 0;
        }
        return osg::Matrix(matrix);
    }

    osg::Matrix OpenXRViewImpl::projectionMatrix()
    {
        auto hmdViews = mXR->mPrivate->getHmdViews();

        float near = Settings::Manager::getFloat("near clip", "Camera");
        float far = Settings::Manager::getFloat("viewing distance", "Camera");
        //return perspectiveFovMatrix()

        return perspectiveFovMatrix(near, far, hmdViews[mViewIndex].fov);
    }



    osg::Matrix OpenXRViewImpl::viewMatrix()
    {
        osg::Matrix viewMatrix;
        auto hmdViews = mXR->mPrivate->getHmdViews();
        auto pose = hmdViews[mViewIndex].pose;
        osg::Vec3 position = osg::Vec3(pose.position.x, pose.position.y, pose.position.z);


        // invert orientation (conjugate of Quaternion) and position to apply to the view matrix as offset
        // TODO: Why invert/conjugate?
        viewMatrix.setTrans(position);
        viewMatrix.postMultRotate(osg::fromXR(pose.orientation));

        // Scale to world units
        viewMatrix.postMultScale(osg::Vec3d(mMetersPerUnit, mMetersPerUnit, mMetersPerUnit));

        return viewMatrix;
    }

    osg::Camera* OpenXRViewImpl::createCamera(int eye, const osg::Vec4& clearColor, osg::GraphicsContext* gc)
    {
        osg::ref_ptr<osg::Camera> camera = new osg::Camera();
        camera->setClearColor(clearColor);
        camera->setClearMask(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        camera->setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT);
        camera->setRenderOrder(osg::Camera::PRE_RENDER, eye);
        camera->setComputeNearFarMode(osg::CullSettings::DO_NOT_COMPUTE_NEAR_FAR);
        camera->setAllowEventFocus(false);
        camera->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
        camera->setViewport(0, 0, mWidth, mHeight);
        camera->setGraphicsContext(gc);

        camera->setInitialDrawCallback(new OpenXRView::InitialDrawCallback());
        camera->setPreDrawCallback(new OpenXRView::PredrawCallback(camera.get(), this));
        camera->setFinalDrawCallback(new OpenXRView::PostdrawCallback(camera.get(), this));

        return camera.release();
    }

    OpenXRView::OpenXRView(
        osg::ref_ptr<OpenXRManager> XR, osg::ref_ptr<osg::State> state, float metersPerUnit, unsigned int viewIndex)
    : mPrivate(new OpenXRViewImpl(XR, state, metersPerUnit, viewIndex))
    {

    }

    OpenXRView::~OpenXRView()
    {

    }

    //! Get the next color buffer
    osg::ref_ptr<OpenXRTextureBuffer> OpenXRView::prepareNextSwapchainImage()
    {
        return mPrivate->prepareNextSwapchainImage();
    }

    //! Release current color buffer. Do not forget to call this after rendering to the color buffer.
    void   OpenXRView::releaseSwapchainImage()
    {
        mPrivate->releaseSwapchainImage();
    }

    void OpenXRView::PredrawCallback::operator()(osg::RenderInfo& info) const
    {
        mView->prerenderCallback(info);
    }

    void OpenXRView::PostdrawCallback::operator()(osg::RenderInfo& info) const
    {
        mView->postrenderCallback(info);
    }

    void OpenXRView::prerenderCallback(osg::RenderInfo& renderInfo)
    {
        mPrivate->prerenderCallback(renderInfo);
    }

    void OpenXRView::postrenderCallback(osg::RenderInfo& renderInfo)
    {
        mPrivate->postrenderCallback(renderInfo);
    }

    osg::Camera* OpenXRView::createCamera(int eye, const osg::Vec4& clearColor, osg::GraphicsContext* gc)
    {
        return mPrivate->createCamera(eye, clearColor, gc);
    }

    osg::Matrix OpenXRView::projectionMatrix()
    {
        return mPrivate->projectionMatrix();
    }

    osg::Matrix OpenXRView::viewMatrix()
    {
        return mPrivate->viewMatrix();
    }

    void OpenXRView::InitialDrawCallback::operator()(osg::RenderInfo& renderInfo) const
    {
        osg::GraphicsOperation* graphicsOperation = renderInfo.getCurrentCamera()->getRenderer();
        osgViewer::Renderer* renderer = dynamic_cast<osgViewer::Renderer*>(graphicsOperation);
        if (renderer != nullptr)
        {
            // Disable normal OSG FBO camera setup because it will undo the MSAA FBO configuration.
            renderer->setCameraRequiresSetUp(false);
        }
    }
}
