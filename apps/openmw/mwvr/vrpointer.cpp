#include "vrpointer.hpp"
#include "vrutil.hpp"

#include <osg/MatrixTransform>
#include <osg/Drawable>
#include <osg/BlendFunc>
#include <osg/Fog>
#include <osg/LightModel>
#include <osg/Material>

#include <components/debug/debuglog.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/resource/scenemanager.hpp>

#include <components/sceneutil/shadow.hpp>
#include <components/sceneutil/positionattitudetransform.hpp>

#include <components/vr/trackingmanager.hpp>
#include <components/vr/viewer.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"

#include "../mwrender/renderingmanager.hpp"
#include "../mwrender/vismask.hpp"

namespace MWVR
{
    UserPointer::UserPointer(osg::Group* root)
        : mRoot(root)
    {
        mPointerGeometry = createPointerGeometry();
        mPointerPAT = new SceneUtil::PositionAttitudeTransform();
        mPointerPAT->addChild(mPointerGeometry);
        mPointerPAT->setNodeMask(MWRender::VisMask::Mask_Pointer);
        mPointerPAT->setName("Pointer Transform 2");
    }

    UserPointer::~UserPointer()
    {
    }

    void UserPointer::setSource(VR::VRPath source)
    {
        mSourcePath = source;

        if (mSource)
        {
            mSource->removeChild(mPointerPAT);
        }
        if(mSourcePath)
        {
            mSource = VR::Viewer::instance().getTrackingNode(source);
            if (mSource)
            {
                mSource->addChild(mPointerPAT);
            }
        }
    }

    const MWRender::RayResult& UserPointer::getPointerRay() const
    {
        return mPointerRay;
    }

    bool UserPointer::canPlaceObject() const
    {
        return mCanPlaceObject;
    }

    void UserPointer::updatePointerTarget()
    {
        mPointerPAT->setScale(osg::Vec3f(1, 1, 1));
        mPointerPAT->setPosition(osg::Vec3f(0, 0, 0));
        mPointerPAT->setAttitude(osg::Quat(0,0,0,1));

        mDistanceToPointerTarget = Util::getPoseTarget(mPointerRay, Util::getNodePose(mPointerPAT), true);
        // Make a ref-counted copy of the target node to ensure the object's lifetime this frame.
        mPointerTarget = mPointerRay.mHitNode;

        mCanPlaceObject = false;
        if (mPointerRay.mHit)
        {
            // check if the wanted position is on a flat surface, and not e.g. against a vertical wall
            mCanPlaceObject = !(std::acos((mPointerRay.mHitNormalWorld / mPointerRay.mHitNormalWorld.length()) * osg::Vec3f(0, 0, 1)) >= osg::DegreesToRadians(30.f));
        }

        if (mDistanceToPointerTarget > 0.f)
        {
            mPointerPAT->setPosition(osg::Vec3f(0.f, 2.f * mDistanceToPointerTarget / 3.f, 0.f));
            mPointerPAT->setScale(osg::Vec3f(0.25f, mDistanceToPointerTarget / 3.f, 0.25f));
        }
        else
        {
            mPointerPAT->setScale(osg::Vec3f(0.25f, 10000.f, 0.25f));
        }
    }

    osg::ref_ptr<osg::Geometry> UserPointer::createPointerGeometry()
    {
        osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry();

        // Create pointer geometry, which will point from the tip of the player's finger.
        // The geometry will be a Four sided pyramid, with the top at the player's fingers

        osg::Vec3 vertices[]{
            {0, 0, 0}, // origin
            {-1, 1, -1}, // A
            {-1, 1, 1}, //  B
            {1, 1, 1}, //   C
            {1, 1, -1}, //  D
        };

        osg::Vec4 colors[]{
            osg::Vec4(1.0f, 0.0f, 0.0f, 0.0f),
            osg::Vec4(1.0f, 0.0f, 0.0f, 1.0f),
            osg::Vec4(1.0f, 0.0f, 0.0f, 1.0f),
            osg::Vec4(1.0f, 0.0f, 0.0f, 1.0f),
            osg::Vec4(1.0f, 0.0f, 0.0f, 1.0f),
        };

        const int O = 0;
        const int A = 1;
        const int B = 2;
        const int C = 3;
        const int D = 4;

        const int triangles[] =
        {
            A,D,B,
            B,D,C,
            O,D,A,
            O,C,D,
            O,B,C,
            O,A,B,
        };
        int numVertices = sizeof(triangles) / sizeof(*triangles);
        osg::ref_ptr<osg::Vec3Array> vertexArray = new osg::Vec3Array(numVertices);
        osg::ref_ptr<osg::Vec4Array> colorArray = new osg::Vec4Array(numVertices);
        for (int i = 0; i < numVertices; i++)
        {
            (*vertexArray)[i] = vertices[triangles[i]];
            (*colorArray)[i] = colors[triangles[i]];
        }

        geometry->setVertexArray(vertexArray);
        geometry->setColorArray(colorArray, osg::Array::BIND_PER_VERTEX);
        geometry->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::TRIANGLES, 0, numVertices));
        geometry->setSupportsDisplayList(false);
        geometry->setDataVariance(osg::Object::STATIC);
        geometry->setName("VRPointer");

        auto stateset = geometry->getOrCreateStateSet();
        stateset->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
        stateset->setMode(GL_BLEND, osg::StateAttribute::ON);
        stateset->setAttributeAndModes(new osg::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
        stateset->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
        osg::ref_ptr<osg::Fog> fog(new osg::Fog);
        fog->setStart(10000000);
        fog->setEnd(10000000);
        stateset->setAttributeAndModes(fog, osg::StateAttribute::OFF | osg::StateAttribute::OVERRIDE);

        osg::ref_ptr<osg::LightModel> lightmodel = new osg::LightModel;
        lightmodel->setAmbientIntensity(osg::Vec4(1.0, 1.0, 1.0, 1.0));
        stateset->setAttributeAndModes(lightmodel, osg::StateAttribute::ON);
        SceneUtil::ShadowManager::disableShadowsForStateSet(stateset);

        osg::ref_ptr<osg::Material> material = new osg::Material;
        material->setColorMode(osg::Material::ColorMode::AMBIENT_AND_DIFFUSE);
        stateset->setAttributeAndModes(material, osg::StateAttribute::ON);

        MWBase::Environment::get().getResourceSystem()->getSceneManager()->recreateShaders(geometry, "objects", true);

        return geometry;
    }

}
