#include "callbackmanager.hpp"

#include "osgUtil/RenderStage"
#include "osgViewer/Viewer"
#include "osgViewer/Renderer"

#include <components/debug/debuglog.hpp>

namespace Misc
{
    //! Customized RenderStage that handles callbacks thread-safely
    //! and without any risk that unrelated code will delete your callbacks.
    class CustomRenderStage : public osgUtil::RenderStage
    {
    public:
        CustomRenderStage();

        CustomRenderStage(const osgUtil::RenderStage& rhs, const osg::CopyOp& copyop = osg::CopyOp::SHALLOW_COPY);
        CustomRenderStage(const CustomRenderStage& rhs, const osg::CopyOp& copyop = osg::CopyOp::SHALLOW_COPY);

        void setCallbackManager(CallbackManager* manager) {
            mCallbackManager = manager;
        }

        void setStereoView(CallbackManager::View view) {
            mView = view;
        }

        osg::Object* cloneType() const override { return new CustomRenderStage(); }
        osg::Object* clone(const osg::CopyOp& copyop) const override { return new CustomRenderStage(*this, copyop); } // note only implements a clone of type.
        bool isSameKindAs(const osg::Object* obj) const override { return dynamic_cast<const CustomRenderStage*>(obj) != 0L; }
        const char* className() const override { return "OMWRenderStage"; }
        void draw(osg::RenderInfo& renderInfo, osgUtil::RenderLeaf*& previous) override;

    private:
        CallbackManager* mCallbackManager = nullptr;
        CallbackManager::View mView = CallbackManager::View::NotStereo;
    };

    static CallbackManager* sInstance = nullptr;

    CallbackManager& CallbackManager::instance()
    {
        return *sInstance;
    }

    CallbackManager::CallbackManager(osg::ref_ptr<osgViewer::Viewer> viewer)
        : mUserCallbacks{ }
        , mViewer{ viewer }
    {
        if (sInstance)
            throw std::logic_error("Double instance og StereoView");
        sInstance = this;

        auto* camera = mViewer->getCamera();

        // To make draw callbacks reliable, we have to replace the RenderStage of OSG
        // with one that calls our callback manager (in addition to calling OSG's callbacks as usual).
        osg::GraphicsOperation* graphicsOperation = camera->getRenderer();
        osgViewer::Renderer* renderer = dynamic_cast<osgViewer::Renderer*>(graphicsOperation);
        if (renderer != nullptr)
        {
            for (unsigned int i : {0, 1})
            {
                auto* sceneView = renderer->getSceneView(i);
                if (sceneView)
                {
                    CustomRenderStage* newRenderStage = nullptr;
                    if (auto* oldRenderStage = sceneView->getRenderStage())
                        newRenderStage = new CustomRenderStage(*oldRenderStage, osg::CopyOp::DEEP_COPY_ALL);
                    else
                        newRenderStage = new CustomRenderStage();
                    newRenderStage->setCallbackManager(this);
                    sceneView->setRenderStage(newRenderStage);

                    CustomRenderStage* newRenderStageLeft = nullptr;
                    if (auto* oldRenderStageLeft = sceneView->getRenderStageLeft())
                        newRenderStageLeft = new CustomRenderStage(*oldRenderStageLeft, osg::CopyOp::DEEP_COPY_ALL);
                    else
                        newRenderStageLeft = new CustomRenderStage(*newRenderStage, osg::CopyOp::DEEP_COPY_ALL);
                    newRenderStageLeft->setCallbackManager(this);
                    newRenderStageLeft->setStereoView(View::Left);
                    sceneView->setRenderStageLeft(newRenderStageLeft);

                    CustomRenderStage* newRenderStageRight = nullptr;
                    if (auto* oldRenderStageRight = sceneView->getRenderStageRight())
                        newRenderStageRight = new CustomRenderStage(*oldRenderStageRight, osg::CopyOp::DEEP_COPY_ALL);
                    else
                        newRenderStageRight = new CustomRenderStage(*newRenderStage, osg::CopyOp::DEEP_COPY_ALL);
                    newRenderStageRight->setCallbackManager(this);
                    newRenderStageRight->setStereoView(View::Right);
                    sceneView->setRenderStageRight(newRenderStageRight);

                    // OSG should always creates all 3 cull visitors, but i branch anyway in case there are variants that don't.
                    auto* cvMain = sceneView->getCullVisitor();
                    auto* cvLeft = sceneView->getCullVisitorLeft();
                    auto* cvRight = sceneView->getCullVisitorRight();
                    if (!cvMain)
                        sceneView->setCullVisitor(cvMain = new osgUtil::CullVisitor());
                    if (!cvLeft)
                        sceneView->setCullVisitor(cvLeft = cvMain->clone());
                    if (!cvRight)
                        sceneView->setCullVisitor(cvRight = cvMain->clone());

                    // Osg gives cullVisitorLeft and cullVisitor the same identifier.
                    cvMain->setIdentifier(mIdentifierMain);
                    cvLeft->setIdentifier(mIdentifierLeft);
                    cvRight->setIdentifier(mIdentifierRight);
                }
                else
                {
                    Log(Debug::Warning) << "Warning: renderer has no SceneViews, unable to setup callback management";
                }
            }
        }
        else
        {
            Log(Debug::Warning) << "Warning: camera has no Renderer, unable to setup callback management";
        }
    }

