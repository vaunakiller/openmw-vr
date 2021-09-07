#ifndef XR_EXTENSIONS_HPP
#define XR_EXTENSIONS_HPP

#include <vector>
#include <memory>
#include <set>
#include <string>
#include <map>

#include <openxr/openxr.h>

#include <components/vr/directx.hpp>

namespace XR
{
    class Extensions
    {
        using ExtensionMap = std::map<std::string, XrExtensionProperties>;
        using LayerMap = std::map<std::string, XrApiLayerProperties>;
        using LayerExtensionMap = std::map<std::string, ExtensionMap>;

    public:
        static Extensions& instance();

    public:
        Extensions();
        ~Extensions();

        bool supportsExtension(const std::string& extensionName) const;
        bool supportsExtension(const std::string& extensionName, uint32_t minimumVersion) const;
        bool supportsLayer(const std::string& layerName) const;
        bool supportsLayer(const std::string& layerName, uint32_t minimumVersion) const;
        bool enableExtension(const std::string& extensionName, bool optional);
        bool enableExtension(const std::string& extensionName, bool optional, uint32_t minimumVersion);
        bool extensionEnabled(const std::string& extensionName) const;
        void selectGraphicsAPIExtension(const std::string& extensionName);
        const std::string& graphicsAPIExtensionName() const;

        XrInstance createXrInstance(const std::string& name);

    private:
        void enumerateExtensions(const char* layerName, int logIndent);
        void setupExtensions();

        ExtensionMap mAvailableExtensions;
        LayerMap mAvailableLayers;
        LayerExtensionMap mAvailableLayerExtensions;
        std::vector<std::string> mEnabledExtensions;
        std::string mGraphicsAPIExtension = "";
    };
}

#endif
