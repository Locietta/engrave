/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
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

#include "log.h"
#include "scorecomp.h"

#include <algorithm>
#include <cstdio>
#include <string_view>
#include <vector>

#include "global/io/buffer.h"
#include "global/serialization/textstream.h"

#include "scorerw.h"

using namespace mu::engraving;
using namespace muse;
using namespace muse::io;

struct Edit {
    enum class Type {
        EQUAL,
        DELETE,
        INSERT,
    };

    Type type = Type::EQUAL;
    size_t idx1 = 0;
    size_t idx2 = 0;
};

static std::vector<std::string_view> split_lines(const ByteArray &data) {
    const char *buf = data.const_char();
    const size_t size = data.size();

    std::vector<std::string_view> lines;
    size_t start = 0;

    for (size_t i = 0; i <= size; ++i) {
        if (i == size || buf[i] == '\n') {
            std::string_view line(buf + start, i - start);
            if (!line.empty() && line.back() == '\r') {
                line.remove_suffix(1);
            }
            lines.push_back(line);
            start = i + 1;
        }
    }

    if (!lines.empty() && lines.back().empty()) {
        lines.pop_back();
    }

    return lines;
}

static std::vector<Edit> compute_diff(const std::vector<std::string_view> &lines1, const std::vector<std::string_view> &lines2) {
    const size_t m = lines1.size();
    const size_t n = lines2.size();

    std::vector<std::vector<size_t>> dp(m + 1, std::vector<size_t>(n + 1, 0));
    for (size_t i = 1; i <= m; ++i) {
        for (size_t j = 1; j <= n; ++j) {
            if (lines1[i - 1] == lines2[j - 1]) {
                dp[i][j] = dp[i - 1][j - 1] + 1;
            } else {
                dp[i][j] = std::max(dp[i - 1][j], dp[i][j - 1]);
            }
        }
    }

    std::vector<Edit> edits;
    size_t i = m;
    size_t j = n;

    while (i > 0 || j > 0) {
        if (i > 0 && j > 0 && lines1[i - 1] == lines2[j - 1]) {
            edits.push_back({Edit::Type::EQUAL, i - 1, j - 1});
            --i;
            --j;
        } else if (j > 0 && (i == 0 || dp[i][j - 1] >= dp[i - 1][j])) {
            edits.push_back({Edit::Type::INSERT, 0, j - 1});
            --j;
        } else {
            edits.push_back({Edit::Type::DELETE, i - 1, 0});
            --i;
        }
    }

    std::reverse(edits.begin(), edits.end());
    return edits;
}

bool ScoreComp::save_compare_score(Score *score, const String &save_name, const String &compare_with_local_path) {
    if (!ScoreRW::save_score(score, save_name)) {
        return false;
    }
    return ScoreComp::compare_files(ScoreRW::root_path() + u"/" + compare_with_local_path, save_name);
}

bool ScoreComp::save_compare_mime_data(muse::ByteArray mime_data, const muse::String &save_name,
                                       const muse::String &compare_with_local_path) {
    if (!ScoreRW::save_mime_data(mime_data, save_name)) {
        return false;
    }
    return ScoreComp::compare_files(ScoreRW::root_path() + u"/" + compare_with_local_path, save_name);
}

bool ScoreComp::compare_files(const String &full_path1, const String &full_path2) {
    TRACEFUNC;

    RetVal<ByteArray> rv1 = muse::io::file_system()->read_file(full_path1);
    if (!rv1.ret) {
        LOGE() << "Failed to read: " << full_path1 << ", err: " << rv1.ret.to_string();
        return false;
    }

    RetVal<ByteArray> rv2 = muse::io::file_system()->read_file(full_path2);
    if (!rv2.ret) {
        LOGE() << "Failed to read: " << full_path2 << ", err: " << rv2.ret.to_string();
        return false;
    }

    const std::vector<std::string_view> lines1 = split_lines(rv1.val);
    const std::vector<std::string_view> lines2 = split_lines(rv2.val);
    if (lines1 == lines2) {
        return true;
    }

    ByteArray output_buf;
    Buffer buf(&output_buf);
    buf.open(IODevice::WRITE_ONLY);
    TextStream ts(&buf);

    ts << "--- " << full_path1 << "\n";
    ts << "+++ " << full_path2 << "\n";

    const std::vector<Edit> edits = compute_diff(lines1, lines2);
    constexpr size_t ctx = 3;
    const size_t n = edits.size();

    size_t i = 0;
    while (i < n) {
        if (edits[i].type == Edit::Type::EQUAL) {
            ++i;
            continue;
        }

        const size_t hunk_start = i >= ctx ? i - ctx : 0;

        size_t pos = i;
        while (pos < n) {
            while (pos < n && edits[pos].type != Edit::Type::EQUAL) {
                ++pos;
            }
            size_t eq_end = pos;
            while (eq_end < n && edits[eq_end].type == Edit::Type::EQUAL) {
                ++eq_end;
            }
            if (eq_end < n && (eq_end - pos) < 2 * ctx) {
                pos = eq_end;
            } else {
                break;
            }
        }

        const size_t hunk_end = std::min(n, pos + ctx);

        size_t start1 = 0;
        size_t start2 = 0;
        size_t count1 = 0;
        size_t count2 = 0;
        bool start1_set = false;
        bool start2_set = false;
        for (size_t k = hunk_start; k < hunk_end; ++k) {
            const Edit &e = edits[k];
            if (e.type != Edit::Type::INSERT) {
                if (!start1_set) {
                    start1 = e.idx1;
                    start1_set = true;
                }
                ++count1;
            }
            if (e.type != Edit::Type::DELETE) {
                if (!start2_set) {
                    start2 = e.idx2;
                    start2_set = true;
                }
                ++count2;
            }
        }

        const size_t header_start1 = start1_set ? start1 + 1 : 0;
        const size_t header_start2 = start2_set ? start2 + 1 : 0;

        ts << "@@ -" << static_cast<uint64_t>(header_start1);
        if (count1 != 1) {
            ts << "," << static_cast<uint64_t>(count1);
        }
        ts << " +" << static_cast<uint64_t>(header_start2);
        if (count2 != 1) {
            ts << "," << static_cast<uint64_t>(count2);
        }
        ts << " @@\n";

        for (size_t k = hunk_start; k < hunk_end; ++k) {
            const Edit &e = edits[k];
            if (e.type == Edit::Type::EQUAL) {
                ts << " " << lines1[e.idx1] << "\n";
            } else if (e.type == Edit::Type::DELETE) {
                ts << "-" << lines1[e.idx1] << "\n";
            } else {
                ts << "+" << lines2[e.idx2] << "\n";
            }
        }

        i = hunk_end;
    }

    ts.flush();
    std::fwrite(output_buf.const_char(), 1, output_buf.size(), stdout);

    return false;
}
