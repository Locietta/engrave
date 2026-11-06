#include "noqt_filesystem.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <exception>
#include <fstream>
#include <limits>
#include <new>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "io/ioretcodes.h"

using namespace muse;
using namespace muse::io;
using music::NativeFileSystem;
namespace fs = std::filesystem;

namespace {
constexpr uint64_t k_max_read_file_size = 256ull * 1024ull * 1024ull;

std::string path_to_utf8(const fs::path &path, bool generic = true) {
    const std::u8string utf8 = generic ? path.generic_u8string() : path.u8string();
    return std::string(reinterpret_cast<const char *>(utf8.data()), utf8.size());
}

bool has_embedded_nul(const std::string &value) { return value.find('\0') != std::string::npos; }

Ret failure(io::Err code, std::string text) { return Ret(static_cast<int>(code), std::move(text)); }

Ret fs_failure(io::Err code, const char *operation, const fs::path &path, const std::error_code &ec) {
    std::string text = std::string(operation) + ": " + path_to_utf8(path);
    if (ec) {
        text += ": ";
        text += ec.message();
    }
    return failure(code, std::move(text));
}

Ret read_only_failure(io::Err code, const char *operation, const io::PathT &path) {
    return failure(code, std::string(operation) + ": protected by the read-only resource root: " + path.to_std_string());
}

char ascii_lower(char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; }

bool same_component(const std::string &lhs, const std::string &rhs) {
#ifdef _WIN32
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (size_t i = 0; i < lhs.size(); ++i) {
        if (ascii_lower(lhs[i]) != ascii_lower(rhs[i])) {
            return false;
        }
    }
    return true;
#else
    return lhs == rhs;
#endif
}

bool is_path_prefix(const fs::path &prefix, const fs::path &path) {
    auto it = path.begin();
    for (const fs::path &component : prefix) {
        if (it == path.end() || !same_component(path_to_utf8(component), path_to_utf8(*it))) {
            return false;
        }
        ++it;
    }
    return true;
}

bool paths_equal(const fs::path &lhs, const fs::path &rhs) { return is_path_prefix(lhs, rhs) && is_path_prefix(rhs, lhs); }

bool is_strict_path_prefix(const fs::path &prefix, const fs::path &path) {
    return is_path_prefix(prefix, path) && !paths_equal(prefix, path);
}

fs::path absolute_normalized(const fs::path &path) {
    std::error_code ec;
    fs::path absolute = fs::absolute(path, ec);
    if (ec) {
        absolute = path;
    }
    return absolute.lexically_normal();
}

io::PathT append_name(const io::PathT &directory, const std::string &name) {
    std::string path = directory.to_std_string();
    while (path.size() > 1 && (path.back() == '/' || path.back() == '\\')) {
        path.pop_back();
    }
    if (path.empty()) {
        path = ".";
    }
    if (path.back() != '/') {
        path.push_back('/');
    }
    path += name;
    return io::PathT(std::move(path));
}

// QDir::cleanPath equivalent: '/'-separated, redundant separators and '.' dropped,
// ".." resolved lexically (never above the root, drive or UNC share), symlinks
// untouched, no trailing separator. Resource paths keep their ":/" form.
std::string clean_path_string(std::string path) {
#ifdef _WIN32
    for (char &c : path) {
        if (c == '\\') {
            c = '/';
        }
    }
#endif
    // Resource marker (":/x") or drive ("C:/x") prefix, kept verbatim.
    std::string drive;
    if (!path.empty() && path[0] == ':') {
        drive = ":";
        path.erase(0, 1);
    } else if (path.size() >= 2 && path[1] == ':') {
        drive = path.substr(0, 2);
        path.erase(0, 2);
    }

    std::size_t index = 0;
    std::size_t leading_slashes = 0;
    while (index < path.size() && path[index] == '/') {
        ++index;
        ++leading_slashes;
    }
    const bool absolute = leading_slashes > 0;
    // A UNC root ("//host/share") keeps two slashes and cannot be cancelled by "..".
    const bool unc = absolute && leading_slashes > 1 && drive.empty();

    std::vector<std::string> components;
    for (std::size_t split = index; split <= path.size();) {
        const std::size_t slash = path.find('/', split);
        const std::string component = path.substr(split, slash == std::string::npos ? std::string::npos : slash - split);
        if (component.empty() || component == ".") {
            // Redundant separator or current directory.
        } else if (component == "..") {
            const std::size_t protected_count = unc ? 2 : 0;
            if (components.size() > protected_count && components.back() != "..") {
                components.pop_back();
            } else if (!absolute) {
                // A relative path cannot be collapsed further; keep the "..".
                components.push_back(component);
            }
            // Above an absolute root there is nothing left to drop.
        } else {
            components.push_back(component);
        }
        if (slash == std::string::npos) {
            break;
        }
        split = slash + 1;
    }

    std::string result = drive;
    result += unc ? "//" : (absolute ? "/" : "");
    for (std::size_t i = 0; i < components.size(); ++i) {
        if (!result.empty() && result.back() != '/' && (absolute || i > 0)) {
            result.push_back('/');
        }
        result += components[i];
    }
    return result;
}

bool unsafe_virtual_component(const fs::path &component) {
    std::string name = path_to_utf8(component);
    if (name.empty()) {
        return false; // separator or trailing slash
    }
#ifdef _WIN32
    // Win32 strips trailing spaces/dots before resolving a component, so ".. " is also "..".
    const size_t last = name.find_last_not_of(" .");
    if (last == std::string::npos) {
        return true;
    }
    name.resize(last + 1);
#endif
    return name == "..";
}

bool glob_char_equal(char lhs, char rhs) {
#ifdef _WIN32
    return ascii_lower(lhs) == ascii_lower(rhs);
#else
    return lhs == rhs;
#endif
}

bool glob_match(std::string_view name, std::string_view pattern) {
    size_t name_index = 0;
    size_t pattern_index = 0;
    size_t star_index = std::string_view::npos;
    size_t star_name_index = 0;

    while (name_index < name.size()) {
        if (pattern_index < pattern.size() && pattern[pattern_index] == '*') {
            star_index = pattern_index++;
            star_name_index = name_index;
        } else if (pattern_index < pattern.size() &&
                   (pattern[pattern_index] == '?' || glob_char_equal(pattern[pattern_index], name[name_index]))) {
            ++pattern_index;
            ++name_index;
        } else if (star_index != std::string_view::npos) {
            pattern_index = star_index + 1;
            name_index = ++star_name_index;
        } else {
            return false;
        }
    }

    while (pattern_index < pattern.size() && pattern[pattern_index] == '*') {
        ++pattern_index;
    }
    return pattern_index == pattern.size();
}

bool matches_any_filter(const std::string &name, const std::vector<std::string> &filters) {
    if (filters.empty()) {
        return true;
    }
    for (const std::string &filter : filters) {
        if (glob_match(name, filter)) {
            return true;
        }
    }
    return false;
}

DateTime date_time_from_file_time(const fs::file_time_type &file_time) {
    using namespace std::chrono;
    const auto system_time = time_point_cast<system_clock::duration>(file_time - fs::file_time_type::clock::now() + system_clock::now());
    const std::time_t seconds = system_clock::to_time_t(system_time);

    std::tm tm = {};
#ifdef _WIN32
    if (localtime_s(&tm, &seconds) != 0) {
        return {};
    }
#else
    if (localtime_r(&seconds, &tm) == nullptr) {
        return {};
    }
#endif
    return DateTime(Date(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday), Time(tm.tm_hour, tm.tm_min, tm.tm_sec));
}

Ret copy_entry(const fs::path &source, const fs::path &destination) {
    std::error_code ec;
    const fs::file_status source_status = fs::symlink_status(source, ec);
    if (ec) {
        return fs_failure(io::Err::FS_COPY_ERROR, "Failed to stat copy source", source, ec);
    }

    switch (source_status.type()) {
        case fs::file_type::regular: fs::copy_file(source, destination, fs::copy_options::none, ec); break;
        case fs::file_type::directory: {
            fs::create_directories(destination, ec);
            if (ec) {
                return fs_failure(io::Err::FS_COPY_ERROR, "Failed to create copy destination", destination, ec);
            }
            if (!fs::is_directory(destination, ec)) {
                return fs_failure(io::Err::FS_COPY_ERROR, "Copy destination is not a directory", destination, ec);
            }
            fs::directory_iterator iterator(source, ec);
            const fs::directory_iterator end;
            if (ec) {
                return fs_failure(io::Err::FS_COPY_ERROR, "Failed to list copy source", source, ec);
            }
            for (; iterator != end; iterator.increment(ec)) {
                if (ec) {
                    break;
                }
                const Ret ret = copy_entry(iterator->path(), destination / iterator->path().filename());
                if (!ret) {
                    return ret;
                }
            }
            break;
        }
        case fs::file_type::symlink: fs::copy_symlink(source, destination, ec); break;
        case fs::file_type::not_found: return failure(io::Err::FS_NOT_EXIST, "Copy source does not exist: " + path_to_utf8(source));
        default: return failure(io::Err::FS_COPY_ERROR, "Unsupported copy source type: " + path_to_utf8(source));
    }

    if (ec) {
        return fs_failure(io::Err::FS_COPY_ERROR, "Failed to copy", source, ec);
    }
    return make_ok();
}

void scan_directory(const fs::path &directory, const io::PathT &output_directory, bool recursive, bool want_files, bool want_folders,
                    const std::vector<std::string> &filters, io::paths_t &output, Ret &result) {
    std::error_code ec;
    fs::directory_iterator iterator(directory, ec);
    const fs::directory_iterator end;
    if (ec) {
        result = fs_failure(io::Err::FS_READ_ERROR, "Failed to list directory", directory, ec);
        return;
    }

    for (; iterator != end; iterator.increment(ec)) {
        if (ec) {
            result = fs_failure(io::Err::FS_READ_ERROR, "Failed to list directory", directory, ec);
            return;
        }

        const fs::directory_entry &entry = *iterator;
        std::error_code status_error;
        const fs::file_status link_status = entry.symlink_status(status_error);
        if (status_error) {
            result = fs_failure(io::Err::FS_READ_ERROR, "Failed to stat directory entry", entry.path(), status_error);
            return;
        }

        const bool is_link = link_status.type() == fs::file_type::symlink;
        fs::file_status status = link_status;
        if (is_link) {
            status = entry.status(status_error);
            if (status_error) {
                continue; // dangling symlink
            }
        }

        const std::string name = path_to_utf8(entry.path().filename());
        const bool matched = matches_any_filter(name, filters);
        if (status.type() == fs::file_type::directory) {
            if (want_folders && matched) {
                output.push_back(append_name(output_directory, name));
            }
            if (recursive && !is_link) {
                scan_directory(entry.path(), append_name(output_directory, name), true, want_files, false, filters, output, result);
                if (!result) {
                    return;
                }
            }
        } else if (status.type() == fs::file_type::regular && want_files && matched) {
            output.push_back(append_name(output_directory, name));
        }
    }

    if (ec) {
        result = fs_failure(io::Err::FS_READ_ERROR, "Failed to list directory", directory, ec);
    }
}

fs::path resolved_path(const fs::path &path) {
    std::error_code ec;
    const fs::path canonical = fs::weakly_canonical(path, ec);
    return ec ? absolute_normalized(path) : canonical.lexically_normal();
}

bool paths_overlap(const fs::path &lhs, const fs::path &rhs) {
    const fs::path first = resolved_path(lhs);
    const fs::path second = resolved_path(rhs);
    return is_path_prefix(first, second) || is_path_prefix(second, first);
}

bool remove_once_with_retry(const fs::path &path, std::error_code &ec) {
    ec.clear();
    fs::remove(path, ec);
    if (!ec) {
        return true;
    }
    if (ec != std::errc::permission_denied) {
        return false;
    }

    // Windows read-only attribute (and some POSIX ACLs) prevent deletion; match upstream retry.
    std::error_code permission_error;
    fs::permissions(path, fs::perms::owner_write, fs::perm_options::add, permission_error);
    if (permission_error) {
        return false;
    }
    ec.clear();
    fs::remove(path, ec);
    return !ec;
}

bool remove_tree(const fs::path &path, std::error_code &ec) {
    fs::directory_iterator iterator(path, ec);
    const fs::directory_iterator end;
    if (ec) {
        return false;
    }
    for (; iterator != end; iterator.increment(ec)) {
        if (ec) {
            return false;
        }

        std::error_code entry_error;
        const fs::file_status status = iterator->symlink_status(entry_error);
        if (entry_error) {
            ec = entry_error;
            return false;
        }
        if (status.type() == fs::file_type::directory) {
            if (!remove_tree(iterator->path(), ec)) {
                return false;
            }
        } else if (!remove_once_with_retry(iterator->path(), ec)) {
            return false;
        }
    }
    if (ec) {
        return false;
    }
    return remove_once_with_retry(path, ec);
}
} // namespace

