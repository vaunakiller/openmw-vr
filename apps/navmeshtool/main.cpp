#include "worldspacedata.hpp"
#include "navmesh.hpp"

#include <components/debug/debugging.hpp>
#include <components/detournavigator/navmeshdb.hpp>
#include <components/detournavigator/recastglobalallocator.hpp>
#include <components/esm3/esmreader.hpp>
#include <components/esm3/variant.hpp>
#include <components/esmloader/esmdata.hpp>
#include <components/esmloader/load.hpp>
#include <components/fallback/fallback.hpp>
#include <components/fallback/validate.hpp>
#include <components/files/configurationmanager.hpp>
#include <components/resource/bulletshapemanager.hpp>
#include <components/resource/imagemanager.hpp>
#include <components/resource/niffilemanager.hpp>
#include <components/resource/scenemanager.hpp>
#include <components/settings/settings.hpp>
#include <components/to_utf8/to_utf8.hpp>
#include <components/version/version.hpp>
#include <components/vfs/manager.hpp>
#include <components/vfs/registerarchives.hpp>

#include <osg/Vec3f>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#include <cstddef>
#include <stdexcept>
#include <thread>
#include <vector>

namespace NavMeshTool
{
    namespace
    {
        namespace bpo = boost::program_options;

        using StringsVector = std::vector<std::string>;

        bpo::options_description makeOptionsDescription()
        {
            using Fallback::FallbackMap;

            bpo::options_description result;

            result.add_options()
                ("help", "print help message")

                ("version", "print version information and quit")

                ("data", bpo::value<Files::MaybeQuotedPathContainer>()->default_value(Files::MaybeQuotedPathContainer(), "data")
                    ->multitoken()->composing(), "set data directories (later directories have higher priority)")

                ("data-local", bpo::value<Files::MaybeQuotedPathContainer::value_type>()->default_value(Files::MaybeQuotedPathContainer::value_type(), ""),
                    "set local data directory (highest priority)")

                ("fallback-archive", bpo::value<StringsVector>()->default_value(StringsVector(), "fallback-archive")
                    ->multitoken()->composing(), "set fallback BSA archives (later archives have higher priority)")

                ("resources", bpo::value<Files::MaybeQuotedPath>()->default_value(Files::MaybeQuotedPath(), "resources"),
                    "set resources directory")

                ("content", bpo::value<StringsVector>()->default_value(StringsVector(), "")
                    ->multitoken()->composing(), "content file(s): esm/esp, or omwgame/omwaddon/omwscripts")

                ("fs-strict", bpo::value<bool>()->implicit_value(true)
                    ->default_value(false), "strict file system handling (no case folding)")

                ("encoding", bpo::value<std::string>()->
                    default_value("win1252"),
                    "Character encoding used in OpenMW game messages:\n"
                    "\n\twin1250 - Central and Eastern European such as Polish, Czech, Slovak, Hungarian, Slovene, Bosnian, Croatian, Serbian (Latin script), Romanian and Albanian languages\n"
                    "\n\twin1251 - Cyrillic alphabet such as Russian, Bulgarian, Serbian Cyrillic and other languages\n"
                    "\n\twin1252 - Western European (Latin) alphabet, used by default")

                ("fallback", bpo::value<Fallback::FallbackMap>()->default_value(Fallback::FallbackMap(), "")
                    ->multitoken()->composing(), "fallback values")

                ("threads", bpo::value<std::size_t>()->default_value(std::max<std::size_t>(std::thread::hardware_concurrency() - 1, 1)),
                    "number of threads for parallel processing")

                ("process-interior-cells", bpo::value<bool>()->implicit_value(true)
                    ->default_value(false), "build navmesh for interior cells")
            ;

            return result;
        }

        void loadSettings(const Files::ConfigurationManager& config, Settings::Manager& settings)
        {
            const std::string localDefault = (config.getLocalPath() / "defaults.bin").string();
            const std::string globalDefault = (config.getGlobalPath() / "defaults.bin").string();

            if (boost::filesystem::exists(localDefault))
                settings.loadDefault(localDefault);
            else if (boost::filesystem::exists(globalDefault))
                settings.loadDefault(globalDefault);
            else
                throw std::runtime_error("No default settings file found! Make sure the file \"defaults.bin\" was properly installed.");

            const std::string settingsPath = (config.getUserConfigPath() / "settings.cfg").string();
            if (boost::filesystem::exists(settingsPath))
                settings.loadUser(settingsPath);
        }

