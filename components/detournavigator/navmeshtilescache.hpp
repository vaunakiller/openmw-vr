#ifndef OPENMW_COMPONENTS_DETOURNAVIGATOR_NAVMESHTILESCACHE_H
#define OPENMW_COMPONENTS_DETOURNAVIGATOR_NAVMESHTILESCACHE_H

#include "preparednavmeshdata.hpp"
#include "recastmesh.hpp"
#include "tileposition.hpp"

#include <atomic>
#include <map>
#include <list>
#include <mutex>
#include <cassert>
#include <cstring>
#include <vector>

namespace osg
{
    class Stats;
}

namespace DetourNavigator
{
    struct RecastMeshData
    {
        Mesh mMesh;
        std::vector<CellWater> mWater;
        std::vector<Heightfield> mHeightfields;
        std::vector<FlatHeightfield> mFlatHeightfields;
    };

    inline bool operator <(const RecastMeshData& lhs, const RecastMeshData& rhs)
    {
        return std::tie(lhs.mMesh, lhs.mWater, lhs.mHeightfields, lhs.mFlatHeightfields)
                < std::tie(rhs.mMesh, rhs.mWater, rhs.mHeightfields, rhs.mFlatHeightfields);
    }

    inline bool operator <(const RecastMeshData& lhs, const RecastMesh& rhs)
    {
        return std::tie(lhs.mMesh, lhs.mWater, lhs.mHeightfields, lhs.mFlatHeightfields)
                < std::tie(rhs.getMesh(), rhs.getWater(), rhs.getHeightfields(), rhs.getFlatHeightfields());
    }

    inline bool operator <(const RecastMesh& lhs, const RecastMeshData& rhs)
    {
        return std::tie(lhs.getMesh(), lhs.getWater(), lhs.getHeightfields(), lhs.getFlatHeightfields())
                < std::tie(rhs.mMesh, rhs.mWater, rhs.mHeightfields, rhs.mFlatHeightfields);
    }

    class NavMeshTilesCache
    {
    public:
        struct Item
        {
            std::atomic<std::int64_t> mUseCount;
            osg::Vec3f mAgentHalfExtents;
            TilePosition mChangedTile;
            RecastMeshData mRecastMeshData;
            std::unique_ptr<PreparedNavMeshData> mPreparedNavMeshData;
            std::size_t mSize;

            Item(const osg::Vec3f& agentHalfExtents, const TilePosition& changedTile,
                 RecastMeshData&& recastMeshData, std::size_t size)
                : mUseCount(0)
                , mAgentHalfExtents(agentHalfExtents)
                , mChangedTile(changedTile)
                , mRecastMeshData(std::move(recastMeshData))
                , mSize(size)
            {}
        };

        using ItemIterator = std::list<Item>::iterator;

        class Value
        {
        public:
            Value()
                : mOwner(nullptr), mIterator() {}

            Value(NavMeshTilesCache& owner, ItemIterator iterator)
                : mOwner(&owner), mIterator(iterator)
            {
            }

            Value(const Value& other) = delete;

            Value(Value&& other)
                : mOwner(other.mOwner), mIterator(other.mIterator)
            {
                other.mOwner = nullptr;
            }

            ~Value()
            {
                if (mOwner)
                    mOwner->releaseItem(mIterator);
            }

            Value& operator =(const Value& other) = delete;

            Value& operator =(Value&& other)
            {
                if (mOwner)
                    mOwner->releaseItem(mIterator);

                mOwner = other.mOwner;
                mIterator = other.mIterator;

                other.mOwner = nullptr;

                return *this;
            }

            const PreparedNavMeshData& get() const
            {
                return *mIterator->mPreparedNavMeshData;
            }

            operator bool() const
            {
                return mOwner;
            }

        private:
            NavMeshTilesCache* mOwner;
            ItemIterator mIterator;
        };

        struct Stats
        {
            std::size_t mNavMeshCacheSize;
            std::size_t mUsedNavMeshTiles;
            std::size_t mCachedNavMeshTiles;
            std::size_t mHitCount;
            std::size_t mGetCount;
        };

        NavMeshTilesCache(const std::size_t maxNavMeshDataSize);

        Value get(const osg::Vec3f& agentHalfExtents, const TilePosition& changedTile,
            const RecastMesh& recastMesh);

        Value set(const osg::Vec3f& agentHalfExtents, const TilePosition& changedTile,
            const RecastMesh& recastMesh, std::unique_ptr<PreparedNavMeshData>&& value);

        Stats getStats() const;

    private:
        mutable std::mutex mMutex;
        std::size_t mMaxNavMeshDataSize;
        std::size_t mUsedNavMeshDataSize;
        std::size_t mFreeNavMeshDataSize;
        std::size_t mHitCount;
        std::size_t mGetCount;
        std::list<Item> mBusyItems;
        std::list<Item> mFreeItems;
        std::map<std::tuple<osg::Vec3f, TilePosition, std::reference_wrapper<const RecastMeshData>>, ItemIterator, std::less<>> mValues;

        void removeLeastRecentlyUsed();

        void acquireItemUnsafe(ItemIterator iterator);

        void releaseItem(ItemIterator iterator);
    };

    void reportStats(const NavMeshTilesCache::Stats& stats, unsigned int frameNumber, osg::Stats& out);
}

#endif