struct NativeFileSystem::Stream {
    std::ofstream file;
};

NativeFileSystem::NativeFileSystem(fs::path resource_root) {
    if (resource_root.empty()) {
        return;
    }

    std::error_code ec;
    fs::path absolute = fs::absolute(resource_root, ec);
    if (ec) {
        absolute = resource_root;
    }
    const fs::path canonical = fs::weakly_canonical(absolute, ec);
    if (!ec) {
        absolute = canonical;
    }
    resource_root_ = absolute.lexically_normal();
    // A trailing separator would survive lexically_normal and break component-wise containment.
    while (resource_root_.has_relative_path() && resource_root_.filename().empty()) {
        resource_root_ = resource_root_.parent_path();
    }
    if (!resource_root_.is_absolute()) {
        resource_root_.clear();
        return;
    }
    has_resource_root_ = true;
}

NativeFileSystem::~NativeFileSystem() {
    std::lock_guard<std::mutex> lock(streams_mutex_);
    for (auto &item : streams_) {
        if (item.second && item.second->file.is_open()) {
            item.second->file.flush();
            item.second->file.close();
        }
    }
    streams_.clear();
}

NativeFileSystem::Target NativeFileSystem::resolve(const io::PathT &path) const {
    try {
        Target target;
        const std::string &raw = path.to_std_string();
        if (has_embedded_nul(raw)) {
            return target;
        }

        if (raw.size() >= 2 && raw[0] == ':' && raw[1] == '/') {
            if (!has_resource_root_) {
                return target;
            }

            // Qt cleans resource paths, so ":/" and "://" name the same root (fret.cpp uses "://data/...").
            const std::size_t start = raw.find_first_not_of('/', 1);
            fs::path relative = fs::u8path(start == std::string::npos ? std::string() : raw.substr(start));
            if (relative.is_absolute() || relative.has_root_name() || relative.has_root_directory()) {
                return target;
            }
            for (const fs::path &component : relative) {
                if (unsafe_virtual_component(component) || component.has_root_name() || component.has_root_directory()) {
                    return target;
                }
            }

            relative = relative.lexically_normal();
            if (relative == fs::path(".")) {
                relative.clear();
            }
            // Lexical containment: every component is below resourceRoot and ".." is rejected above.
            target.path = (resource_root_ / relative).lexically_normal();
            if (!is_path_prefix(resource_root_, target.path)) {
                return target;
            }
            target.virtual_rel = path_to_utf8(relative);
            target.valid = true;
            target.resource = true;
            target.virtual_path = true;
            return target;
        }

        target.path = fs::u8path(raw);
        target.valid = true;
        target.resource = is_protected_path(target.path);
        return target;
    } catch (const std::exception &) {
        return Target{};
    }
}