    void CallbackManager::callback(DrawStage stage, View view, osg::RenderInfo& info)
    {
        std::unique_lock<std::mutex> lock(mMutex);
        auto frameNo = info.getState()->getFrameStamp()->getFrameNumber();
        auto& callbacks = mUserCallbacks[stage];

        for (int i = 0; static_cast<unsigned int>(i) < callbacks.size(); i++)
        {
            auto& callbackInfo = callbacks[i];
            if (frameNo >= callbackInfo.frame)
            {
                if (callbackInfo.callback)
                    callbackInfo.callback->run(info, view);
                if (callbackInfo.oneshot)
                {
                    callbacks.erase(callbacks.begin() + 1);
                    i--;
                }
            }
        }

        mCondition.notify_all();
    }

    void CallbackManager::addCallback(DrawStage stage, std::shared_ptr<DrawCallback> cb)
    {
        std::unique_lock<std::mutex> lock(mMutex);
        CallbackInfo callbackInfo;
        callbackInfo.callback = cb;
        callbackInfo.frame = mViewer->getFrameStamp()->getFrameNumber();
        callbackInfo.oneshot = false;
        mUserCallbacks[stage].push_back(callbackInfo);
    }

    void CallbackManager::removeCallback(DrawStage stage, std::shared_ptr<DrawCallback> cb)
    {
        std::unique_lock<std::mutex> lock(mMutex);
        auto& cbs = mUserCallbacks[stage];
        for (uint32_t i = 0; i < cbs.size(); i++)
            if (cbs[i].callback == cb)
                cbs.erase(cbs.begin() + i);
    }

    void CallbackManager::addCallbackOneshot(DrawStage stage, std::shared_ptr<DrawCallback> cb)
    {
        std::unique_lock<std::mutex> lock(mMutex);
        CallbackInfo callbackInfo;
        callbackInfo.callback = cb;
        callbackInfo.frame = mViewer->getFrameStamp()->getFrameNumber();
        callbackInfo.oneshot = true;
        mUserCallbacks[stage].push_back(callbackInfo);
    }

    void CallbackManager::waitCallbackOneshot(DrawStage stage, std::shared_ptr<DrawCallback> cb)
    {
        std::unique_lock<std::mutex> lock(mMutex);
        while (hasOneshot(stage, cb))
            mCondition.wait(lock);
    }

    CallbackManager::View CallbackManager::getView(const osgUtil::CullVisitor* cv) const
    {
        if (cv->getIdentifier() == mIdentifierMain)
            return View::NotStereo;
        if (cv->getIdentifier() == mIdentifierLeft)
            return View::Left;
        if (cv->getIdentifier() == mIdentifierRight)
            return View::Right;
        return View::NotStereo;
    }

