// SPDX-License-Identifier: GPL-3.0-only
#include <array>

#include <gtest/gtest.h>

#include "global/types/bytearray.h"
#include "io/byteview.h"

TEST(ByteView, BorrowsBytesAndPreservesSeekBounds) {
    const std::array<uint8_t, 5> data{0, 0xff, 3, 4, 5};
    muse::io::ByteView view(data);
    ASSERT_TRUE(view.open());
    EXPECT_EQ(view.read_data(), data.data());
    EXPECT_EQ(view.size(), data.size());
    ASSERT_TRUE(view.seek(1));
    uint8_t read[8]{};
    EXPECT_EQ(view.read(read, 8), 4u);
    EXPECT_EQ(read[0], 0xff);
    EXPECT_FALSE(view.seek(6));
    EXPECT_EQ(view.pos(), 5u);
    EXPECT_TRUE(view.seek(0));
    EXPECT_EQ(view.read_all().size(), data.size());
    view.close();
    EXPECT_FALSE(view.open(muse::io::IODevice::WRITE_ONLY));
    EXPECT_FALSE(view.open(muse::io::IODevice::READ_WRITE));
    ASSERT_TRUE(view.open());
    EXPECT_EQ(view.pos(), 0u);
}

TEST(ByteView, EmptyInput) {
    muse::io::ByteView view(std::span<const uint8_t>{});
    ASSERT_TRUE(view.open());
    EXPECT_TRUE(view.seek(0));
    EXPECT_FALSE(view.seek(1));
    EXPECT_TRUE(view.read_all().empty());
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

#include <chrono>

#include "io/streamfile.h"

TEST(StreamingFile, UnicodePathAndHeaderSeek) {
    const auto path =
        std::filesystem::temp_directory_path() / (std::filesystem::path(u"engrave-\u97f3\U0001D11E-\U0001F600-") +=
                                                  std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

    struct Cleanup {
        std::filesystem::path p;

        ~Cleanup() {
            std::error_code ec;
            std::filesystem::remove(p, ec);
        }
    } cleanup{path};

    muse::io::StreamingFileDevice out(path);
    ASSERT_TRUE(out.open(muse::io::IODevice::WRITE_ONLY));
    const uint8_t data[]{1, 2, 3, 4};
    ASSERT_EQ(out.write(data, 4), 4u);
    ASSERT_TRUE(out.seek(0));
    ASSERT_EQ(out.write(data + 3, 1), 1u);
    out.close();
    ASSERT_FALSE(out.has_error()) << out.error_string();
    std::ifstream input(path, std::ios::binary);
    const std::string result{std::istreambuf_iterator<char>(input), {}};
    EXPECT_EQ(result, std::string("\x04\x02\x03\x04", 4));
}

TEST(StreamingFile, LimitsApplyBeforeWritesAndCountRewrites) {
    const auto path = std::filesystem::temp_directory_path() /
                      ("engrave-stream-limit-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

    struct Cleanup {
        std::filesystem::path p;

        ~Cleanup() {
            std::error_code ec;
            std::filesystem::remove(p, ec);
        }
    } cleanup{path};

    const uint8_t data[]{1, 2, 3, 4};
    {
        muse::io::StreamingFileDevice out(path, 4);
        ASSERT_TRUE(out.open(muse::io::IODevice::WRITE_ONLY));
        ASSERT_EQ(out.write(data, 4), 4u);
        EXPECT_EQ(out.write(data, 1), 0u);
        EXPECT_TRUE(out.limit_exceeded());
        out.close();
        EXPECT_EQ(std::filesystem::file_size(path), 4u);
    }
    {
        muse::io::StreamingFileDevice out(path, 4, 5);
        ASSERT_TRUE(out.open(muse::io::IODevice::WRITE_ONLY));
        ASSERT_EQ(out.write(data, 4), 4u);
        ASSERT_TRUE(out.seek(0));
        EXPECT_EQ(out.write(data, 2), 0u);
        EXPECT_TRUE(out.limit_exceeded());
        out.close();
        EXPECT_EQ(std::filesystem::file_size(path), 4u);
    }
}

#ifndef _WIN32
TEST(StreamingFile, FinalizationFailurePropagates) {
    muse::io::StreamingFileDevice out("/dev/full");
    ASSERT_TRUE(out.open(muse::io::IODevice::WRITE_ONLY));
    const uint8_t value = 1;
    out.write(&value, 1);
    out.close();
    EXPECT_TRUE(out.has_error());
}
#endif

TEST(ByteArray, CopiesOwnBytesEvenWhenMutablePointerWasLentBeforeCopy) {
    muse::ByteArray original("abc");
    uint8_t *borrowed = original.data();
    muse::ByteArray copy = original;
    ASSERT_NE(original.const_data(), copy.const_data());
    borrowed[0] = 'x';
    EXPECT_EQ(copy, muse::ByteArray("abc"));
    copy[1] = 'y';
    EXPECT_EQ(original, muse::ByteArray("xbc"));
    EXPECT_EQ(copy.const_data()[copy.size()], 0);
}

TEST(ByteArray, BinarySlicesAndSelfAppendPreserveBytesAcrossReallocation) {
    const uint8_t bytes[]{0, 0xff, 1, 2};
    muse::ByteArray value(bytes, 4);
    value.push_back(value);
    EXPECT_EQ(value.size(), 8u);
    EXPECT_EQ(value.left(4), value.right(4));
    const auto slice = value.const_data() + 1;
    value.push_back(slice, 3);
    EXPECT_EQ(value.right(3), muse::ByteArray(bytes + 1, 3));
    EXPECT_EQ(value.const_data()[value.size()], 0);
    EXPECT_EQ(value.left(100), value);
    EXPECT_EQ(value.right(100), value);
}

TEST(ByteArray, EmptyAndMovedFromValuesRemainReadableAndReusable) {
    muse::ByteArray empty;
    EXPECT_EQ(empty.const_data()[0], 0);
    empty.reserve(100);
    EXPECT_TRUE(empty.empty());
    muse::ByteArray value("abc");
    muse::ByteArray moved(std::move(value));
    EXPECT_EQ(moved, muse::ByteArray("abc"));
    value.clear();
    EXPECT_EQ(value.const_data()[0], 0);
    value.push_back('z');
    EXPECT_EQ(value, muse::ByteArray("z"));
    value.resize(4);
    EXPECT_EQ(value.const_data()[4], 0);
    EXPECT_EQ(value[1], 0);
}