bool NativeFileSystem::is_inside_resource_root(const fs::path &path) const {
    if (!has_resource_root_) {
        return false;
    }

    if (is_path_prefix(resource_root_, absolute_normalized(path))) {
        return true;
    }

    std::error_code ec;
    const fs::path canonical = fs::weakly_canonical(path, ec);
    return !ec && is_path_prefix(resource_root_, canonical.lexically_normal());
}

bool NativeFileSystem::is_protected_path(const fs::path &path) const {
    if (is_inside_resource_root(path)) {
        return true;
    }

    // weakly_canonical cannot resolve a dangling final symlink, so follow it explicitly.
    fs::path current = path;
    for (int depth = 0; depth < 16; ++depth) {
        std::error_code ec;
        const fs::file_status status = fs::symlink_status(current, ec);
        if (ec || status.type() != fs::file_type::symlink) {
            return false;
        }
        fs::path target = fs::read_symlink(current, ec);
        if (ec) {
            return false;
        }
        if (target.is_relative()) {
            target = current.parent_path() / target;
        }
        current = target.lexically_normal();
        if (is_inside_resource_root(current)) {
            return true;
        }
    }
    return false;
}

bool NativeFileSystem::is_resource_ancestor(const fs::path &path) const {
    if (!has_resource_root_) {
        return false;
    }

    if (is_strict_path_prefix(absolute_normalized(path), resource_root_)) {
        return true;
    }

    std::error_code ec;
    const fs::path canonical = fs::weakly_canonical(path, ec);
    return !ec && is_strict_path_prefix(canonical.lexically_normal(), resource_root_);
}

