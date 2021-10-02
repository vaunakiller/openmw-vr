#include "extensions.hpp"
#include "debug.hpp"
#include "platform.hpp"

#include <openxr/openxr.h>
#include <cassert>

#include <components/debug/debuglog.hpp>
#include <components/settings/settings.hpp>

namespace XR
{
    static Extensions* sExtensions = nullptr;

    Extensions& Extensions::instance()
    {
        assert(sExtensions);
        return *sExtensions;
    }

    Extensions::Extensions()
    {
        if (!sExtensions)
            sExtensions = this;
        else
            throw std::logic_error("Duplicated XR::Extensions singleton");
        
        XrApiLayerProperties apiLayerProperties;
        apiLayerProperties.type = XR_TYPE_API_LAYER_PROPERTIES;
        apiLayerProperties.next = nullptr;

        // Enumerate layers and their extensions.
        uint32_t layerCount;
        CHECK_XRCMD(xrEnumerateApiLayerProperties(0, &layerCount, nullptr));
        std::vector<XrApiLayerProperties> layers(layerCount, apiLayerProperties);
        CHECK_XRCMD(xrEnumerateApiLayerProperties((uint32_t)layers.size(), &layerCount, layers.data()));

        Log(Debug::Verbose) << "Available Extensions: ";
        enumerateExtensions(nullptr, 2);
        Log(Debug::Verbose) << "Available Layers: ";

        if (layers.size() == 0)
        {
            Log(Debug::Verbose) << "  No layers available";
        }
        for (const XrApiLayerProperties& layer : layers) {
            Log(Debug::Verbose) << "  Name=" << layer.layerName << " SpecVersion=" << layer.layerVersion;
            mAvailableLayers[layer.layerName] = layer;
            enumerateExtensions(layer.layerName, 4);
        }
    }

    Extensions::~Extensions()
    {
    }

    bool Extensions::supportsExtension(const std::string& extensionName) const
    {
        return mAvailableExtensions.count(extensionName) > 0;
    }

    bool Extensions::supportsExtension(const std::string& extensionName, uint32_t minimumVersion) const
    {
        auto it = mAvailableExtensions.find(extensionName);
        return it != mAvailableExtensions.end() && it->second.extensionVersion >= minimumVersion;
    }

    bool Extensions::supportsLayer(const std::string& layerName) const
    {
        return mAvailableLayers.count(layerName) > 0;
    }

    bool Extensions::supportsLayer(const std::string& layerName, uint32_t minimumVersion) const
    {
        auto it = mAvailableLayers.find(layerName);
        return it != mAvailableLayers.end() && it->second.layerVersion >= minimumVersion;
    }

    bool Extensions::enableExtension(const std::string& extensionName, bool optional)
    {
        auto it = mAvailableExtensions.find(extensionName);
        if (it != mAvailableExtensions.end())
        {
            Log(Debug::Verbose) << "  " << extensionName << ": enabled";
            mEnabledExtensions.push_back(extensionName);
            return true;
        }
        else
        {
            Log(Debug::Verbose) << "  " << extensionName << ": disabled (not supported)";
            if (!optional)
            {
                throw std::runtime_error(std::string("Required OpenXR extension ") + extensionName + " not supported by the runtime");
            }
            return false;
        }
    }

    bool Extensions::enableExtension(const std::string& extensionName, bool optional, uint32_t minimumVersion)
    {
        auto it = mAvailableExtensions.find(extensionName);
        if (it != mAvailableExtensions.end() && it->second.extensionVersion >= minimumVersion)
        {
            Log(Debug::Verbose) << "  " << extensionName << ": enabled";
            mEnabledExtensions.push_back(extensionName);
            return true;
        }
        else
        {
            Log(Debug::Verbose) << "  " << extensionName << ": disabled (not supported)";
            if (!optional)
            {
                throw std::runtime_error(std::string("Required OpenXR extension ") + extensionName + " not supported by the runtime");
            }
            return false;
        }
    }

    bool Extensions::extensionEnabled(const std::string& extensionName) const
    {
        for (const auto& extension : mEnabledExtensions)
            if (extension == extensionName)
                return true;
        return false;
    }

