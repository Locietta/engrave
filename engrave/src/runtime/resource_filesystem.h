#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "io/ifilesystem.h"

namespace music {
// Native, IFileSystem. Virtual ":/..." paths resolve under resourceRoot and
// are read-only. The hidden attribute cannot be set portably, so setAttribute is a no-op.
struct NativeFileSystem final : muse::io::IFileSystem {
    explicit NativeFileSystem(std::filesystem::path resource_root);
    ~NativeFileSystem() override;

    muse::Ret exists(const muse::io::PathT &path) const override;
    muse::Ret remove(const muse::io::PathT &path, bool only_if_empty = false) override;
    muse::Ret clear(const muse::io::PathT &path) override;
    muse::Ret copy(const muse::io::PathT &src, const muse::io::PathT &dst, bool replace = false) override;
    muse::Ret move(const muse::io::PathT &src, const muse::io::PathT &dst, bool replace = false) override;

    muse::Ret make_path(const muse::io::PathT &path) const override;
    muse::Ret make_link(const muse::io::PathT &target_path, const muse::io::PathT &link_path) const override;

    muse::io::EntryType entry_type(const muse::io::PathT &path) const override;

    muse::RetVal<uint64_t> file_size(const muse::io::PathT &path) const override;
    muse::RetVal<uint64_t> available_space(const muse::io::PathT &path) const override;

    muse::RetVal<muse::io::paths_t>
    scan_files(const muse::io::PathT &root_dir, const std::vector<std::string> &filters,
               muse::io::ScanMode mode = muse::io::ScanMode::FILES_IN_CURRENT_DIR_AND_SUBDIRS) const override;

    void set_attribute(const muse::io::PathT &path, muse::io::IFileSystem::Attribute attribute) const override;
    bool set_permissions_allowed_for_all(const muse::io::PathT &path) const override;

    muse::RetVal<muse::ByteArray> read_file(const muse::io::PathT &file_path) const override;
    muse::Ret read_file(const muse::io::PathT &file_path, muse::ByteArray &data) const override;
    muse::Ret write_file(const muse::io::PathT &file_path, const muse::ByteArray &data) override;

    muse::RetVal<muse::io::StreamId> open_stream(const muse::io::PathT &file_path, muse::io::OpenMode mode) override;
    muse::Ret write_to_stream(muse::io::StreamId file_id, const muse::ByteArray &data,
                              uint64_t offset = muse::io::stream_pos_current) override;
    muse::Ret close_stream(muse::io::StreamId file_id) override;

    muse::io::PathT canonical_file_path(const muse::io::PathT &file_path) const override;
    muse::io::PathT absolute_path(const muse::io::PathT &file_path) const override;
    muse::io::PathT absolute_file_path(const muse::io::PathT &file_path) const override;
    muse::io::PathT clean_path(const muse::io::PathT &file_path) const override;
    bool is_sym_link(const muse::io::PathT &file_path) const override;
    muse::io::PathT sym_link_target(const muse::io::PathT &file_path) const override;
    muse::io::PathT temporary_directory_path() const override;
    muse::DateTime birth_time(const muse::io::PathT &file_path) const override;
    muse::DateTime last_modified(const muse::io::PathT &file_path) const override;
    muse::Ret is_writable(const muse::io::PathT &file_path) const override;

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
} // namespace music