Ret NativeFileSystem::exists(const io::PathT &path) const {
    const Target target = resolve(path);
    if (!target.valid) {
        return failure(io::Err::FS_NOT_EXIST, "Invalid path: " + path.to_std_string());
    }

    std::error_code ec;
    if (fs::exists(target.path, ec)) {
        return make_ok();
    }
    if (ec && ec != std::errc::no_such_file_or_directory) {
        return fs_failure(io::Err::FS_NOT_EXIST, "Failed to stat", target.path, ec);
    }
    return make_ret(io::Err::FS_NOT_EXIST);
}

Ret NativeFileSystem::remove(const io::PathT &path, bool only_if_empty) {
    const Target target = resolve(path);
    if (!target.valid) {
        return failure(io::Err::FS_REMOVE_ERROR, "Invalid path: " + path.to_std_string());
    }
    if (target.resource || is_resource_ancestor(target.path)) {
        return read_only_failure(io::Err::FS_REMOVE_ERROR, "Cannot remove", path);
    }

    std::error_code ec;
    const fs::file_status status = fs::symlink_status(target.path, ec);
    if (ec) {
        if (ec == std::errc::no_such_file_or_directory) {
            return make_ok();
        }
        return fs_failure(io::Err::FS_REMOVE_ERROR, "Failed to stat", target.path, ec);
    }
    if (status.type() == fs::file_type::not_found) {
        return make_ok();
    }

    bool removed = false;
    if (status.type() == fs::file_type::directory) {
        if (only_if_empty) {
            if (!fs::is_empty(target.path, ec)) {
                if (ec) {
                    return fs_failure(io::Err::FS_REMOVE_ERROR, "Failed to inspect directory", target.path, ec);
                }
                return make_ret(io::Err::FS_DIR_NOT_EMPTY_ERROR);
            }
            removed = remove_once_with_retry(target.path, ec);
        } else {
            removed = remove_tree(target.path, ec);
        }
    } else {
        removed = remove_once_with_retry(target.path, ec);
    }

    if (!removed) {
        return fs_failure(io::Err::FS_REMOVE_ERROR, "Failed to remove", target.path, ec);
    }
    return make_ok();
}

Ret NativeFileSystem::clear(const io::PathT &path) {
    const Target target = resolve(path);
    if (!target.valid) {
        return failure(io::Err::FS_CLEAR_ERROR, "Invalid path: " + path.to_std_string());
    }
    if (target.resource || is_resource_ancestor(target.path)) {
        return read_only_failure(io::Err::FS_CLEAR_ERROR, "Cannot clear", path);
    }

    std::error_code ec;
    const fs::file_status status = fs::symlink_status(target.path, ec);
    if (ec) {
        if (ec == std::errc::no_such_file_or_directory) {
            return make_ok();
        }
        return fs_failure(io::Err::FS_CLEAR_ERROR, "Failed to stat", target.path, ec);
    }
    if (status.type() == fs::file_type::not_found) {
        return make_ok();
    }
    if (status.type() == fs::file_type::symlink) {
        return failure(io::Err::FS_CLEAR_ERROR, "Refusing to clear a symlinked directory: " + path_to_utf8(target.path));
    }
    if (status.type() != fs::file_type::directory) {
        return failure(io::Err::FS_CLEAR_ERROR, "Not a directory: " + path_to_utf8(target.path));
    }

    fs::directory_iterator iterator(target.path, ec);
    const fs::directory_iterator end;
    if (ec) {
        return fs_failure(io::Err::FS_CLEAR_ERROR, "Failed to list directory", target.path, ec);
    }

    for (; iterator != end; iterator.increment(ec)) {
        if (ec) {
            return fs_failure(io::Err::FS_CLEAR_ERROR, "Failed to list directory", target.path, ec);
        }

        const fs::directory_entry &entry = *iterator;
        std::error_code entry_error;
        const fs::file_status entry_status = entry.symlink_status(entry_error);
        if (entry_error) {
            return fs_failure(io::Err::FS_CLEAR_ERROR, "Failed to stat directory entry", entry.path(), entry_error);
        }

        bool removed = false;
        if (entry_status.type() == fs::file_type::directory) {
            removed = remove_tree(entry.path(), entry_error);
        } else {
            removed = remove_once_with_retry(entry.path(), entry_error);
        }
        if (!removed) {
            return fs_failure(io::Err::FS_CLEAR_ERROR, "Failed to remove directory entry", entry.path(), entry_error);
        }
    }
    if (ec) {
        return fs_failure(io::Err::FS_CLEAR_ERROR, "Failed to list directory", target.path, ec);
    }

    return make_ok();
}