    void Extensions::selectGraphicsAPIExtension(const std::string& extensionName)
    {
        mGraphicsAPIExtension = extensionName;
    }

    const std::string& Extensions::graphicsAPIExtensionName() const
    {
        return mGraphicsAPIExtension;
    }

    XrInstance Extensions::createXrInstance(const std::string& name)
    {
        setupExtensions();

        std::vector<const char*> cExtensionNames;
        for (const auto& extensionName : mEnabledExtensions)
            cExtensionNames.push_back(extensionName.c_str());

        XrInstance instance = XR_NULL_HANDLE;
        XrInstanceCreateInfo createInfo;
        createInfo.type = XR_TYPE_INSTANCE_CREATE_INFO;
        createInfo.next = nullptr;
        createInfo.enabledExtensionCount = cExtensionNames.size();
        createInfo.enabledExtensionNames = cExtensionNames.data();
        strcpy(createInfo.applicationInfo.applicationName, "openmw_vr");
        createInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;

        auto res = CHECK_XRCMD(xrCreateInstance(&createInfo, &instance));
        if (!XR_SUCCEEDED(res))
            initFailure(res, instance);
        return instance;
    }

    void Extensions::enumerateExtensions(const char* layerName, int logIndent)
    {
        XrExtensionProperties extensionProperties;
        extensionProperties.type = XR_TYPE_EXTENSION_PROPERTIES;
        extensionProperties.next = nullptr;
        
        uint32_t extensionCount = 0;
        std::vector<XrExtensionProperties> availableExtensions;
        CHECK_XRCMD(xrEnumerateInstanceExtensionProperties(layerName, 0, &extensionCount, nullptr));
        availableExtensions.resize(extensionCount, extensionProperties);
        CHECK_XRCMD(xrEnumerateInstanceExtensionProperties(layerName, availableExtensions.size(), &extensionCount, availableExtensions.data()));

        std::vector<std::string> extensionNames;
        const std::string indentStr(logIndent, ' ');
        for (auto& extension : availableExtensions)
        {
            if (layerName)
                mAvailableLayerExtensions[layerName][extension.extensionName] = extension;
            else
                mAvailableExtensions[extension.extensionName] = extension;

            Log(Debug::Verbose) << indentStr << "Name=" << extension.extensionName << " SpecVersion=" << extension.extensionVersion;
        }
    }

#if    !XR_KHR_composition_layer_depth \
    || !XR_EXT_hp_mixed_reality_controller \
    || !XR_EXT_debug_utils \
    || !XR_HTC_vive_cosmos_controller_interaction \
    || !XR_HUAWEI_controller_interaction

#error "OpenXR extensions missing. Please upgrade your copy of the OpenXR SDK to 1.0.13 minimum"
#endif
    void Extensions::setupExtensions()
    {
        mEnabledExtensions.clear();

        std::vector<const char*> optionalExtensions = {
            XR_EXT_HP_MIXED_REALITY_CONTROLLER_EXTENSION_NAME,
            XR_HTC_VIVE_COSMOS_CONTROLLER_INTERACTION_EXTENSION_NAME,
            XR_HUAWEI_CONTROLLER_INTERACTION_EXTENSION_NAME
        };

        if (Settings::Manager::getBool("enable XR_KHR_composition_layer_depth", "VR Debug"))
            optionalExtensions.emplace_back(XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME);

        if (Settings::Manager::getBool("enable XR_EXT_debug_utils", "VR Debug"))
            optionalExtensions.emplace_back(XR_EXT_DEBUG_UTILS_EXTENSION_NAME);

        Log(Debug::Verbose) << "Using extensions:";

        auto graphicsAPIExtension = graphicsAPIExtensionName();
        if (graphicsAPIExtension.empty() || !enableExtension(graphicsAPIExtension, true))
        {
            throw std::runtime_error("No graphics APIs supported by openmw are supported by the OpenXR runtime.");
        }

        for (auto optionalExtension : optionalExtensions)
            enableExtension(optionalExtension, true);
    }
}
