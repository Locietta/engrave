#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "global/types/bytearray.h"
#include "global/types/datetime.h"
#include "global/types/retval.h"
#include "io/ioenums.h"
#include "io/path.h"

namespace muse::io {
// Native, NativeFileSystem. Virtual ":/..." paths resolve under resourceRoot and
// are read-only. The hidden attribute cannot be set portably, so setAttribute is a no-op.
struct NativeFileSystem {
    enum class Attribute { HIDDEN };
    explicit NativeFileSystem(std::filesystem::path resource_root);
    ~NativeFileSystem();

    muse::Ret exists(const muse::io::PathT &path) const;
    muse::Ret remove(const muse::io::PathT &path, bool only_if_empty = false);
    muse::Ret clear(const muse::io::PathT &path);
    muse::Ret copy(const muse::io::PathT &src, const muse::io::PathT &dst, bool replace = false);
    muse::Ret move(const muse::io::PathT &src, const muse::io::PathT &dst, bool replace = false);

    muse::Ret make_path(const muse::io::PathT &path) const;
    muse::Ret make_link(const muse::io::PathT &target_path, const muse::io::PathT &link_path) const;

    muse::io::EntryType entry_type(const muse::io::PathT &path) const;

    muse::RetVal<uint64_t> file_size(const muse::io::PathT &path) const;
    muse::RetVal<uint64_t> available_space(const muse::io::PathT &path) const;

    muse::RetVal<muse::io::paths_t> scan_files(const muse::io::PathT &root_dir, const std::vector<std::string> &filters,
                                               muse::io::ScanMode mode = muse::io::ScanMode::FILES_IN_CURRENT_DIR_AND_SUBDIRS) const;

    void set_attribute(const muse::io::PathT &path, muse::io::NativeFileSystem::Attribute attribute) const;
    bool set_permissions_allowed_for_all(const muse::io::PathT &path) const;

    muse::RetVal<muse::ByteArray> read_file(const muse::io::PathT &file_path) const;
    muse::Ret read_file(const muse::io::PathT &file_path, muse::ByteArray &data) const;
    muse::Ret write_file(const muse::io::PathT &file_path, const muse::ByteArray &data);

    muse::RetVal<muse::io::StreamId> open_stream(const muse::io::PathT &file_path, muse::io::OpenMode mode);
    muse::Ret write_to_stream(muse::io::StreamId file_id, std::span<const uint8_t> data, uint64_t offset = muse::io::stream_pos_current);
    muse::Ret close_stream(muse::io::StreamId file_id);

    muse::io::PathT canonical_file_path(const muse::io::PathT &file_path) const;
    muse::io::PathT absolute_path(const muse::io::PathT &file_path) const;
    muse::io::PathT absolute_file_path(const muse::io::PathT &file_path) const;
    muse::io::PathT clean_path(const muse::io::PathT &file_path) const;
    bool is_sym_link(const muse::io::PathT &file_path) const;
    muse::io::PathT sym_link_target(const muse::io::PathT &file_path) const;
    muse::io::PathT temporary_directory_path() const;
    muse::DateTime birth_time(const muse::io::PathT &file_path) const;
    muse::DateTime last_modified(const muse::io::PathT &file_path) const;
    muse::Ret is_writable(const muse::io::PathT &file_path) const;

private:
    struct Stream;

    struct Target {
        std::filesystem::path path;
        std::string virtual_rel;
        bool valid = false;
        bool resource = false; // read-only: a ":/" path or resolves inside the resource root
        bool virtual_path = false;
    };

    Target resolve(const muse::io::PathT &path) const;
    bool is_inside_resource_root(const std::filesystem::path &path) const;
    bool is_protected_path(const std::filesystem::path &path) const;
    bool is_resource_ancestor(const std::filesystem::path &path) const;

    std::filesystem::path resource_root_;
    bool has_resource_root_ = false;
    mutable std::mutex streams_mutex_;
    std::unordered_map<muse::io::StreamId, std::unique_ptr<Stream>> streams_;
    muse::io::StreamId next_stream_id_ = 0;
};

NativeFileSystem *file_system();
void set_file_system(NativeFileSystem *file_system);
} // namespace muse::io