Ret NativeFileSystem::copy(const io::PathT &src, const io::PathT &dst, bool replace) {
    const Target source = resolve(src);
    const Target destination = resolve(dst);
    if (!source.valid) {
        return failure(io::Err::FS_NOT_EXIST, "Invalid copy source: " + src.to_std_string());
    }
    if (!destination.valid) {
        return failure(io::Err::FS_COPY_ERROR, "Invalid copy destination: " + dst.to_std_string());
    }
    if (destination.resource || (replace && is_resource_ancestor(destination.path))) {
        return read_only_failure(io::Err::FS_COPY_ERROR, "Cannot copy to", dst);
    }

    std::error_code ec;
    if (!fs::exists(source.path, ec)) {
        if (ec && ec != std::errc::no_such_file_or_directory) {
            return fs_failure(io::Err::FS_COPY_ERROR, "Failed to stat copy source", source.path, ec);
        }
        return make_ret(io::Err::FS_NOT_EXIST);
    }

    ec.clear();
    const bool destination_exists = fs::exists(destination.path, ec);
    if (destination_exists && !replace) {
        return make_ret(io::Err::FS_ALREADY_EXISTS);
    }
    if (paths_overlap(source.path, destination.path)) {
        return failure(io::Err::FS_COPY_ERROR,
                       "Copy source and destination overlap: " + src.to_std_string() + " -> " + dst.to_std_string());
    }
    if (destination_exists) {
        const Ret removed = remove(dst);
        if (!removed) {
            return removed;
        }
    }

    return copy_entry(source.path, destination.path);
}

Ret NativeFileSystem::move(const io::PathT &src, const io::PathT &dst, bool replace) {
    const Target source = resolve(src);
    const Target destination = resolve(dst);
    if (!source.valid) {
        return failure(io::Err::FS_NOT_EXIST, "Invalid move source: " + src.to_std_string());
    }
    if (!destination.valid) {
        return failure(io::Err::FS_MOVE_ERRORS, "Invalid move destination: " + dst.to_std_string());
    }
    if (source.resource || is_resource_ancestor(source.path)) {
        return read_only_failure(io::Err::FS_MOVE_ERRORS, "Cannot move", src);
    }
    if (destination.resource || (replace && is_resource_ancestor(destination.path))) {
        return read_only_failure(io::Err::FS_MOVE_ERRORS, "Cannot move to", dst);
    }

    std::error_code ec;
    if (!fs::exists(source.path, ec)) {
        if (ec && ec != std::errc::no_such_file_or_directory) {
            return fs_failure(io::Err::FS_MOVE_ERRORS, "Failed to stat move source", source.path, ec);
        }
        return make_ret(io::Err::FS_NOT_EXIST);
    }

    ec.clear();
    const bool destination_exists = fs::exists(destination.path, ec);
    if (destination_exists && !replace) {
        return make_ret(io::Err::FS_ALREADY_EXISTS);
    }
    if (paths_overlap(source.path, destination.path)) {
        return failure(io::Err::FS_MOVE_ERRORS,
                       "Move source and destination overlap: " + src.to_std_string() + " -> " + dst.to_std_string());
    }
    if (destination_exists) {
        const Ret removed = remove(dst);
        if (!removed) {
            return removed;
        }
    }

    fs::rename(source.path, destination.path, ec);
    if (ec) {
        return fs_failure(io::Err::FS_MOVE_ERRORS, "Failed to move", source.path, ec);
    }
    return make_ok();
}

Ret NativeFileSystem::make_path(const io::PathT &path) const {
    const Target target = resolve(path);
    if (!target.valid || target.path.empty()) {
        return failure(io::Err::FS_MAKING_ERROR, "Invalid path: " + path.to_std_string());
    }
    if (target.resource) {
        return read_only_failure(io::Err::FS_MAKING_ERROR, "Cannot create resource path", path);
    }

    std::error_code ec;
    fs::create_directories(target.path, ec);
    if (ec) {
        return fs_failure(io::Err::FS_MAKING_ERROR, "Failed to create directory", target.path, ec);
    }
    if (!fs::is_directory(target.path, ec)) {
        return fs_failure(io::Err::FS_MAKING_ERROR, "Not a directory", target.path, ec);
    }
    return make_ok();
}

Ret NativeFileSystem::make_link(const io::PathT &target_path, const io::PathT &link_path) const {
    const Target target = resolve(target_path);
    const Target link = resolve(link_path);
    if (!target.valid) {
        return failure(io::Err::FS_MAKING_ERROR, "Invalid link target: " + target_path.to_std_string());
    }
    if (!link.valid || link.path.empty()) {
        return failure(io::Err::FS_MAKING_ERROR, "Invalid link path: " + link_path.to_std_string());
    }
    if (link.resource) {
        return read_only_failure(io::Err::FS_MAKING_ERROR, "Cannot create link in", link_path);
    }

    const fs::path link_target = target.path.lexically_normal();
    std::error_code ec;
    const bool target_is_directory = fs::is_directory(link_target, ec);
    if (ec && ec != std::errc::no_such_file_or_directory) {
        return fs_failure(io::Err::FS_MAKING_ERROR, "Failed to inspect link target", link_target, ec);
    }
    ec.clear();
    if (target_is_directory) {
        fs::create_directory_symlink(link_target, link.path, ec);
    } else {
        fs::create_symlink(link_target, link.path, ec);
    }
    if (ec) {
        return fs_failure(io::Err::FS_MAKING_ERROR, "Failed to create link", link.path, ec);
    }
    return make_ok();
}

