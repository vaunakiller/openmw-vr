#include "sdlcursormanager.hpp"

#include <stdexcept>

#include <SDL_mouse.h>
#include <SDL_endian.h>
#include <SDL_render.h>
#include <SDL_hints.h>

#include <osg/GraphicsContext>
#include <osg/Geometry>
#include <osg/Texture2D>
#include <osg/TexMat>
#include <osg/Version>
#include <osgViewer/GraphicsWindow>

#include <components/debug/debuglog.hpp>

#include "imagetosurface.hpp"

#if defined(OSG_LIBRARY_STATIC) && (!defined(ANDROID) || OSG_VERSION_GREATER_THAN(3, 6, 5))
// Sets the default windowing system interface according to the OS.
// Necessary for OpenSceneGraph to do some things, like decompression.
USE_GRAPHICSWINDOW()
#endif

namespace SDLUtil
{

    SDLCursorManager::SDLCursorManager() :
        mEnabled(false),
        mInitialized(false)
    {
    }

    SDLCursorManager::~SDLCursorManager()
    {
        CursorMap::const_iterator curs_iter = mCursorMap.begin();

        while(curs_iter != mCursorMap.end())
        {
            SDL_FreeCursor(curs_iter->second);
            ++curs_iter;
        }

        mCursorMap.clear();
    }

    void SDLCursorManager::setEnabled(bool enabled)
    {
        if(mInitialized && enabled == mEnabled)
            return;

        mInitialized = true;
        mEnabled = enabled;

        //turn on hardware cursors
        if(enabled)
        {
            _setGUICursor(mCurrentCursor);
        }
        //turn off hardware cursors
        else
        {
            SDL_ShowCursor(SDL_FALSE);
        }
    }

    void SDLCursorManager::cursorChanged(const std::string& name)
    {
        mCurrentCursor = name;
        _setGUICursor(name);
    }

    void SDLCursorManager::_setGUICursor(const std::string &name)
    {
        auto it = mCursorMap.find(name);
        if (it != mCursorMap.end())
            SDL_SetCursor(it->second);
    }

    void SDLCursorManager::createCursor(const std::string& name, int rotDegrees, osg::Image* image, Uint8 hotspot_x, Uint8 hotspot_y)
    {
#ifndef ANDROID
        _createCursorFromResource(name, rotDegrees, image, hotspot_x, hotspot_y);
#endif
    }

    SDLUtil::SurfaceUniquePtr decompress(osg::ref_ptr<osg::Image> source, float rotDegrees)
    {
        int width = source->s();
        int height = source->t();
        bool useAlpha = source->isImageTranslucent();

        osg::ref_ptr<osg::Image> decompressedImage = new osg::Image;
        decompressedImage->setFileName(source->getFileName());
        decompressedImage->allocateImage(width, height, 1, useAlpha ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE);
        for (int s=0; s<width; ++s)
            for (int t=0; t<height; ++t)
                decompressedImage->setColor(source->getColor(s,t,0), s,t,0);

        Uint32 redMask = 0x000000ff;
        Uint32 greenMask = 0x0000ff00;
        Uint32 blueMask = 0x00ff0000;
        Uint32 alphaMask = useAlpha ? 0xff000000 : 0;

        SDL_Surface *cursorSurface = SDL_CreateRGBSurfaceFrom(decompressedImage->data(),
                                                              width,
                                                              height,
                                                              decompressedImage->getPixelSizeInBits(),
                                                              decompressedImage->getRowSizeInBytes(),
                                                              redMask,
                                                              greenMask,
                                                              blueMask,
                                                              alphaMask);

        SDL_Surface *targetSurface = SDL_CreateRGBSurface(0, width, height, 32, redMask, greenMask, blueMask, alphaMask);
        SDL_Renderer *renderer = SDL_CreateSoftwareRenderer(targetSurface);

        SDL_RenderClear(renderer);

        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
        SDL_Texture *cursorTexture = SDL_CreateTextureFromSurface(renderer, cursorSurface);

        SDL_RenderCopyEx(renderer, cursorTexture, nullptr, nullptr, -rotDegrees, nullptr, SDL_FLIP_VERTICAL);

        SDL_DestroyTexture(cursorTexture);
        SDL_FreeSurface(cursorSurface);
        SDL_DestroyRenderer(renderer);

        return SDLUtil::SurfaceUniquePtr(targetSurface, SDL_FreeSurface);
    }

    void SDLCursorManager::_createCursorFromResource(const std::string& name, int rotDegrees, osg::Image* image, Uint8 hotspot_x, Uint8 hotspot_y)
    {
        if (mCursorMap.find(name) != mCursorMap.end())
            return;

        try {
            auto surface = decompress(image, static_cast<float>(rotDegrees));

            //set the cursor and store it for later
            SDL_Cursor* curs = SDL_CreateColorCursor(surface.get(), hotspot_x, hotspot_y);

            mCursorMap.insert(CursorMap::value_type(std::string(name), curs));
        } catch (std::exception& e) {
            Log(Debug::Warning) << e.what();
            Log(Debug::Warning) << "Using default cursor.";
            return;
        }
    }

}
