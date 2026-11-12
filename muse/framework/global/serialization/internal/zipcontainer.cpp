/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore Limited and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "zipcontainer.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <ctime>

#include <zlib.h>

#include "global/io/dir.h"
#include "global/io/writebudget.h"
#include "log.h"

// Zip standard version for archives handled by this API
// (actually, the only basic support of this version is implemented but it is enough for now)
#define ZIP_VERSION 20

#define ZDEBUG \
    if (0) LOGD

using namespace muse::io;

typedef unsigned long int ulong;
typedef unsigned short int ushort;
typedef unsigned int uint;

namespace muse {
static inline uint32_t read_u_int(const uint8_t *data) {
    return uint32_t(data[0]) | (uint32_t(data[1]) << 8) | (uint32_t(data[2]) << 16) | (uint32_t(data[3]) << 24);
}

static inline ushort read_u_short(const uint8_t *data) { return ushort(data[0]) | ushort(ushort(data[1]) << 8); }

static inline void write_u_int(uint8_t *data, uint i) {
    data[0] = i & 0xff;
    data[1] = (i >> 8) & 0xff;
    data[2] = (i >> 16) & 0xff;
    data[3] = (i >> 24) & 0xff;
}

static inline void write_u_short(uint8_t *data, ushort i) {
    data[0] = i & 0xff;
    data[1] = (i >> 8) & 0xff;
}

static inline void copy_u_int(uint8_t *dest, const uint8_t *src) {
    dest[0] = src[0];
    dest[1] = src[1];
    dest[2] = src[2];
    dest[3] = src[3];
}

static inline void copy_u_short(uint8_t *dest, const uint8_t *src) {
    dest[0] = src[0];
    dest[1] = src[1];
}

static void write_ms_dos_date(uint8_t *dest, const std::tm &dt) {
    if (dt.tm_year > 0) {
        uint16_t time = (dt.tm_hour << 11)  // 5 bit hour
                        | (dt.tm_min << 5)  // 6 bit minute
                        | (dt.tm_sec >> 1); // 5 bit double seconds

        dest[0] = time & 0xff;
        dest[1] = time >> 8;

        uint16_t date = ((dt.tm_year + 1900 - 1980) << 9) // 7 bit year 1980-based
                        | ((dt.tm_mon + 1) << 5)          // 4 bit month
                        | (dt.tm_mday);                   // 5 bit day

        dest[2] = char(date);
        dest[3] = char(date >> 8);
    } else {
        dest[0] = 0;
        dest[1] = 0;
        dest[2] = 0;
        dest[3] = 0;
    }
}

static int inflate(Bytef *dest, ulong *dest_len, const Bytef *source, ulong source_len) {
    z_stream stream{};
    int err;

    stream.next_in = const_cast<Bytef *>(source);
    stream.avail_in = (uInt) source_len;
    if ((uLong) stream.avail_in != source_len) {
        return Z_BUF_ERROR;
    }

    stream.next_out = dest;
    stream.avail_out = (uInt) *dest_len;
    if ((uLong) stream.avail_out != *dest_len) {
        return Z_BUF_ERROR;
    }

    stream.zalloc = (alloc_func) 0;
    stream.zfree = (free_func) 0;

    err = inflateInit2(&stream, -MAX_WBITS);
    if (err != Z_OK) {
        return err;
    }

    err = inflate(&stream, Z_FINISH);
    if (err != Z_STREAM_END) {
        inflateEnd(&stream);
        if (err == Z_NEED_DICT || (err == Z_BUF_ERROR && stream.avail_in == 0)) {
            return Z_DATA_ERROR;
        }
        return err == Z_OK ? Z_BUF_ERROR : err;
    }

    // The declared compressed size is part of the entry; trailing bytes are corrupt.
    if (stream.avail_in != 0) {
        inflateEnd(&stream);
        return Z_DATA_ERROR;
    }

    *dest_len = stream.total_out;

    err = inflateEnd(&stream);
    return err;
}

static int deflate(Bytef *dest, ulong *dest_len, const Bytef *source, ulong source_len) {
    z_stream stream;
    int err;

    stream.next_in = const_cast<Bytef *>(source);
    stream.avail_in = (uInt) source_len;
    stream.next_out = dest;
    stream.avail_out = (uInt) *dest_len;
    if ((uLong) stream.avail_out != *dest_len) {
        return Z_BUF_ERROR;
    }

    stream.zalloc = (alloc_func) 0;
    stream.zfree = (free_func) 0;
    stream.opaque = (voidpf) 0;

    err = deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
    if (err != Z_OK) {
        return err;
    }

    err = deflate(&stream, Z_FINISH);
    if (err != Z_STREAM_END) {
        deflateEnd(&stream);
        return err == Z_OK ? Z_BUF_ERROR : err;
    }
    *dest_len = stream.total_out;

    err = deflateEnd(&stream);
    return err;
}

namespace WindowsFileAttributes {
enum {
    DIR = 0x10,  // FILE_ATTRIBUTE_DIRECTORY
    FILE = 0x80, // FILE_ATTRIBUTE_NORMAL
    TYPE_MASK = 0x90,