EntryType NativeFileSystem::entry_type(const io::PathT &path) const {
    const Target target = resolve(path);
    if (!target.valid) {
        return EntryType::UNDEFINED;
    }

    std::error_code ec;
    const fs::file_status status = fs::status(target.path, ec);
    if (ec) {
        return EntryType::UNDEFINED;
    }
    if (fs::is_regular_file(status)) {
        return EntryType::FILE;
    }
    if (fs::is_directory(status)) {
        return EntryType::DIR;
    }
    return EntryType::UNDEFINED;
}

RetVal<uint64_t> NativeFileSystem::file_size(const io::PathT &path) const {
    RetVal<uint64_t> result;
    const Target target = resolve(path);
    if (!target.valid) {
        result.ret = failure(io::Err::FS_NOT_EXIST, "Invalid path: " + path.to_std_string());
        return result;
    }

    const Ret exists_ret = exists(path);
    if (!exists_ret) {
        result.ret = exists_ret;
        return result;
    }

    std::error_code ec;
    const uint64_t size = fs::file_size(target.path, ec);
    if (ec) {
        result.ret = fs_failure(io::Err::FS_READ_ERROR, "Failed to get file size", target.path, ec);
        return result;
    }
    result.ret = make_ok();
    result.val = size;
    return result;
}

RetVal<uint64_t> NativeFileSystem::available_space(const io::PathT &path) const {
    RetVal<uint64_t> result;
    const Target target = resolve(path);
    if (!target.valid) {
        result.ret = failure(io::Err::FS_NOT_EXIST, "Invalid path: " + path.to_std_string());
        return result;
    }

    // QStorageInfo treats a missing path as an invalid storage, so an error here
    // means "not there" as much as "cannot query".
    std::error_code ec;
    const fs::space_info space = fs::space(target.path, ec);
    if (ec) {
        result.ret = fs_failure(io::Err::FS_NOT_EXIST, "Failed to query available space", target.path, ec);
        return result;
    }
    result.ret = make_ok();
    result.val = static_cast<uint64_t>(space.available);
    return result;
}

RetVal<io::paths_t> NativeFileSystem::scan_files(const io::PathT &root_dir, const std::vector<std::string> &filters, ScanMode mode) const {
    RetVal<io::paths_t> result;
    const Target root = resolve(root_dir);
    if (!root.valid) {
        result.ret = failure(io::Err::FS_NOT_EXIST, "Invalid path: " + root_dir.to_std_string());
        return result;
    }

    const Ret exists_ret = exists(root_dir);
    if (!exists_ret) {
        result.ret = exists_ret;
        return result;
    }

    std::error_code ec;
    if (!fs::is_directory(root.path, ec)) {
        if (ec && ec != std::errc::no_such_file_or_directory) {
            result.ret = fs_failure(io::Err::FS_READ_ERROR, "Failed to stat directory", root.path, ec);
            return result;
        }
        result.ret = make_ok();
        return result;
    }

    bool want_files = false;
    bool want_folders = false;
    bool recursive = false;
    switch (mode) {
        case ScanMode::FILES_IN_CURRENT_DIR: want_files = true; break;
        case ScanMode::FOLDERS_IN_CURRENT_DIR: want_folders = true; break;
        case ScanMode::FILES_AND_FOLDERS_IN_CURRENT_DIR:
            want_files = true;
            want_folders = true;
            break;
        case ScanMode::FILES_IN_CURRENT_DIR_AND_SUBDIRS:
            want_files = true;
            recursive = true;
            break;
    }

    const io::PathT output_root =
        root.virtual_path ? io::PathT(root.virtual_rel.empty() ? ":/" : ":/" + root.virtual_rel) : io::PathT(path_to_utf8(root.path));
    result.ret = make_ok();
    scan_directory(root.path, output_root, recursive, want_files, want_folders, filters, result.val, result.ret);
    if (!result.ret) {
        result.val.clear();
        return result;
    }
    std::sort(result.val.begin(), result.val.end());
    return result;
}

void NativeFileSystem::set_attribute(const io::PathT &, io::IFileSystem::Attribute) const {
    // Hidden attributes have no portable std::filesystem equivalent, so this is a no-op.
}

bool NativeFileSystem::set_permissions_allowed_for_all(const io::PathT &path) const {
    const Target target = resolve(path);
    if (!target.valid || target.resource) {
        return false;
    }

    const fs::perms all = fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec | fs::perms::group_read |
                          fs::perms::group_write | fs::perms::group_exec | fs::perms::others_read | fs::perms::others_write |
                          fs::perms::others_exec;
    std::error_code ec;
    fs::permissions(target.path, all, fs::perm_options::replace, ec);
    return !ec;
}

Ret NativeFileSystem::read_file(const io::PathT &file_path, ByteArray &data) const {
    const Target target = resolve(file_path);
    if (!target.valid) {
        return failure(io::Err::FS_READ_ERROR, "Invalid path: " + file_path.to_std_string());
    }

    std::ifstream stream(target.path, std::ios::binary);
    if (!stream.is_open()) {
        return failure(io::Err::FS_READ_ERROR, "Failed to open file: " + path_to_utf8(target.path));
    }

    std::error_code ec;
    const uint64_t size = fs::file_size(target.path, ec);
    if (ec) {
        return fs_failure(io::Err::FS_READ_ERROR, "Failed to get file size", target.path, ec);
    }
    if (size > ::k_max_read_file_size) {
        return failure(io::Err::FS_READ_ERROR, "File exceeds the 256 MiB read limit: " + path_to_utf8(target.path));
    }

    ByteArray buffer;
    try {
        buffer = ByteArray(static_cast<size_t>(size));
    } catch (const std::bad_alloc &) {
        return failure(io::Err::FS_READ_ERROR, "Out of memory reading file: " + path_to_utf8(target.path));
    }
    if (size > 0) {
        stream.read(reinterpret_cast<char *>(buffer.data()), static_cast<std::streamsize>(size));
        if (!stream || static_cast<uint64_t>(stream.gcount()) != size) {
            return failure(io::Err::FS_READ_ERROR, "Unexpected end of file: " + path_to_utf8(target.path));
        }
    }
    data = std::move(buffer);
    return make_ok();
}

