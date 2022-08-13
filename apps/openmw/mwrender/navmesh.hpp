#ifndef OPENMW_MWRENDER_NAVMESH_H
#define OPENMW_MWRENDER_NAVMESH_H

#include "navmeshmode.hpp"

#include <components/detournavigator/version.hpp>
#include <components/detournavigator/tileposition.hpp>
#include <components/misc/guarded.hpp>

#include <osg/ref_ptr>

#include <cstddef>
#include <map>
#include <memory>
#include <vector>
#include <string_view>

class dtNavMesh;

namespace osg
{
    class Group;
    class Geometry;
    class StateSet;
}

namespace DetourNavigator
{
    class NavMeshCacheItem;
    struct Settings;
}

namespace SceneUtil
{
    class WorkQueue;
}

namespace MWRender
{
    class NavMesh
    {
    public:
        explicit NavMesh(const osg::ref_ptr<osg::Group>& root, const osg::ref_ptr<SceneUtil::WorkQueue>& workQueue,
                         bool enabled, NavMeshMode mode);
        ~NavMesh();

        bool toggle();

        void update(const std::shared_ptr<Misc::ScopeGuarded<DetourNavigator::NavMeshCacheItem>>& navMesh,
            std::size_t id, const DetourNavigator::Settings& settings);

        void reset();

        void enable();

        void disable();

        bool isEnabled() const
        {
            return mEnabled;
        }

        void setMode(NavMeshMode value);

    private:
        struct Tile
        {
            DetourNavigator::Version mVersion;
            osg::ref_ptr<osg::Group> mGroup;
        };

        struct LessByTilePosition;
        struct CreateNavMeshTileGroups;
        struct DeallocateCreateNavMeshTileGroups;

        osg::ref_ptr<osg::Group> mRootNode;
        osg::ref_ptr<SceneUtil::WorkQueue> mWorkQueue;
        osg::ref_ptr<osg::StateSet> mGroupStateSet;
        osg::ref_ptr<osg::StateSet> mDebugDrawStateSet;
        bool mEnabled;
        NavMeshMode mMode;
        std::size_t mId;
        DetourNavigator::Version mVersion;
        std::map<DetourNavigator::TilePosition, Tile> mTiles;
        std::vector<osg::ref_ptr<CreateNavMeshTileGroups>> mWorkItems;
    };
}

#endif