    READ_ONLY = 0x01, // FILE_ATTRIBUTE_READONLY
    PERM_MASK = 0x01
};
} // namespace WindowsFileAttributes

namespace UnixFileAttributes {
enum {
    DIR = 0040000,       // __S_IFDIR
    FILE = 0100000,      // __S_IFREG
    SYM_LINK = 0120000,  // __S_IFLNK
    TYPE_MASK = 0170000, // __S_IFMT

    READ_USER = 0400,   // __S_IRUSR
    WRITE_USER = 0200,  // __S_IWUSR
    EXE_USER = 0100,    // __S_IXUSR
    READ_GROUP = 0040,  // __S_IRGRP
    WRITE_GROUP = 0020, // __S_IWGRP
    EXE_GROUP = 0010,   // __S_IXGRP
    READ_OTHER = 0004,  // __S_IROTH
    WRITE_OTHER = 0002, // __S_IWOTH
    EXE_OTHER = 0001,   // __S_IXOTH
    PERM_MASK = 0777
};
} // namespace UnixFileAttributes

static std::tm read_ms_dos_date(const uint8_t *src) {
    std::tm tm;
    uint dos_date = read_u_int(src);
    uint64_t u_date;
    u_date = (uint64_t) (dos_date >> 16);
    tm.tm_mday = (u_date & 0x1f);
    tm.tm_mon = ((u_date & 0x1E0) >> 5) - 1;
    tm.tm_year = (((u_date & 0x0FE00) >> 9) + 1980 - 1900);
    tm.tm_hour = ((dos_date & 0xF800) >> 11);
    tm.tm_min = ((dos_date & 0x7E0) >> 5);
    tm.tm_sec = ((dos_date & 0x1f) << 1);

    return tm;
}

// for details, see http://www.pkware.com/documents/casestudies/APPNOTE.TXT

enum HostOS {
    HOST_FAT = 0,
    HOST_AMIGA = 1,
    HOST_VMS = 2, // VAX/VMS
    HOST_UNIX = 3,
    HOST_VM_CMS = 4,
    HOST_ATARI = 5, // what if it's a minix filesystem? [cjh]
    HOST_HPFS = 6,  // filesystem used by OS/2 (and NT 3.x)
    HOST_MAC = 7,
    HOST_Z_SYSTEM = 8,
    HOST_CPM = 9,
    HOST_TOP_S20 = 10, // pkzip 2.50 NTFS
    HOST_NTFS = 11,    // filesystem used by Windows NT
    HOST_QDOS = 12,    // SMS/QDOS
    HOST_ACORN = 13,   // Archimedes Acorn RISC OS
    HOST_VFAT = 14,    // filesystem used by Windows 95, NT
    HOST_MVS = 15,
    HOST_BE_OS = 16, // hybrid POSIX/database filesystem
    HOST_TANDEM = 17,
    HOST_O_S400 = 18,
    HOST_OSX = 19
};

enum GeneralPurposeFlag {
    ENCRYPTED = 0x01,
    ALG_TUNE1 = 0x02,
    ALG_TUNE2 = 0x04,
    HAS_DATA_DESCRIPTOR = 0x08,
    PATCHED_DATA = 0x20,
    STRONG_ENCRYPTED = 0x40,
    UTF8_NAMES = 0x0800,
    CENTRAL_DIRECTORY_ENCRYPTED = 0x2000
};

enum CompressionMethod {
    COMPRESSION_METHOD_STORED = 0,
    COMPRESSION_METHOD_SHRUNK = 1,
    COMPRESSION_METHOD_REDUCED1 = 2,
    COMPRESSION_METHOD_REDUCED2 = 3,
    COMPRESSION_METHOD_REDUCED3 = 4,
    COMPRESSION_METHOD_REDUCED4 = 5,
    COMPRESSION_METHOD_IMPLODED = 6,
    COMPRESSION_METHOD_RESERVED_TOKENIZING = 7, // reserved for tokenizing
    COMPRESSION_METHOD_DEFLATED = 8,
    COMPRESSION_METHOD_DEFLATED64 = 9,
    COMPRESSION_METHOD_PK_IMPLODING = 10,

    COMPRESSION_METHOD_B_ZIP2 = 12,

    COMPRESSION_METHOD_LZMA = 14,

    COMPRESSION_METHOD_TERSE = 18,
    COMPRESSION_METHOD_LZ77 = 19,