RetVal<ByteArray> NativeFileSystem::read_file(const io::PathT &file_path) const {
    RetVal<ByteArray> result;
    const Ret exists_ret = exists(file_path);
    if (!exists_ret) {
        result.ret = exists_ret;
        return result;
    }

    ByteArray data;
    const Ret read_ret = read_file(file_path, data);
    result.ret = read_ret;
    if (read_ret) {
        result.val = std::move(data);
    }
    return result;
}

Ret NativeFileSystem::write_file(const io::PathT &file_path, const ByteArray &data) {
    const Target target = resolve(file_path);
    if (!target.valid) {
        return failure(io::Err::FS_WRITE_ERROR, "Invalid path: " + file_path.to_std_string());
    }
    if (target.resource) {
        return read_only_failure(io::Err::FS_WRITE_ERROR, "Cannot write", file_path);
    }
    if (data.size() > static_cast<size_t>(std::numeric_limits<std::streamsize>::max())) {
        return failure(io::Err::FS_WRITE_ERROR, "Data is too large to write: " + file_path.to_std_string());
    }

    std::ofstream stream(target.path, std::ios::binary | std::ios::trunc);
    if (!stream.is_open()) {
        return failure(io::Err::FS_WRITE_ERROR, "Failed to open file for writing: " + path_to_utf8(target.path));
    }

    if (!data.empty()) {
        stream.write(data.const_char(), static_cast<std::streamsize>(data.size()));
    }
    stream.flush();
    if (!stream) {
        return failure(io::Err::FS_WRITE_ERROR, "Failed to write file: " + path_to_utf8(target.path));
    }
    stream.close();
    if (stream.fail()) {
        return failure(io::Err::FS_WRITE_ERROR, "Failed to close file: " + path_to_utf8(target.path));
    }
    return make_ok();
}

RetVal<StreamId> NativeFileSystem::open_stream(const io::PathT &file_path, OpenMode mode) {
    RetVal<StreamId> result;
    const Target target = resolve(file_path);
    if (!target.valid) {
        result.ret = failure(io::Err::FS_WRITE_ERROR, "Invalid path: " + file_path.to_std_string());
        return result;
    }
    if (target.resource) {
        result.ret = read_only_failure(io::Err::FS_WRITE_ERROR, "Cannot write", file_path);
        return result;
    }

    std::ios::openmode flags = std::ios::binary | std::ios::out;
    flags |= (mode == OpenMode::APPEND) ? std::ios::app : std::ios::trunc;
    auto stream = std::make_unique<Stream>();
    stream->file.open(target.path, flags);
    if (!stream->file.is_open()) {
        result.ret = failure(io::Err::FS_WRITE_ERROR, "Failed to open stream: " + path_to_utf8(target.path));
        return result;
    }

    std::lock_guard<std::mutex> lock(streams_mutex_);
    while (streams_.find(next_stream_id_) != streams_.end()) {
        if (next_stream_id_ == std::numeric_limits<StreamId>::max()) {
            next_stream_id_ = 0;
        } else {
            ++next_stream_id_;
        }
    }

    const StreamId id = next_stream_id_;
    if (next_stream_id_ == std::numeric_limits<StreamId>::max()) {
        next_stream_id_ = 0;
    } else {
        ++next_stream_id_;
    }
    streams_.emplace(id, std::move(stream));

    result.ret = make_ok();
    result.val = id;
    return result;
}

Ret NativeFileSystem::write_to_stream(StreamId file_id, const ByteArray &data, uint64_t offset) {
    std::lock_guard<std::mutex> lock(streams_mutex_);
    auto it = streams_.find(file_id);
    if (it == streams_.end() || !it->second) {
        return failure(io::Err::FS_WRITE_ERROR, "Invalid stream id");
    }

    std::ofstream &stream = it->second->file;
    if (offset != ::muse::io::stream_pos_current) {
        if (offset > static_cast<uint64_t>(std::numeric_limits<std::streamoff>::max())) {
            return failure(io::Err::FS_WRITE_ERROR, "Stream offset is out of range");
        }
        stream.seekp(static_cast<std::streamoff>(offset), std::ios::beg);
        if (!stream) {
            return failure(io::Err::FS_WRITE_ERROR, "Failed to seek stream");
        }
    }

    if (data.size() > static_cast<size_t>(std::numeric_limits<std::streamsize>::max())) {
        return failure(io::Err::FS_WRITE_ERROR, "Stream write is too large");
    }
    if (!data.empty()) {
        stream.write(data.const_char(), static_cast<std::streamsize>(data.size()));
    }
    if (!stream) {
        return failure(io::Err::FS_WRITE_ERROR, "Failed to write stream");
    }
    return make_ok();
}