    bool CallbackManager::hasOneshot(DrawStage stage, std::shared_ptr<DrawCallback> cb)
    {
        for (auto& callbackInfo : mUserCallbacks[stage])
            if (callbackInfo.callback == cb)
                return true;
        return false;
    }

    CustomRenderStage::CustomRenderStage()
    {

    }

    CustomRenderStage::CustomRenderStage(const osgUtil::RenderStage& rhs, const osg::CopyOp& copyop)
        : osgUtil::RenderStage(rhs, copyop)
    {
    }

    CustomRenderStage::CustomRenderStage(const CustomRenderStage& rhs, const osg::CopyOp& copyop)
        : osgUtil::RenderStage(rhs, copyop)
        , mCallbackManager(rhs.mCallbackManager)
        , mView(rhs.mView)
    {
    }

    // This is a copy of DrawInnerOperation from RenderStage.cpp
    // Only changed to match namesapces
    struct DrawInnerOperation : public osg::Operation
    {
        DrawInnerOperation(osgUtil::RenderStage* stage, osg::RenderInfo& renderInfo) :
            osg::Referenced(true),
            osg::Operation("DrawInnerStage", false),
            _stage(stage),
            _renderInfo(renderInfo) {}

        virtual void operator () (osg::Object* object)
        {
            osg::GraphicsContext* context = dynamic_cast<osg::GraphicsContext*>(object);
            if (!context) return;

            // OSG_NOTICE<<"DrawInnerOperation operator"<<std::endl;
            if (_stage && context)
            {
                osgUtil::RenderLeaf* previous = 0;
                bool doCopyTexture = false;
                _renderInfo.setState(context->getState());
                _stage->drawInner(_renderInfo, previous, doCopyTexture);
            }
        }

        osgUtil::RenderStage* _stage;
        osg::RenderInfo _renderInfo;
    };

