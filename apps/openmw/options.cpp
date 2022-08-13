#include "options.hpp"

#include <components/fallback/validate.hpp>
#include <components/files/configurationmanager.hpp>
#include <components/misc/rng.hpp>


namespace
{
    namespace bpo = boost::program_options;
    typedef std::vector<std::string> StringsVector;
}

namespace OpenMW
{
    bpo::options_description makeOptionsDescription()
    {
        bpo::options_description desc("Syntax: openmw <options>\nAllowed options");
        Files::ConfigurationManager::addCommonOptions(desc);

        desc.add_options()
            ("help", "print help message")
            ("version", "print version information and quit")

            ("data", bpo::value<Files::MaybeQuotedPathContainer>()->default_value(Files::MaybeQuotedPathContainer(), "data")
                ->multitoken()->composing(), "set data directories (later directories have higher priority)")

            ("data-local", bpo::value<Files::MaybeQuotedPath>()->default_value(Files::MaybeQuotedPath(), ""),
                "set local data directory (highest priority)")

            ("fallback-archive", bpo::value<StringsVector>()->default_value(StringsVector(), "fallback-archive")
                ->multitoken()->composing(), "set fallback BSA archives (later archives have higher priority)")

            ("resources", bpo::value<Files::MaybeQuotedPath>()->default_value(Files::MaybeQuotedPath(), "resources"),
                "set resources directory")

            ("start", bpo::value<std::string>()->default_value(""),
                "set initial cell")

            ("content", bpo::value<StringsVector>()->default_value(StringsVector(), "")
                ->multitoken()->composing(), "content file(s): esm/esp, or omwgame/omwaddon/omwscripts")

            ("groundcover", bpo::value<StringsVector>()->default_value(StringsVector(), "")
                ->multitoken()->composing(), "groundcover content file(s): esm/esp, or omwgame/omwaddon")

            ("no-sound", bpo::value<bool>()->implicit_value(true)
                ->default_value(false), "disable all sounds")

            ("script-all", bpo::value<bool>()->implicit_value(true)
                ->default_value(false), "compile all scripts (excluding dialogue scripts) at startup")

            ("script-all-dialogue", bpo::value<bool>()->implicit_value(true)
                ->default_value(false), "compile all dialogue scripts at startup")

            ("script-console", bpo::value<bool>()->implicit_value(true)
                ->default_value(false), "enable console-only script functionality")

            ("script-run", bpo::value<std::string>()->default_value(""),
                "select a file containing a list of console commands that is executed on startup")

            ("script-warn", bpo::value<int>()->implicit_value (1)
                ->default_value (1),
                "handling of warnings when compiling scripts\n"
                "\t0 - ignore warning\n"
                "\t1 - show warning but consider script as correctly compiled anyway\n"
                "\t2 - treat warnings as errors")

            ("script-blacklist", bpo::value<StringsVector>()->default_value(StringsVector(), "")
                ->multitoken()->composing(), "ignore the specified script (if the use of the blacklist is enabled)")

            ("script-blacklist-use", bpo::value<bool>()->implicit_value(true)
                ->default_value(true), "enable script blacklisting")

            ("load-savegame", bpo::value<Files::MaybeQuotedPath>()->default_value(Files::MaybeQuotedPath(), ""),
                "load a save game file on game startup (specify an absolute filename or a filename relative to the current working directory)")

            ("skip-menu", bpo::value<bool>()->implicit_value(true)
                ->default_value(false), "skip main menu on game startup")

            ("new-game", bpo::value<bool>()->implicit_value(true)
                ->default_value(false), "run new game sequence (ignored if skip-menu=0)")

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

            ("no-grab", bpo::value<bool>()->implicit_value(true)->default_value(false), "Don't grab mouse cursor")

            ("export-fonts", bpo::value<bool>()->implicit_value(true)
                ->default_value(false), "Export Morrowind .fnt fonts to PNG image and XML file in current directory")

            ("activate-dist", bpo::value <int> ()->default_value (-1), "activation distance override")

            ("random-seed", bpo::value <unsigned int> ()
                ->default_value(Misc::Rng::generateDefaultSeed()),
                "seed value for random number generator")
        ;

        return desc;
    }
}