Ret NativeFileSystem::close_stream(StreamId file_id) {
    std::unique_ptr<Stream> stream;
    {
        std::lock_guard<std::mutex> lock(streams_mutex_);
        auto it = streams_.find(file_id);
        if (it == streams_.end() || !it->second) {
            return failure(io::Err::FS_WRITE_ERROR, "Invalid stream id");
        }
        stream = std::move(it->second);
        streams_.erase(it);
    }

    if (!stream) {
        return failure(io::Err::FS_WRITE_ERROR, "Invalid stream id");
    }
    stream->file.flush();
    const bool flushed = stream->file.good();
    stream->file.close();
    if (!flushed || stream->file.fail()) {
        return failure(io::Err::FS_WRITE_ERROR, "Failed to flush or close stream");
    }
    return make_ok();
}

io::PathT NativeFileSystem::canonical_file_path(const io::PathT &file_path) const {
    const Target target = resolve(file_path);
    if (!target.valid) {
        return {};
    }

    std::error_code ec;
    const fs::path canonical = fs::canonical(target.path, ec);
    if (ec) {
        return {};
    }
    return io::PathT(path_to_utf8(canonical));
}

io::PathT NativeFileSystem::absolute_path(const io::PathT &file_path) const {
    const Target target = resolve(file_path);
    if (!target.valid) {
        return {};
    }

    std::error_code ec;
    const fs::path absolute = fs::absolute(target.path, ec);
    if (ec) {
        return {};
    }
    return io::PathT(path_to_utf8(absolute.lexically_normal().parent_path()));
}

io::PathT NativeFileSystem::absolute_file_path(const io::PathT &file_path) const {
    const Target target = resolve(file_path);
    if (!target.valid) {
        return {};
    }

    std::error_code ec;
    const fs::path absolute = fs::absolute(target.path, ec);
    if (ec) {
        return {};
    }
    return io::PathT(path_to_utf8(absolute.lexically_normal()));
}

io::PathT NativeFileSystem::clean_path(const io::PathT &file_path) const {
    // Lexical only: QDir::cleanPath never touches the disk, so an invalid or
    // missing path is still cleaned instead of rejected.
    return io::PathT(clean_path_string(file_path.to_std_string()));
}

bool NativeFileSystem::is_sym_link(const io::PathT &file_path) const {
    const Target target = resolve(file_path);
    if (!target.valid) {
        return false;
    }

    std::error_code ec;
    const fs::file_status status = fs::symlink_status(target.path, ec);
    return !ec && fs::is_symlink(status);
}

io::PathT NativeFileSystem::sym_link_target(const io::PathT &file_path) const {
    const Target target = resolve(file_path);
    if (!target.valid) {
        return {};
    }

    std::error_code ec;
    fs::path link = fs::read_symlink(target.path, ec);
    if (ec) {
        return {};
    }
    // QFileInfo::symLinkTarget reports an absolute path; read_symlink returns the
    // stored target, which is relative to the link's own directory.
    if (link.is_relative()) {
        link = target.path.parent_path() / link;
    }
    return io::PathT(path_to_utf8(link.lexically_normal()));
}

io::PathT NativeFileSystem::temporary_directory_path() const {
    std::error_code ec;
    const fs::path directory = fs::temp_directory_path(ec);
    if (ec) {
        return {};
    }
    return io::PathT(path_to_utf8(directory));
}

DateTime NativeFileSystem::birth_time(const io::PathT &file_path) const {
    // std::filesystem has no portable creation time; do not fake one.
    (void) file_path;
    return {};
}

DateTime NativeFileSystem::last_modified(const io::PathT &file_path) const {
    const Target target = resolve(file_path);
    if (!target.valid) {
        return {};
    }

    std::error_code ec;
    const fs::file_time_type time = fs::last_write_time(target.path, ec);
    if (ec) {
        return {};
    }
    return date_time_from_file_time(time);
}

Ret NativeFileSystem::is_writable(const io::PathT &file_path) const {
    const Target target = resolve(file_path);
    if (!target.valid) {
        return failure(io::Err::FS_WRITE_ERROR, "Invalid path: " + file_path.to_std_string());
    }
    if (target.resource) {
        return read_only_failure(io::Err::FS_WRITE_ERROR, "Cannot write", file_path);
    }

    std::error_code ec;
    const bool existed = fs::exists(target.path, ec);
    if (ec && ec != std::errc::no_such_file_or_directory) {
        return fs_failure(io::Err::FS_WRITE_ERROR, "Failed to stat", target.path, ec);
    }

    // A directory cannot be opened as a stream; match the upstream permission check.
    ec.clear();
    const fs::file_status status = fs::status(target.path, ec);
    if (!ec && fs::is_directory(status)) {
        const fs::perms write_bits = fs::perms::owner_write | fs::perms::group_write | fs::perms::others_write;
        if ((status.permissions() & write_bits) != fs::perms::none) {
            return make_ok();
        }
        return failure(io::Err::FS_WRITE_ERROR, "Directory is not writable: " + path_to_utf8(target.path));
    }

    std::ofstream stream(target.path, std::ios::binary | std::ios::app);
    if (!stream.is_open()) {
        return failure(io::Err::FS_WRITE_ERROR, "Path is not writable: " + path_to_utf8(target.path));
    }
    stream.close();
    const bool closed = !stream.fail();

    if (!existed) {
        std::error_code remove_error;
        fs::remove(target.path, remove_error);
    }
    if (!closed) {
        return failure(io::Err::FS_WRITE_ERROR, "Failed to close file: " + path_to_utf8(target.path));
    }
    return make_ok();
}