    COMPRESSION_METHOD_JPEG = 96,
    COMPRESSION_METHOD_WAV_PACK = 97,
    COMPRESSION_METHOD_PP_MD = 98,
    COMPRESSION_METHOD_WZ_AES = 99
};

// Hard limits for reading a single entry. The central directory is authoritative.
// 64 MiB for the uncompressed data; the compressed data gets the same baseline plus
// the bounded worst-case raw-deflate overhead (the same bound used by addEntry).
static constexpr uint32_t max_entry_uncompressed_size = 64u * 1024u * 1024u;
static constexpr uint32_t max_entry_compressed_size =
    ::muse::max_entry_uncompressed_size + (::muse::max_entry_uncompressed_size >> 12) + (::muse::max_entry_uncompressed_size >> 14) + 11u;

static constexpr uint32_t encryption_flags = uint32_t(ENCRYPTED) | uint32_t(STRONG_ENCRYPTED) | uint32_t(CENTRAL_DIRECTORY_ENCRYPTED);

static constexpr uint64_t max_total_uncompressed_size = 256ULL * 1024ULL * 1024ULL;

struct LocalFileHeader {
    uint8_t signature[4]; //  0x04034b50
    uint8_t version_needed[2];
    uint8_t general_purpose_bits[2];
    uint8_t compression_method[2];
    uint8_t last_mod_file[4];
    uint8_t crc_32[4];
    uint8_t compressed_size[4];
    uint8_t uncompressed_size[4];
    uint8_t file_name_length[2];
    uint8_t extra_field_length[2];
};

bool ZipContainer::starts_with_entry(const uint8_t *data, size_t size, std::span<const uint8_t> name) {
    if (size < sizeof(LocalFileHeader) + name.size()) {
        return false;
    }

    return read_u_int(data + offsetof(LocalFileHeader, signature)) == 0x04034b50 &&
           read_u_short(data + offsetof(LocalFileHeader, file_name_length)) == name.size() &&
           std::equal(name.begin(), name.end(), data + sizeof(LocalFileHeader));
}

struct DataDescriptor {
    uint8_t crc_32[4];
    uint8_t compressed_size[4];
    uint8_t uncompressed_size[4];
};

struct CentralFileHeader {
    uint8_t signature[4]; // 0x02014b50
    uint8_t version_made[2];
    uint8_t version_needed[2];
    uint8_t general_purpose_bits[2];
    uint8_t compression_method[2];
    uint8_t last_mod_file[4];
    uint8_t crc_32[4];
    uint8_t compressed_size[4];
    uint8_t uncompressed_size[4];
    uint8_t file_name_length[2];
    uint8_t extra_field_length[2];
    uint8_t file_comment_length[2];
    uint8_t disk_start[2];
    uint8_t internal_file_attributes[2];
    uint8_t external_file_attributes[4];
    uint8_t offset_local_header[4];
    LocalFileHeader to_local_header() const;
};

struct EndOfDirectory {
    uint8_t signature[4]; // 0x06054b50
    uint8_t this_disk[2];
    uint8_t start_of_directory_disk[2];
    uint8_t num_dir_entries_this_disk[2];
    uint8_t num_dir_entries[2];
    uint8_t directory_size[4];
    uint8_t dir_start_offset[4];
    uint8_t comment_length[2];
};

struct FileHeader {
    CentralFileHeader h;
    ByteArray file_name;
    ByteArray extra_field;
    ByteArray file_comment;
};

LocalFileHeader CentralFileHeader::to_local_header() const {
    LocalFileHeader h;
    write_u_int(h.signature, 0x04034b50);
    copy_u_short(h.version_needed, version_needed);
    copy_u_short(h.general_purpose_bits, general_purpose_bits);
    copy_u_short(h.compression_method, compression_method);
    copy_u_int(h.last_mod_file, last_mod_file);
    copy_u_int(h.crc_32, crc_32);
    copy_u_int(h.compressed_size, compressed_size);
    copy_u_int(h.uncompressed_size, uncompressed_size);
    copy_u_short(h.file_name_length, file_name_length);
    copy_u_short(h.extra_field_length, extra_field_length);
    return h;
}

struct ZipContainer::Impl {
    IODevice *device = nullptr;

    bool dirty_file_tree = true;
    std::vector<FileHeader> file_headers;
    ByteArray comment;
    uint start_of_directory = 0;
    ZipContainer::Status status = ZipContainer::NO_ERROR;
    uint64_t total_decompressed_bytes = 0;

    ZipContainer::CompressionPolicy compression_policy = ZipContainer::ALWAYS_COMPRESS;

    enum EntryType { DIRECTORY, FILE, SYMLINK };

    void add_entry(EntryType type, const std::string &input_file_name, const ByteArray &contents);
    bool write_to_device(const uint8_t *data, size_t len);
    bool write_to_device(const ByteArray &data);

    Impl(IODevice *d) : device(d) {}

    void scan_files();
    ZipContainer::FileInfo fill_file_info(size_t index) const;

