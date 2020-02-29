#include "openxrmenu.hpp"
#include "openxrmanagerimpl.hpp"
#include <openxr/openxr.h>
#include <osg/texturerectangle>

namespace MWVR
{

    OpenXRMenu::OpenXRMenu(
        osg::ref_ptr<osg::Group> root,
        const std::string& title,
        osg::Vec2 extent_meters,
        Pose pose,
        int width,
        int height,
        const osg::Vec4& clearColor,
        osg::GraphicsContext* gc)
        : mTitle(title)
        , mRoot(root)
    {
        osg::ref_ptr<osg::Vec3Array> vertices{ new osg::Vec3Array(4) };
        osg::ref_ptr<osg::Vec2Array> texCoords{ new osg::Vec2Array(4) };
        osg::ref_ptr<osg::Vec3Array> normals{ new osg::Vec3Array(1) };
        osg::ref_ptr<osg::Vec4Array> colors{ new osg::Vec4Array(1) };
        osg::Vec3 top_left(0, 1, 1);
        osg::Vec3 top_right(1, 1, 1);
        osg::Vec3 bottom_left(0, 1, 0);
        osg::Vec3 bottom_right(1, 1, 0);
        (*vertices)[0] = top_left;
        (*vertices)[1] = top_right;
        (*vertices)[2] = bottom_left;
        (*vertices)[3] = bottom_right;
        (*texCoords)[0].set(0.0f, 0.0f);
        (*texCoords)[1].set(0.0f, 1.0f);
        (*texCoords)[2].set(1.0f, 0.0f);
        (*texCoords)[3].set(1.0f, 1.0f);
        (*normals)[0].set(0.0f, -1.0f, 0.0f);
        (*colors)[0].set(1.0f, 1.0f, 1.0f, 0.0f);
        mGeometry->setVertexArray(vertices);
        mGeometry->setTexCoordArray(0, texCoords);
        mGeometry->setNormalArray(normals, osg::Array::BIND_OVERALL);
        mGeometry->setColorArray(colors, osg::Array::BIND_OVERALL);
        mGeometry->addPrimitiveSet(new osg::DrawArrays(GL_QUADS, 0, 4));
        mGeometry->setUseDisplayList(false);

        mMenuCamera->setClearColor(clearColor);
        mMenuCamera->setClearMask(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        mMenuCamera->setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT);
        mMenuCamera->setRenderOrder(osg::Camera::PRE_RENDER, 0);
        mMenuCamera->setComputeNearFarMode(osg::CullSettings::DO_NOT_COMPUTE_NEAR_FAR);
        mMenuCamera->setAllowEventFocus(false);
        mMenuCamera->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
        mMenuCamera->setViewport(0, 0, width, height);
        mMenuCamera->setGraphicsContext(gc);

        //mMenuImage->setFilter(osg::Texture2D::MIN_FILTER, osg::Texture2D::LINEAR);
        //mMenuImage->setFilter(osg::Texture2D::MAG_FILTER, osg::Texture2D::LINEAR);
        //mMenuImage->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE);
        //mMenuImage->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE);
        //mMenuImage->setTextureSize(width, height);
        //mMenuImage->setSourceFormat(GL_SRGB8_ALPHA8);

        mTransform->setScale(osg::Vec3(extent_meters.x(), 1.f, extent_meters.y()));
        mTransform->setAttitude(pose.orientation);
        mTransform->setPosition(pose.position);

        mGeode->addDrawable(mGeometry);
        mTransform->addChild(mGeode);
        mRoot->addChild(mTransform);
    }

    OpenXRMenu::~OpenXRMenu()
    {
    }

    void OpenXRMenu::updateCallback()
    {
    }

    void OpenXRMenu::postRenderCallback(osg::RenderInfo& renderInfo)
    {
        if (!mTexture || mMenuCamera->getAttachmentMapModifiedCount() > 0)
        {
            auto image = mMenuCamera->getBufferAttachmentMap()[osg::Camera::COLOR_BUFFER]._image;
            mTexture = new osg::TextureRectangle(image);
            auto texMat = new osg::TexMat();
            texMat->setScaleByTextureRectangleSize(true);

            auto* stateSet = mGeometry->getOrCreateStateSet();
            stateSet->setTextureAttributeAndModes(0, mTexture, osg::StateAttribute::ON);
            stateSet->setTextureAttributeAndModes(0, texMat, osg::StateAttribute::ON);
            stateSet->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
        }
    }

    void OpenXRMenu::preRenderCallback(osg::RenderInfo& renderInfo)
    {
        // Doesn't need to do anything
    }
}