    // Note: With the exception of mw*Callback(), this is a copy of draw() from RenderStage.cpp edited only for namespacing.
    // It will have to be manually maintained if changes happen upstream.
    void CustomRenderStage::draw(osg::RenderInfo& renderInfo, osgUtil::RenderLeaf*& previous)
    {
        if (_stageDrawnThisFrame) return;

        if (_initialViewMatrix.valid()) renderInfo.getState()->setInitialViewMatrix(_initialViewMatrix.get());

        // push the stages camera so that drawing code can query it
        if (_camera.valid()) renderInfo.pushCamera(_camera.get());

        _stageDrawnThisFrame = true;

        if (_camera.valid() && _camera->getInitialDrawCallback())
        {
            // if we have a camera with a initial draw callback invoke it.
            _camera->getInitialDrawCallback()->run(renderInfo);
        }
        if (mCallbackManager)
            mCallbackManager->callback(CallbackManager::DrawStage::Initial, mView, renderInfo);

        // note, SceneView does call to drawPreRenderStages explicitly
        // so there is no need to call it here.
        drawPreRenderStages(renderInfo, previous);

        if (_cameraRequiresSetUp || (_camera.valid() && _cameraAttachmentMapModifiedCount != _camera->getAttachmentMapModifiedCount()))
        {
            runCameraSetUp(renderInfo);
        }

        osg::State& state = *renderInfo.getState();

        osg::State* useState = &state;
        osg::GraphicsContext* callingContext = state.getGraphicsContext();
        osg::GraphicsContext* useContext = callingContext;
        osg::OperationThread* useThread = 0;
        osg::RenderInfo useRenderInfo(renderInfo);

        osgUtil::RenderLeaf* saved_previous = previous;

        if (_graphicsContext.valid() && _graphicsContext != callingContext)
        {
            // show we release the context so that others can use it?? will do so right
            // now as an experiment.
            callingContext->releaseContext();

            // OSG_NOTICE<<"  enclosing state before - "<<state.getStateSetStackSize()<<std::endl;

            useState = _graphicsContext->getState();
            useContext = _graphicsContext.get();
            useThread = useContext->getGraphicsThread();
            useRenderInfo.setState(useState);

            // synchronize the frame stamps
            useState->setFrameStamp(const_cast<osg::FrameStamp*>(state.getFrameStamp()));

            // map the DynamicObjectCount across to the new window
            useState->setDynamicObjectCount(state.getDynamicObjectCount());
            useState->setDynamicObjectRenderingCompletedCallback(state.getDynamicObjectRenderingCompletedCallback());

            if (!useThread)
            {
                previous = 0;
                useContext->makeCurrent();

                // OSG_NOTICE<<"  nested state before - "<<useState->getStateSetStackSize()<<std::endl;
            }
        }

        unsigned int originalStackSize = useState->getStateSetStackSize();

        if (_camera.valid() && _camera->getPreDrawCallback())
        {
            // if we have a camera with a pre draw callback invoke it.
            _camera->getPreDrawCallback()->run(renderInfo);
        }
        if (mCallbackManager)
            mCallbackManager->callback(CallbackManager::DrawStage::PreDraw, mView, renderInfo);

        bool doCopyTexture = _texture.valid() ?
            (callingContext != useContext) :
            false;

        if (useThread)
        {
#if 1
            osg::ref_ptr<osg::BlockAndFlushOperation> block = new osg::BlockAndFlushOperation;

            useThread->add(new DrawInnerOperation(this, renderInfo));

            useThread->add(block.get());

            // wait till the DrawInnerOperations is complete.
            block->block();

            doCopyTexture = false;

#else
            useThread->add(new DrawInnerOperation(this, renderInfo), true);

            doCopyTexture = false;
#endif
        }
        else
        {
            drawInner(useRenderInfo, previous, doCopyTexture);

            if (useRenderInfo.getUserData() != renderInfo.getUserData())
            {
                renderInfo.setUserData(useRenderInfo.getUserData());
            }

        }

        if (useState != &state)
        {
            // reset the local State's DynamicObjectCount
            state.setDynamicObjectCount(useState->getDynamicObjectCount());
            useState->setDynamicObjectRenderingCompletedCallback(0);
        }


        // now copy the rendered image to attached texture.
        if (_texture.valid() && !doCopyTexture)
        {
            if (callingContext && useContext != callingContext)
            {
                // make the calling context use the pbuffer context for reading.
                callingContext->makeContextCurrent(useContext);
            }

            copyTexture(renderInfo);
        }

        if (_camera.valid() && _camera->getPostDrawCallback())
        {
            // if we have a camera with a post draw callback invoke it.
            _camera->getPostDrawCallback()->run(renderInfo);
        }
        if (mCallbackManager)
            mCallbackManager->callback(CallbackManager::DrawStage::PostDraw, mView, renderInfo);

        if (_graphicsContext.valid() && _graphicsContext != callingContext)
        {
            useState->popStateSetStackToSize(originalStackSize);

            if (!useThread)
            {


                // flush any command left in the useContex's FIFO
                // to ensure that textures are updated before main thread commenses.
                glFlush();


                useContext->releaseContext();
            }
        }

        if (callingContext && useContext != callingContext)
        {
            // restore the graphics context.

            previous = saved_previous;

            // OSG_NOTICE<<"  nested state after - "<<useState->getStateSetStackSize()<<std::endl;
            // OSG_NOTICE<<"  enclosing state after - "<<state.getStateSetStackSize()<<std::endl;

            callingContext->makeCurrent();
        }

        // render all the post draw callbacks
        drawPostRenderStages(renderInfo, previous);

        if (_camera.valid() && _camera->getFinalDrawCallback())
        {
            // if we have a camera with a final callback invoke it.
            _camera->getFinalDrawCallback()->run(renderInfo);
        }
        if (mCallbackManager)
            mCallbackManager->callback(CallbackManager::DrawStage::Final, mView, renderInfo);

        // pop the render stages camera.
        if (_camera.valid()) renderInfo.popCamera();

        // clean up state graph to make sure RenderLeaf etc, can be reused
        if (_rootStateGraph.valid()) _rootStateGraph->clean();
    }
}
