#include "settings.hpp"
#include "parser.hpp"

#include <sstream>

#include <components/files/configurationmanager.hpp>
#include <components/misc/stringops.hpp>
#include <components/vr/vr.hpp>

namespace Settings
{

CategorySettingValueMap Manager::mDefaultSettings = CategorySettingValueMap();
CategorySettingValueMap Manager::mUserSettings = CategorySettingValueMap();
CategorySettingValueMap Manager::mSettingsOverrides = CategorySettingValueMap();
CategorySettingVector Manager::mChangedSettings = CategorySettingVector();

void Manager::clear()
{
    mDefaultSettings.clear();
    mUserSettings.clear();
    mChangedSettings.clear();
    mSettingsOverrides.clear();
}

std::string Manager::load(const Files::ConfigurationManager& cfgMgr, bool loadEditorSettings)
{
    SettingsFileParser parser;
    const std::vector<boost::filesystem::path>& paths = cfgMgr.getActiveConfigPaths();
    if (paths.empty())
        throw std::runtime_error("No config dirs! ConfigurationManager::readConfiguration must be called first.");

    // Create file name strings for either the engine or the editor.
    std::string defaultSettingsFile = "";
    std::string userSettingsFile = "";

    if (!loadEditorSettings)
    {
        defaultSettingsFile = "defaults.bin";
        userSettingsFile = "settings.cfg";
}
    else
    {
        defaultSettingsFile = "defaults-cs.bin";
        userSettingsFile = "openmw-cs.cfg";
    }

    // Create the settings manager and load default settings file.
    const std::string defaultsBin = (paths.front() / defaultSettingsFile).string();
    if (!boost::filesystem::exists(defaultsBin))
        throw std::runtime_error ("No default settings file found! Make sure the file \"" + defaultSettingsFile + "\" was properly installed.");
    parser.loadSettingsFile(defaultsBin, mDefaultSettings, true, false);

    // Load "settings.cfg" or "openmw-cs.cfg" from every config dir except the last one as additional default settings.
    for (int i = 0; i < static_cast<int>(paths.size()) - 1; ++i)
{
        const std::string additionalDefaults = (paths[i] / userSettingsFile).string();
        if (boost::filesystem::exists(additionalDefaults))
            parser.loadSettingsFile(additionalDefaults, mDefaultSettings, false, true);
}

    // Load "settings.cfg" or "openmw-cs.cfg" from the last config dir as user settings. This path will be used to save modified settings.
    std::string settingspath = (paths.back() / userSettingsFile).string();
    if (boost::filesystem::exists(settingspath))
        parser.loadSettingsFile(settingspath, mUserSettings, false, false);

    if (VR::getVR())
    {
        const std::string overridesFile = (paths.front() / "settings-overrides-vr.cfg").string();
        if (boost::filesystem::exists(overridesFile))
            loadOverrides(overridesFile);
        else
            throw std::runtime_error("No settings overrides file found! Make sure the file \"settings-overrides-vr.cfg\" was properly installed.");
    }

    return settingspath;
}

void Manager::loadOverrides(const std::string& file)
{
    SettingsFileParser parser;
    parser.loadSettingsFile(file, mSettingsOverrides);
}

void Manager::saveUser(const std::string &file)
{
    SettingsFileParser parser;
    parser.saveSettingsFile(file, mUserSettings);
}

std::string Manager::getString(const std::string &setting, const std::string &category)
{
    CategorySettingValueMap::key_type key (category, setting);
    CategorySettingValueMap::iterator it = mSettingsOverrides.find(key);
    if (it != mSettingsOverrides.end())
        return it->second;

    it = mUserSettings.find(key);
    if (it != mUserSettings.end())
        return it->second;

    it = mDefaultSettings.find(key);
    if (it != mDefaultSettings.end())
        return it->second;

    throw std::runtime_error(std::string("Trying to retrieve a non-existing setting: ") + setting
                             + ".\nMake sure the defaults.bin file was properly installed.");
}

float Manager::getFloat (const std::string& setting, const std::string& category)
{
    const std::string& value = getString(setting, category);
    std::stringstream stream(value);
    float number = 0.f;
    stream >> number;
    return number;
}

double Manager::getDouble (const std::string& setting, const std::string& category)
{
    const std::string& value = getString(setting, category);
    std::stringstream stream(value);
    double number = 0.0;
    stream >> number;
    return number;
}

int Manager::getInt (const std::string& setting, const std::string& category)
{
    const std::string& value = getString(setting, category);
    std::stringstream stream(value);
    int number = 0;
    stream >> number;
    return number;
}

std::int64_t Manager::getInt64 (const std::string& setting, const std::string& category)
{
    const std::string& value = getString(setting, category);
    std::stringstream stream(value);
    std::size_t number = 0;
    stream >> number;
    return number;
}

bool Manager::getBool (const std::string& setting, const std::string& category)
{
    const std::string& string = getString(setting, category);
    return Misc::StringUtils::ciEqual(string, "true");
}

osg::Vec2f Manager::getVector2 (const std::string& setting, const std::string& category)
{
    const std::string& value = getString(setting, category);
    std::stringstream stream(value);
    float x, y;
    stream >> x >> y;
    if (stream.fail())
        throw std::runtime_error(std::string("Can't parse 2d vector: " + value));
    return {x, y};
}

osg::Vec3f Manager::getVector3 (const std::string& setting, const std::string& category)
{
    const std::string& value = getString(setting, category);
    std::stringstream stream(value);
    float x, y, z;
    stream >> x >> y >> z;
    if (stream.fail())
        throw std::runtime_error(std::string("Can't parse 3d vector: " + value));
    return {x, y, z};
}

void Manager::setString(const std::string &setting, const std::string &category, const std::string &value)
{
    CategorySettingValueMap::key_type key (category, setting);
    auto found = mSettingsOverrides.find(key);
    if (found != mSettingsOverrides.end())
        return;

    found = mUserSettings.find(key);
    if (found != mUserSettings.end())
    {
        if (found->second == value)
            return;
    }

    mUserSettings[key] = value;

    mChangedSettings.insert(key);
}

void Manager::setInt (const std::string& setting, const std::string& category, const int value)
{
    std::ostringstream stream;
    stream << value;
    setString(setting, category, stream.str());
}

void Manager::setFloat (const std::string &setting, const std::string &category, const float value)
{
    std::ostringstream stream;
    stream << value;
    setString(setting, category, stream.str());
}

void Manager::setDouble (const std::string &setting, const std::string &category, const double value)
{
    std::ostringstream stream;
    stream << value;
    setString(setting, category, stream.str());
}

void Manager::setBool(const std::string &setting, const std::string &category, const bool value)
{
    setString(setting, category, value ? "true" : "false");
}

void Manager::setVector2 (const std::string &setting, const std::string &category, const osg::Vec2f value)
{
    std::ostringstream stream;
    stream << value.x() << " " << value.y();
    setString(setting, category, stream.str());
}

void Manager::setVector3 (const std::string &setting, const std::string &category, const osg::Vec3f value)
{
    std::ostringstream stream;
    stream << value.x() << ' ' << value.y() << ' ' << value.z();
    setString(setting, category, stream.str());
}

void Manager::resetPendingChange(const std::string &setting, const std::string &category)
{
    CategorySettingValueMap::key_type key (category, setting);
    mChangedSettings.erase(key);
}

CategorySettingVector Manager::getPendingChanges()
{
    return mChangedSettings;
}

CategorySettingVector Manager::getPendingChanges(const CategorySettingVector& filter)
{
    CategorySettingVector intersection;
    std::set_intersection(mChangedSettings.begin(), mChangedSettings.end(),
                          filter.begin(), filter.end(),
                          std::inserter(intersection, intersection.begin()));
    return intersection;
}

void Manager::resetPendingChanges()
{
    mChangedSettings.clear();
}

void Manager::resetPendingChanges(const CategorySettingVector& filter)
{
    for (const auto& key : filter)
    {
        mChangedSettings.erase(key);
    }
}

void Manager::overrideString(const std::string& setting, const std::string& category, const std::string& value)
{
    CategorySettingValueMap::key_type key = std::make_pair(category, setting);

    CategorySettingValueMap::iterator found = mUserSettings.find(key);
    if (found != mUserSettings.end())
    {
        if (found->second == value)
            return;
    }

    mSettingsOverrides[key] = value;
}

void Manager::overrideInt(const std::string& setting, const std::string& category, const int value)
{
    std::ostringstream stream;
    stream << value;
    overrideString(setting, category, stream.str());
}

void Manager::overrideFloat(const std::string& setting, const std::string& category, const float value)
{
    std::ostringstream stream;
    stream << value;
    overrideString(setting, category, stream.str());
}

void Manager::overrideBool(const std::string& setting, const std::string& category, const bool value)
{
    overrideString(setting, category, value ? "true" : "false");
}

void Manager::overrideVector2(const std::string& setting, const std::string& category, const osg::Vec2f value)
{
    std::ostringstream stream;
    stream << value.x() << " " << value.y();
    overrideString(setting, category, stream.str());
}

void Manager::overrideVector3(const std::string& setting, const std::string& category, const osg::Vec3f value)
{
    std::ostringstream stream;
    stream << value.x() << ' ' << value.y() << ' ' << value.z();
    overrideString(setting, category, stream.str());
}

}
