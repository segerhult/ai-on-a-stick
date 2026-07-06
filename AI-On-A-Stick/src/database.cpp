#include "database.h"

#include <chrono>
#include <filesystem>
#include <stdexcept>

namespace {
namespace fs = std::filesystem;

std::int64_t to_unix_seconds(const fs::file_time_type& file_time) {
    using namespace std::chrono;
    const auto system_now = system_clock::now();
    const auto file_now = fs::file_time_type::clock::now();
    const auto adjusted = system_now + duration_cast<system_clock::duration>(file_time - file_now);
    return duration_cast<seconds>(adjusted.time_since_epoch()).count();
}
}  // namespace

Database::Database(const AppConfig& config) : config_(config) {}

Database::~Database() {
    if (db_ != nullptr) {
        sqlite3_close(db_);
    }
}

void Database::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    fs::create_directories(fs::path(config_.data_root) / "machines");

    const auto db_path = fs::path(config_.data_root) / "brain.db";
    if (sqlite3_open(db_path.string().c_str(), &db_) != SQLITE_OK) {
        throw std::runtime_error("Unable to open SQLite database: " + db_path.string());
    }

    execute(R"sql(
        CREATE TABLE IF NOT EXISTS machines (
            id TEXT PRIMARY KEY,
            hostname TEXT NOT NULL,
            operating_system TEXT NOT NULL,
            architecture TEXT NOT NULL,
            last_seen_at INTEGER NOT NULL DEFAULT (strftime('%s', 'now'))
        );
    )sql");

    execute(R"sql(
        CREATE TABLE IF NOT EXISTS files (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            machine_id TEXT NOT NULL,
            path TEXT NOT NULL,
            size_bytes INTEGER NOT NULL,
            modified_at INTEGER NOT NULL,
            indexed_at INTEGER NOT NULL DEFAULT (strftime('%s', 'now')),
            UNIQUE(machine_id, path)
        );
    )sql");
}

void Database::upsert_machine(const MachineProfile& machine) {
    std::lock_guard<std::mutex> lock(mutex_);
    static constexpr const char* sql =
        "INSERT INTO machines (id, hostname, operating_system, architecture, last_seen_at) "
        "VALUES (?, ?, ?, ?, strftime('%s', 'now')) "
        "ON CONFLICT(id) DO UPDATE SET "
        "hostname = excluded.hostname, "
        "operating_system = excluded.operating_system, "
        "architecture = excluded.architecture, "
        "last_seen_at = strftime('%s', 'now');";

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &statement, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db_));
    }

    sqlite3_bind_text(statement, 1, machine.id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, machine.hostname.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, machine.operating_system.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 4, machine.architecture.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(statement) != SQLITE_DONE) {
        sqlite3_finalize(statement);
        throw std::runtime_error(sqlite3_errmsg(db_));
    }

    sqlite3_finalize(statement);
}

void Database::index_scan_paths(const MachineProfile& machine, const std::vector<std::string>& scan_paths) {
    for (const auto& root : scan_paths) {
        const fs::path root_path(root);
        if (!fs::exists(root_path)) {
            continue;
        }

        for (const auto& entry : fs::recursive_directory_iterator(root_path)) {
            if (!entry.is_regular_file()) {
                continue;
            }

            const auto size = entry.file_size();
            const auto modified_at = to_unix_seconds(entry.last_write_time());
            insert_file(machine, entry.path(), size, modified_at);
        }
    }
}

std::vector<FileRecord> Database::recent_files(int limit) const {
    std::lock_guard<std::mutex> lock(mutex_);
    static constexpr const char* sql =
        "SELECT path, machine_id, size_bytes, modified_at "
        "FROM files ORDER BY indexed_at DESC LIMIT ?;";

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &statement, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db_));
    }

    sqlite3_bind_int(statement, 1, limit);

    std::vector<FileRecord> rows;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        FileRecord row;
        row.path = reinterpret_cast<const char*>(sqlite3_column_text(statement, 0));
        row.machine_id = reinterpret_cast<const char*>(sqlite3_column_text(statement, 1));
        row.size = static_cast<std::uint64_t>(sqlite3_column_int64(statement, 2));
        row.modified_at = sqlite3_column_int64(statement, 3);
        rows.push_back(std::move(row));
    }

    sqlite3_finalize(statement);
    return rows;
}

std::size_t Database::file_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    static constexpr const char* sql = "SELECT COUNT(*) FROM files;";
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &statement, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db_));
    }

    std::size_t count = 0;
    if (sqlite3_step(statement) == SQLITE_ROW) {
        count = static_cast<std::size_t>(sqlite3_column_int64(statement, 0));
    }

    sqlite3_finalize(statement);
    return count;
}

void Database::execute(const std::string& sql) const {
    char* error = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &error) != SQLITE_OK) {
        std::string message = error != nullptr ? error : "Unknown SQLite error";
        sqlite3_free(error);
        throw std::runtime_error(message);
    }
}

void Database::insert_file(const MachineProfile& machine, const std::string& file_path, std::uint64_t size,
                           std::int64_t modified_at) {
    std::lock_guard<std::mutex> lock(mutex_);
    static constexpr const char* sql =
        "INSERT INTO files (machine_id, path, size_bytes, modified_at, indexed_at) "
        "VALUES (?, ?, ?, ?, strftime('%s', 'now')) "
        "ON CONFLICT(machine_id, path) DO UPDATE SET "
        "size_bytes = excluded.size_bytes, "
        "modified_at = excluded.modified_at, "
        "indexed_at = strftime('%s', 'now');";

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &statement, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db_));
    }

    const std::string normalized_path = fs::absolute(fs::path(file_path)).string();
    sqlite3_bind_text(statement, 1, machine.id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, normalized_path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(statement, 3, static_cast<sqlite3_int64>(size));
    sqlite3_bind_int64(statement, 4, static_cast<sqlite3_int64>(modified_at));

    if (sqlite3_step(statement) != SQLITE_DONE) {
        sqlite3_finalize(statement);
        throw std::runtime_error(sqlite3_errmsg(db_));
    }

    sqlite3_finalize(statement);
}
