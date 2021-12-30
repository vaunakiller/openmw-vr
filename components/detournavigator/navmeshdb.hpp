#ifndef OPENMW_COMPONENTS_DETOURNAVIGATOR_NAVMESHDB_H
#define OPENMW_COMPONENTS_DETOURNAVIGATOR_NAVMESHDB_H

#include "tileposition.hpp"

#include <components/sqlite3/db.hpp>
#include <components/sqlite3/statement.hpp>
#include <components/sqlite3/transaction.hpp>
#include <components/sqlite3/types.hpp>

#include <boost/serialization/strong_typedef.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>
#include <memory>

struct sqlite3;
struct sqlite3_stmt;

namespace DetourNavigator
{
    BOOST_STRONG_TYPEDEF(std::int64_t, TileId)
    BOOST_STRONG_TYPEDEF(std::int64_t, TileRevision)
    BOOST_STRONG_TYPEDEF(std::int64_t, TileVersion)
    BOOST_STRONG_TYPEDEF(std::int64_t, ShapeId)

    struct Tile
    {
        TileId mTileId;
        TileVersion mVersion;
    };

    struct TileData
    {
        TileId mTileId;
        TileVersion mVersion;
        std::vector<std::byte> mData;
    };

    enum class ShapeType
    {
        Collision = 1,
        Avoid = 2,
    };

    std::ostream& operator<<(std::ostream& stream, ShapeType value);

    namespace DbQueries
    {
        struct GetMaxTileId
        {
            static std::string_view text() noexcept;
            static void bind(sqlite3&, sqlite3_stmt&) {}
        };

        struct FindTile
        {
            static std::string_view text() noexcept;
            static void bind(sqlite3& db, sqlite3_stmt& statement, const std::string& worldspace,
                const TilePosition& tilePosition, const std::vector<std::byte>& input);
        };

        struct GetTileData
        {
            static std::string_view text() noexcept;
            static void bind(sqlite3& db, sqlite3_stmt& statement, const std::string& worldspace,
                const TilePosition& tilePosition, const std::vector<std::byte>& input);
        };

        struct InsertTile
        {
            static std::string_view text() noexcept;
            static void bind(sqlite3& db, sqlite3_stmt& statement, TileId tileId, const std::string& worldspace,
                const TilePosition& tilePosition, TileVersion version, const std::vector<std::byte>& input,
                const std::vector<std::byte>& data);
        };

        struct UpdateTile
        {
            static std::string_view text() noexcept;
            static void bind(sqlite3& db, sqlite3_stmt& statement, TileId tileId, TileVersion version,
                const std::vector<std::byte>& data);
        };

        struct GetMaxShapeId
        {
            static std::string_view text() noexcept;
            static void bind(sqlite3&, sqlite3_stmt&) {}
        };

        struct FindShapeId
        {
            static std::string_view text() noexcept;
            static void bind(sqlite3& db, sqlite3_stmt& statement, const std::string& name,
                ShapeType type, const Sqlite3::ConstBlob& hash);
        };

        struct InsertShape
        {
            static std::string_view text() noexcept;
            static void bind(sqlite3& db, sqlite3_stmt& statement, ShapeId shapeId, const std::string& name,
                ShapeType type, const Sqlite3::ConstBlob& hash);
        };
    }

    class NavMeshDb
    {
    public:
        explicit NavMeshDb(std::string_view path);

        Sqlite3::Transaction startTransaction();

        TileId getMaxTileId();

        std::optional<Tile> findTile(const std::string& worldspace,
            const TilePosition& tilePosition, const std::vector<std::byte>& input);

        std::optional<TileData> getTileData(const std::string& worldspace,
            const TilePosition& tilePosition, const std::vector<std::byte>& input);

        int insertTile(TileId tileId, const std::string& worldspace, const TilePosition& tilePosition,
            TileVersion version, const std::vector<std::byte>& input, const std::vector<std::byte>& data);

        int updateTile(TileId tileId, TileVersion version, const std::vector<std::byte>& data);

        ShapeId getMaxShapeId();

        std::optional<ShapeId> findShapeId(const std::string& name, ShapeType type, const Sqlite3::ConstBlob& hash);

        int insertShape(ShapeId shapeId, const std::string& name, ShapeType type, const Sqlite3::ConstBlob& hash);

    private:
        Sqlite3::Db mDb;
        Sqlite3::Statement<DbQueries::GetMaxTileId> mGetMaxTileId;
        Sqlite3::Statement<DbQueries::FindTile> mFindTile;
        Sqlite3::Statement<DbQueries::GetTileData> mGetTileData;
        Sqlite3::Statement<DbQueries::InsertTile> mInsertTile;
        Sqlite3::Statement<DbQueries::UpdateTile> mUpdateTile;
        Sqlite3::Statement<DbQueries::GetMaxShapeId> mGetMaxShapeId;
        Sqlite3::Statement<DbQueries::FindShapeId> mFindShapeId;
        Sqlite3::Statement<DbQueries::InsertShape> mInsertShape;
    };
}

#endif
