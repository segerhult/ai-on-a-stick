#pragma once

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

#include <sqlite3.h>

#include "config.h"
#include "machine.h"

struct FileRecord {
    std::string path;
    std::string machine_id;
    std::uint64_t size = 0;
    std::int64_t modified_at = 0;
};

class Database {
  public:
    explicit Database(const AppConfig& config);
    ~Database();

    void initialize();
    void upsert_machine(const MachineProfile& machine);
    void index_scan_paths(const MachineProfile& machine, const std::vector<std::string>& scan_paths);
    std::vector<FileRecord> recent_files(int limit) const;
    std::size_t file_count() const;

  private:
    void execute(const std::string& sql) const;
    void insert_file(const MachineProfile& machine, const std::filesystem::path& file_path, std::uint64_t size,
                     std::int64_t modified_at);

    AppConfig config_;
    sqlite3* db_ = nullptr;
    mutable std::mutex mutex_;
};
