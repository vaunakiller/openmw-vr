#include "multiview.hpp"

#include <osg/GLExtensions>

#include <components/settings/settings.hpp>
#include <components/debug/debuglog.hpp>

namespace Stereo
{
    namespace
    {
        bool getMultiviewSupported(unsigned int contextID)
        {
#ifdef OSG_HAS_MULTIVIEW
            if (!Settings::Manager::getBool("stereo enabled", "Stereo"))
            {
                Log(Debug::Verbose) << "Disabling Multiview (disabled by config)";
                return false;
            }

            if (!Settings::Manager::getBool("multiview", "Stereo"))
            {
                Log(Debug::Verbose) << "Disabling Multiview (disabled by config)";
                return false;
            }

            if (!osg::isGLExtensionSupported(contextID, "GL_OVR_multiview"))
            {
                Log(Debug::Verbose) << "Disabling Multiview (opengl extension \"GL_OVR_multiview\" not supported)";
                return false;
            }

            if (!osg::isGLExtensionSupported(contextID, "GL_OVR_multiview2"))
            {
                Log(Debug::Verbose) << "Disabling Multiview (opengl extension \"GL_OVR_multiview2\" not supported)";
                return false;
            }

            if (!osg::isGLExtensionOrVersionSupported(contextID, "ARB_texture_view", 4.3))
            {
                Log(Debug::Verbose) << "Disabling Multiview (opengl extension \"ARB_texture_view\" not supported)";
                return false;
            }

            Log(Debug::Verbose) << "Enabling Multiview";

            return true;
#else
            return false;
#endif
        }
    }

    bool getMultiview(unsigned int contextID)
    {
        static bool multiView = getMultiviewSupported(contextID);
        return multiView;
    }
}