        int runNavMeshTool(int argc, char *argv[])
        {
            bpo::options_description desc = makeOptionsDescription();

            bpo::parsed_options options = bpo::command_line_parser(argc, argv)
                .options(desc).allow_unregistered().run();
            bpo::variables_map variables;

            bpo::store(options, variables);
            bpo::notify(variables);

            if (variables.find("help") != variables.end())
            {
                getRawStdout() << desc << std::endl;
                return 0;
            }

            Files::ConfigurationManager config;

            bpo::variables_map composingVariables = Files::separateComposingVariables(variables, desc);
            config.readConfiguration(variables, desc);
            Files::mergeComposingVariables(variables, composingVariables, desc);

            const std::string encoding(variables["encoding"].as<std::string>());
            Log(Debug::Info) << ToUTF8::encodingUsingMessage(encoding);
            ToUTF8::Utf8Encoder encoder(ToUTF8::calculateEncoding(encoding));

            Files::PathContainer dataDirs(asPathContainer(variables["data"].as<Files::MaybeQuotedPathContainer>()));

            auto local = variables["data-local"].as<Files::MaybeQuotedPathContainer::value_type>();
            if (!local.empty())
                dataDirs.push_back(std::move(local));

            config.processPaths(dataDirs);

            const auto fsStrict = variables["fs-strict"].as<bool>();
            const auto resDir = variables["resources"].as<Files::MaybeQuotedPath>();
            Version::Version v = Version::getOpenmwVersion(resDir.string());
            Log(Debug::Info) << v.describe();
            dataDirs.insert(dataDirs.begin(), resDir / "vfs");
            const auto fileCollections = Files::Collections(dataDirs, !fsStrict);
            const auto archives = variables["fallback-archive"].as<StringsVector>();
            const auto contentFiles = variables["content"].as<StringsVector>();
            const std::size_t threadsNumber = variables["threads"].as<std::size_t>();

            if (threadsNumber < 1)
            {
                std::cerr << "Invalid threads number: " << threadsNumber << ", expected >= 1";
                return -1;
            }

            const bool processInteriorCells = variables["process-interior-cells"].as<bool>();

            Fallback::Map::init(variables["fallback"].as<Fallback::FallbackMap>().mMap);

            VFS::Manager vfs(fsStrict);

            VFS::registerArchives(&vfs, fileCollections, archives, true);

            Settings::Manager settings;
            loadSettings(config, settings);

            const osg::Vec3f agentHalfExtents = Settings::Manager::getVector3("default actor pathfind half extents", "Game");

            DetourNavigator::NavMeshDb db((config.getUserDataPath() / "navmesh.db").string());

            std::vector<ESM::ESMReader> readers(contentFiles.size());
            EsmLoader::Query query;
            query.mLoadActivators = true;
            query.mLoadCells = true;
            query.mLoadContainers = true;
            query.mLoadDoors = true;
            query.mLoadGameSettings = true;
            query.mLoadLands = true;
            query.mLoadStatics = true;
            const EsmLoader::EsmData esmData = EsmLoader::loadEsmData(query, contentFiles, fileCollections, readers, &encoder);

            Resource::ImageManager imageManager(&vfs);
            Resource::NifFileManager nifFileManager(&vfs);
            Resource::SceneManager sceneManager(&vfs, &imageManager, &nifFileManager);
            Resource::BulletShapeManager bulletShapeManager(&vfs, &sceneManager, &nifFileManager);
            DetourNavigator::RecastGlobalAllocator::init();
            DetourNavigator::Settings navigatorSettings = DetourNavigator::makeSettingsFromSettingsManager();
            navigatorSettings.mRecast.mSwimHeightScale = EsmLoader::getGameSetting(esmData.mGameSettings, "fSwimHeightScale").getFloat();

            WorldspaceData cellsData = gatherWorldspaceData(navigatorSettings, readers, vfs, bulletShapeManager,
                                                            esmData, processInteriorCells);

            generateAllNavMeshTiles(agentHalfExtents, navigatorSettings, threadsNumber, cellsData, std::move(db));

            Log(Debug::Info) << "Done";

            return 0;
        }
    }
}

int main(int argc, char *argv[])
{
    return wrapApplication(NavMeshTool::runNavMeshTool, argc, argv, "NavMeshTool");
}