    std::string fix_file_path(const ByteArray &path) const;
};

void ZipContainer::Impl::scan_files() {
    if (!dirty_file_tree) {
        return;
    }

    if (status != ZipContainer::NO_ERROR) {
        return;
    }

    if (!(device->is_open() || device->open(IODevice::READ_ONLY))) {
        status = ZipContainer::FILE_OPEN_ERROR;
        file_headers.clear();
        return;
    }

    if ((device->open_mode() & IODevice::READ_ONLY) == 0) { // only read the index from readable files.
        status = ZipContainer::FILE_READ_ERROR;
        file_headers.clear();
        return;
    }

    dirty_file_tree = false;

    const auto fail = [this](const char *message) {
        LOGW() << "Zip: " << message;
        file_headers.clear();
        comment.clear();
        status = ZipContainer::FILE_READ_ERROR;
    };

    const uint64_t file_size_value = device->size();
    if (file_size_value < sizeof(EndOfDirectory)) {
        fail("File is smaller than minimum ZIP size.");
        return;
    }

    if (!device->seek(0)) {
        fail("Failed to seek to start of ZIP file.");
        return;
    }

    uint8_t tmp[4];
    if (device->read(tmp, sizeof(tmp)) != sizeof(tmp)) {
        fail("Failed to read ZIP signature.");
        return;
    }

    const uint32_t first_sig = read_u_int(tmp);
    if (first_sig != 0x04034b50 && first_sig != 0x06054b50) {
        fail("Invalid ZIP signature.");
        return;
    }

    EndOfDirectory eod;
    std::memset(&eod, 0, sizeof(EndOfDirectory));
    bool found_eod = false;
    uint64_t eod_offset = 0;
    const uint64_t max_comment = std::min<uint64_t>(65535, file_size_value - sizeof(EndOfDirectory));
    for (uint64_t i = 0; i <= max_comment; ++i) {
        const uint64_t candidate_pos = file_size_value - sizeof(EndOfDirectory) - i;
        if (!device->seek(static_cast<size_t>(candidate_pos))) {
            fail("Failed to seek while searching for EndOfDirectory.");
            return;
        }
        if (device->read(reinterpret_cast<uint8_t *>(&eod), sizeof(EndOfDirectory)) != sizeof(EndOfDirectory)) {
            fail("Failed to read while searching for EndOfDirectory.");
            return;
        }
        if (read_u_int(eod.signature) != 0x06054b50) {
            continue;
        }
        // A signature inside the archive comment is not the record. The declared
        // comment length must exactly span the bytes after this candidate;
        // otherwise keep scanning backwards for the real record.
        if (read_u_short(eod.comment_length) != i) {
            continue;
        }
        found_eod = true;
        eod_offset = candidate_pos;
        if (i > 0) {
            comment = device->read(static_cast<size_t>(i));
            if (comment.size() != i) {
                fail("Failed to read archive comment.");
                return;
            }
        }
        break;
    }

    if (!found_eod) {
        fail("EndOfDirectory not found.");
        return;
    }

    const uint16_t this_disk_value = read_u_short(eod.this_disk);
    const uint16_t directory_disk = read_u_short(eod.start_of_directory_disk);
    const uint16_t entries_this_disk = read_u_short(eod.num_dir_entries_this_disk);
    const uint16_t num_dir_entries_value = read_u_short(eod.num_dir_entries);
    if (this_disk_value != 0 || directory_disk != 0 || entries_this_disk != num_dir_entries_value) {
        fail("Multi-disk ZIP archives are unsupported.");
        return;
    }

    const uint64_t dir_start_offset_value = read_u_int(eod.dir_start_offset);
    const uint64_t directory_size_value = read_u_int(eod.directory_size);

    if (dir_start_offset_value + directory_size_value > eod_offset) {
        fail("Central directory extends beyond EndOfDirectory offset.");
        return;
    }

    if (directory_size_value < uint64_t(num_dir_entries_value) * sizeof(CentralFileHeader)) {
        fail("Central directory size too small for declared number of entries.");
        return;
    }

    start_of_directory = static_cast<uint>(dir_start_offset_value);
    if (!device->seek(static_cast<size_t>(dir_start_offset_value))) {
        fail("Failed to seek to central directory.");
        return;
    }

    file_headers.clear();
    file_headers.reserve(num_dir_entries_value);
    uint64_t total_uncompressed_size = 0;

    for (uint16_t i = 0; i < num_dir_entries_value; ++i) {
        FileHeader header;
        if (device->read(reinterpret_cast<uint8_t *>(&header.h), sizeof(CentralFileHeader)) != sizeof(CentralFileHeader)) {
            fail("Failed to read complete central directory header.");
            return;
        }

        if (read_u_int(header.h.signature) != 0x02014b50) {
            fail("Invalid central directory header signature.");
            return;
        }

        if (read_u_short(header.h.disk_start) != 0) {
            fail("Central directory entry belongs to another disk.");
            return;
        }

        const size_t name_len = read_u_short(header.h.file_name_length);
        header.file_name = device->read(name_len);
        if (header.file_name.size() != name_len) {
            fail("Failed to read filename from central directory.");
            return;
        }

        const size_t extra_len = read_u_short(header.h.extra_field_length);
        header.extra_field = device->read(extra_len);
        if (header.extra_field.size() != extra_len) {
            fail("Failed to read extra field from central directory.");
            return;
        }

        const size_t comment_len = read_u_short(header.h.file_comment_length);
        header.file_comment = device->read(comment_len);
        if (header.file_comment.size() != comment_len) {
            fail("Failed to read file comment from central directory.");
            return;
        }

        if (device->pos() > dir_start_offset_value + directory_size_value) {
            fail("Central directory entry extends past declared directory size.");
            return;
        }

        const uint32_t entry_uncompressed_size = read_u_int(header.h.uncompressed_size);
        if (entry_uncompressed_size > ::muse::max_entry_uncompressed_size) {
            fail("Central directory entry uncompressed size exceeds limit.");
            return;
        }

        total_uncompressed_size += entry_uncompressed_size;
        if (total_uncompressed_size > ::muse::max_total_uncompressed_size) {
            fail("Aggregate uncompressed size across entries exceeds limit.");
            return;
        }

        file_headers.push_back(std::move(header));
    }
}

ZipContainer::FileInfo ZipContainer::Impl::fill_file_info(size_t index) const {
    ZipContainer::FileInfo file_info;
    const FileHeader &header = file_headers.at(index);
    uint32_t mode = read_u_int(header.h.external_file_attributes);
    const HostOS host_os = HostOS(read_u_short(header.h.version_made) >> 8);
    switch (host_os) {
        case HOST_UNIX:
            mode = (mode >> 16) & 0xffff;
            switch (mode & UnixFileAttributes::TYPE_MASK) {
                case UnixFileAttributes::SYM_LINK: file_info.is_sym_link = true; break;
                case UnixFileAttributes::DIR: file_info.is_dir = true; break;
                case UnixFileAttributes::FILE:
                default: // ### just for the case; should we warn?
                    file_info.is_file = true;
                    break;
            }
            break;
        case HOST_FAT:
        case HOST_NTFS:
        case HOST_HPFS:
        case HOST_VFAT:
            switch (mode & WindowsFileAttributes::TYPE_MASK) {
                case WindowsFileAttributes::DIR: file_info.is_dir = true; break;
                case WindowsFileAttributes::FILE:
                default: file_info.is_file = true; break;
            }

            break;
        default: LOGW("Zip: Zip entry format at %zu is not supported.", index); return file_info; // we don't support anything else
    }

    // ushort general_purpose_bits = readUShort(header.h.general_purpose_bits);
    // if bit 11 is set, the filename and comment fields must be encoded using UTF-8
    // const bool inUtf8 = (general_purpose_bits & Utf8Names) != 0;
    file_info.file_path = fix_file_path(header.file_name);
    file_info.crc = read_u_int(header.h.crc_32);
    file_info.size = read_u_int(header.h.uncompressed_size);
    file_info.last_modified = read_ms_dos_date(header.h.last_mod_file);

    return file_info;
}

void ZipContainer::Impl::add_entry(EntryType type, const std::string &input_file_name, const ByteArray &contents) {
    if (!(device->is_open() || device->open(IODevice::WRITE_ONLY))) {
        status = ZipContainer::FILE_OPEN_ERROR;
        return;
    }
    device->seek(start_of_directory);

    // don't compress small files
    ZipContainer::CompressionPolicy compression = compression_policy;
    if (compression_policy == ZipContainer::AUTO_COMPRESS) {
        if (contents.size() < 64) {
            compression = ZipContainer::NEVER_COMPRESS;
        } else {
            compression = ZipContainer::ALWAYS_COMPRESS;
        }
    }

    FileHeader header;
    std::memset(&header.h, 0, sizeof(CentralFileHeader));
    write_u_int(header.h.signature, 0x02014b50);

    write_u_short(header.h.version_needed, ZIP_VERSION);
    write_u_int(header.h.uncompressed_size, (uint) contents.size());

    std::time_t t = std::time(0); // get time now
    std::tm now;
#ifdef WIN32
    localtime_s(&now, &t);
#else
    localtime_r(&t, &now);
#endif
    write_ms_dos_date(header.h.last_mod_file, now);
    ByteArray data;
    if (compression == ZipContainer::ALWAYS_COMPRESS) {
        write_u_short(header.h.compression_method, COMPRESSION_METHOD_DEFLATED);

        ulong len = (ulong) contents.size();
        // shamelessly copied form zlib
        len += (len >> 12) + (len >> 14) + 11;
        int res;
        do {
            // Charge the compression workspace before allocating it.
            if (io::active_write_budget && !io::active_write_budget->charge(static_cast<uint64_t>(contents.size()) + len)) {
                status = ZipContainer::FILE_WRITE_ERROR;
                return;
            }
            data.resize(len);
            res = deflate((uint8_t *) data.data(), &len, (const uint8_t *) contents.const_data(), (ulong) contents.size());
            if (io::active_write_budget && !io::active_write_budget->poll()) {
                status = ZipContainer::FILE_WRITE_ERROR;
                return;
            }

            switch (res) {
                case Z_OK: data.resize(len); break;
                case Z_MEM_ERROR:
                    LOGW("Zip: Z_MEM_ERROR: Not enough memory to compress file, skipping");
                    data.resize(0);
                    break;
                case Z_BUF_ERROR: len *= 2; break;
            }
        } while (res == Z_BUF_ERROR);
    }
    // TODO add a check if data.size() > contents.size().  Then try to store the original and revert the compression method to be uncompressed
    const ByteArray &payload = compression == ZipContainer::ALWAYS_COMPRESS ? data : contents;
    write_u_int(header.h.compressed_size, (uint) payload.size());
    uint crc_32 = ::crc32(0, 0, 0);
    crc_32 = ::crc32(crc_32, (const uint8_t *) contents.const_data(), (uint) contents.size());
    write_u_int(header.h.crc_32, crc_32);

    // if bit 11 is set, the filename and comment fields must be encoded using UTF-8
    ushort general_purpose_bits = UTF8_NAMES; // always use utf-8
    write_u_short(header.h.general_purpose_bits, general_purpose_bits);

    //const bool inUtf8 = (general_purpose_bits & Utf8Names) != 0;
    header.file_name = ByteArray(input_file_name.c_str(), input_file_name.size());
    if (header.file_name.size() > 0xffff) {
        LOGW("Zip: Filename is too long, chopping it to 65535 bytes");
        header.file_name = header.file_name.left(0xffff); // ### don't break the utf-8 sequence, if any
    }
    if (header.file_comment.size() + header.file_name.size() > 0xffff) {
        LOGW("Zip: File comment is too long, chopping it to 65535 bytes");
        header.file_comment.truncate(0xffff - header.file_name.size()); // ### don't break the utf-8 sequence, if any
    }
    write_u_short(header.h.file_name_length, (ushort) header.file_name.size());
    //h.extra_field_length[2];

    write_u_short(header.h.version_made, HOST_UNIX << 8);
    //uint8_t internal_file_attributes[2];
    //uint8_t external_file_attributes[4];
    uint32_t mode = UnixFileAttributes::READ_USER | UnixFileAttributes::WRITE_USER | UnixFileAttributes::EXE_USER |
                    UnixFileAttributes::READ_GROUP | UnixFileAttributes::READ_OTHER;
    switch (type) {
        case SYMLINK: mode |= UnixFileAttributes::SYM_LINK; break;
        case DIRECTORY: mode |= UnixFileAttributes::DIR; break;
        case FILE: mode |= UnixFileAttributes::FILE; break;
        default: UNREACHABLE; break;
    }
    write_u_int(header.h.external_file_attributes, mode << 16);
    write_u_int(header.h.offset_local_header, start_of_directory);

    file_headers.push_back(header);

    bool ok = true;

    LocalFileHeader h = header.h.to_local_header();
    ok &= write_to_device((const uint8_t *) &h, sizeof(LocalFileHeader));
    ok &= write_to_device(header.file_name);
    ok &= write_to_device(payload);

    start_of_directory = (uint) device->pos();
    dirty_file_tree = true;

    if (!ok) {
        status = ZipContainer::FILE_WRITE_ERROR;
    }
}

bool ZipContainer::Impl::write_to_device(const uint8_t *data, size_t len) { return device->write(data, len) == len; }

bool ZipContainer::Impl::write_to_device(const ByteArray &data) { return device->write(data) == data.size(); }

std::string ZipContainer::Impl::fix_file_path(const ByteArray &path) const {
    // fix the file path, if broken (convert separators, eat leading and trailing ones)
    std::string fixed = Dir::from_native_separators(path.const_char()).to_std_string();
    {
        bool front_ok = false;
        while (!fixed.empty() && !front_ok) {
            if (fixed.front() == '/') {
                fixed = fixed.substr(1);
            } else if (fixed.rfind("./", 0) == 0) {
                fixed = fixed.substr(2);
            } else if (fixed.rfind("../", 0) == 0) {
                fixed = fixed.substr(3);
            } else {
                front_ok = true;
            }
        }
    }

    while (!fixed.empty() && fixed.back() == '/') {
        fixed = fixed.substr(0, fixed.size() - 1);
    }

    return fixed;
}

ZipContainer::ZipContainer(IODevice *device) : p(new Impl(device)) { assert(device); }

ZipContainer::~ZipContainer() {
    close();
    delete p;
}

std::vector<ZipContainer::FileInfo> ZipContainer::file_info_list() const {
    p->scan_files();
    std::vector<FileInfo> files;
    const size_t num_file_headers = p->file_headers.size();
    files.reserve(num_file_headers);
    for (size_t i = 0; i < num_file_headers; ++i) {
        files.push_back(p->fill_file_info(i));
    }
    return files;
}

int ZipContainer::count() const {
    p->scan_files();
    return (int) p->file_headers.size();
}

bool ZipContainer::file_exists(const std::string &input_file_name) const {
    p->scan_files();

    for (size_t i = 0; i < p->file_headers.size(); ++i) {
        std::string current_file_name = p->fix_file_path(p->file_headers.at(i).file_name.const_char());
        if (current_file_name == input_file_name) {
            return true;
        }
    }

    return false;
}

ByteArray ZipContainer::file_data(const std::string &input_file_name) const {
    if (p->status != ZipContainer::NO_ERROR) {
        return ByteArray();
    }

    p->scan_files();

    if (p->status != ZipContainer::NO_ERROR) {
        return ByteArray();
    }

    size_t i = 0;
    for (i = 0; i < p->file_headers.size(); ++i) {
        std::string current_file_name = p->fix_file_path(p->file_headers.at(i).file_name.const_char());
        if (current_file_name == input_file_name) {
            break;
        }
    }

    if (i == p->file_headers.size()) {
        return ByteArray();
    }

    const auto fail = [this](const char *message) -> ByteArray {
        LOGW() << "Zip: " << message;
        p->status = ZipContainer::FILE_READ_ERROR;
        return ByteArray();
    };

    const FileHeader &header = p->file_headers.at(i);

    const uint32_t central_version = read_u_short(header.h.version_needed);
    if (central_version > ZIP_VERSION) {
        LOGW() << "Zip: .ZIP specification version " << central_version << " implementation is needed to extract the data.";
        p->status = ZipContainer::FILE_READ_ERROR;
        return ByteArray();
    }

    const uint32_t central_flags = read_u_short(header.h.general_purpose_bits);
    if ((central_flags & ::muse::encryption_flags) != 0) {
        return fail("Unsupported encryption method is needed to extract the data.");
    }

    const uint32_t central_method = read_u_short(header.h.compression_method);
    if (central_method != COMPRESSION_METHOD_STORED && central_method != COMPRESSION_METHOD_DEFLATED) {
        LOGW() << "Zip: Unsupported compression method " << central_method << " is needed to extract the data.";
        p->status = ZipContainer::FILE_READ_ERROR;
        return ByteArray();
    }

    const uint32_t central_crc = read_u_int(header.h.crc_32);
    const uint32_t compressed_size = read_u_int(header.h.compressed_size);
    const uint32_t uncompressed_size = read_u_int(header.h.uncompressed_size);
    const uint32_t local_offset = read_u_int(header.h.offset_local_header);

    if (uncompressed_size > ::muse::max_entry_uncompressed_size) {
        return fail("Uncompressed entry exceeds the supported size limit.");
    }

    if (p->total_decompressed_bytes + uncompressed_size > ::muse::max_total_uncompressed_size) {
        return fail("Aggregate decompressed size exceeds limit.");
    }

    if (compressed_size > ::muse::max_entry_compressed_size) {
        return fail("Compressed entry exceeds the supported size limit.");
    }

    if (central_method == COMPRESSION_METHOD_STORED && compressed_size != uncompressed_size) {
        return fail("Stored entry has different compressed and uncompressed sizes.");
    }

    IODevice *device = p->device;
    const uint64_t file_size = device->size();
    if (uint64_t(local_offset) > file_size || file_size - uint64_t(local_offset) < sizeof(LocalFileHeader)) {
        return fail("Local file header offset is outside the file.");
    }

    if (!device->seek(static_cast<size_t>(local_offset))) {
        return fail("Failed to seek to the local file header.");
    }

    LocalFileHeader lh;
    std::memset(&lh, 0, sizeof(lh));
    if (device->read(reinterpret_cast<uint8_t *>(&lh), sizeof(lh)) != sizeof(lh)) {
        return fail("Failed to read the complete local file header.");
    }

    if (read_u_int(lh.signature) != 0x04034b50) {
        return fail("Invalid local file header signature.");
    }

    const uint32_t local_version = read_u_short(lh.version_needed);
    if (local_version > ZIP_VERSION) {
        LOGW() << "Zip: .ZIP specification version " << local_version << " implementation is needed to extract the data.";
        p->status = ZipContainer::FILE_READ_ERROR;
        return ByteArray();
    }

    const uint32_t local_flags = read_u_short(lh.general_purpose_bits);
    if ((local_flags & ::muse::encryption_flags) != 0) {
        return fail("Unsupported encryption method is needed to extract the data.");
    }

    const uint32_t flag_mask = ::muse::encryption_flags | uint32_t(HAS_DATA_DESCRIPTOR);
    if ((central_flags & flag_mask) != (local_flags & flag_mask)) {
        return fail("Local and central general purpose flags are inconsistent.");
    }

    const uint32_t local_method = read_u_short(lh.compression_method);
    if (local_method != central_method) {
        return fail("Local and central compression methods are inconsistent.");
    }

    if (local_method != COMPRESSION_METHOD_STORED && local_method != COMPRESSION_METHOD_DEFLATED) {
        LOGW() << "Zip: Unsupported compression method " << local_method << " is needed to extract the data.";
        p->status = ZipContainer::FILE_READ_ERROR;
        return ByteArray();
    }

    // With a data descriptor, the central directory is authoritative and the local
    // CRC/sizes may be zero. Without one, the local header must match the central one.
    if ((central_flags & HAS_DATA_DESCRIPTOR) == 0) {
        if (read_u_int(lh.crc_32) != central_crc || read_u_int(lh.compressed_size) != compressed_size ||
            read_u_int(lh.uncompressed_size) != uncompressed_size) {
            return fail("Local file header CRC or sizes do not match the central directory.");
        }
    }

    const uint32_t local_name_length = read_u_short(lh.file_name_length);
    const uint32_t local_extra_length = read_u_short(lh.extra_field_length);
    const uint64_t data_offset = uint64_t(local_offset) + sizeof(LocalFileHeader) + local_name_length + local_extra_length;
    if (data_offset > file_size || compressed_size > file_size - data_offset) {
        return fail("Compressed data extends past the end of the file.");
    }

    if (!device->seek(static_cast<size_t>(data_offset))) {
        return fail("Failed to seek to the compressed data.");
    }

    ByteArray compressed = device->read(static_cast<size_t>(compressed_size));
    if (compressed.size() != compressed_size) {
        return fail("Failed to read the complete compressed data.");
    }

    ByteArray result;
    if (central_method == COMPRESSION_METHOD_STORED) {
        result = std::move(compressed);
    } else {
        // Allocate exactly the declared output size; an empty entry still needs a
        // valid one-byte destination for zlib. Inflate once with no retry growth.
        const size_t output_capacity = uncompressed_size > 0 ? uncompressed_size : 1;
        result.resize(output_capacity);
        ulong output_len = static_cast<ulong>(output_capacity);
        const int res = inflate(result.data(), &output_len, compressed.const_data(), static_cast<ulong>(compressed_size));
        if (res != Z_OK) {
            LOGW() << "Zip: inflate failed with error " << res << ".";
            p->status = ZipContainer::FILE_READ_ERROR;
            return ByteArray();
        }

        if (output_len != uncompressed_size) {
            return fail("Inflated data size does not match the central directory.");
        }

        result.resize(static_cast<size_t>(output_len));
    }

    uint32_t actual_crc = static_cast<uint32_t>(::crc32(0, Z_NULL, 0));
    if (!result.empty()) {
        actual_crc = static_cast<uint32_t>(::crc32(actual_crc, result.const_data(), static_cast<uInt>(result.size())));
    }

    if (actual_crc != central_crc) {
        return fail("CRC does not match the central directory.");
    }

    p->total_decompressed_bytes += result.size();

    return result;
}

ZipContainer::Status ZipContainer::status() const { return p->status; }

void ZipContainer::set_compression_policy(CompressionPolicy policy) { p->compression_policy = policy; }

ZipContainer::CompressionPolicy ZipContainer::compression_policy() const { return p->compression_policy; }

void ZipContainer::add_file(const std::string &input_file_name, const ByteArray &data) {
    p->add_entry(Impl::FILE, Dir::from_native_separators(input_file_name).to_std_string(), data);
}

void ZipContainer::add_directory(const std::string &dir_name) {
    std::string name(Dir::from_native_separators(dir_name).to_std_string());
    // separator is mandatory
    if (name.back() != '/') {
        name.push_back('/');
    }
    p->add_entry(Impl::DIRECTORY, name, ByteArray());
}

void ZipContainer::close() {
    if (!(p->device->open_mode() & IODevice::WRITE_ONLY)) {
        p->device->close();
        return;
    }

    bool ok = true;

    p->device->seek(p->start_of_directory);
    // write new directory
    for (size_t i = 0; i < p->file_headers.size(); ++i) {
        const FileHeader &header = p->file_headers.at(i);
        ok &= p->write_to_device((const uint8_t *) &header.h, sizeof(CentralFileHeader));
        ok &= p->write_to_device(header.file_name);
        ok &= p->write_to_device(header.extra_field);
        ok &= p->write_to_device(header.file_comment);
    }
    int dir_size = (int) p->device->pos() - (int) p->start_of_directory;
    // write end of directory
    EndOfDirectory eod;
    memset(&eod, 0, sizeof(EndOfDirectory));
    write_u_int(eod.signature, 0x06054b50);
    //uint8_t this_disk[2];
    //uint8_t start_of_directory_disk[2];
    write_u_short(eod.num_dir_entries_this_disk, (ushort) p->file_headers.size());
    write_u_short(eod.num_dir_entries, (ushort) p->file_headers.size());
    write_u_int(eod.directory_size, dir_size);
    write_u_int(eod.dir_start_offset, p->start_of_directory);
    write_u_short(eod.comment_length, (ushort) p->comment.size());

    ok &= p->write_to_device((const uint8_t *) &eod, sizeof(EndOfDirectory));
    ok &= p->write_to_device(p->comment);
    p->device->close();

    if (!ok) {
        p->status = ZipContainer::FILE_WRITE_ERROR;
    }
}
} // namespace muse
