// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "draw/internal/fontsdatabase.h"
#include "runtime_assets.h"

namespace music {
struct FontRecord {
    RuntimeFontInfo info;
    muse::draw::FontDataKey key;
    std::shared_ptr<const muse::ByteArray> bytes;
};

using ActiveTable = std::vector<FontRecord>;

inline const FontRecord *find_record(const ActiveTable &table, const muse::draw::FontDataKey &key) {
    for (const FontRecord &record : table) {
        // FontDataKey equality: case-insensitive family, exact bold/italic.
        if (record.key == key) {
            return &record;
        }
    }
    return nullptr;
}

inline const FontRecord *find_regular_family_fallback(const ActiveTable &table, const muse::draw::FontDataKey &key) {
    // Families such as Ubuntu ship a Medium face that FreeType flags as bold,
    // so a regular query has no exact record. Upstream/desktop resolves by
    // family and the closest available style; choose an upright face of the
    // requested family when the exact regular style is absent. Bold/italic
    // queries keep exact-key semantics (no synthetic styling is invented).
    if (key.bold() || key.italic()) {
        return nullptr;
    }
    const FontRecord *italic = nullptr;
    for (const FontRecord &record : table) {
        if (record.key.family() != key.family()) {
            continue;
        }
        if (!record.key.italic()) {
            return &record;
        }
        if (!italic) {
            italic = &record;
        }
    }
    return italic;
}

struct RuntimeFontsDatabase {

    using Font = muse::draw::Font;
    using FontDataKey = muse::draw::FontDataKey;
    using String = muse::String;

    void set_default_font(Font::Type type, const FontDataKey &key) { return database_.set_default_font(type, key); }

    void insert_substitution(const String &f1, const String &substitute_name) { return database_.insert_substitution(f1, substitute_name); }

    void remove_substitutions(const String &f1, const std::vector<String> &substitute_names) {
        return database_.remove_substitutions(f1, substitute_names);
    }

    int add_font(const FontDataKey &key, const muse::io::PathT &path) { return database_.add_font(key, path); }

    int add_font(const FontDataKey &key, std::shared_ptr<muse::io::IODevice> device) { return database_.add_font(key, device); }

    void remove_font(const FontDataKey &key) { return database_.remove_font(key); }

    uint64_t revision() const { return database_.revision(); }

    explicit RuntimeFontsDatabase(std::shared_ptr<const ActiveTable> active) : active_(std::move(active)) {}

    ~RuntimeFontsDatabase() = default;

    void publish(std::shared_ptr<const ActiveTable> active) noexcept { active_.swap(active); }

    std::shared_ptr<const ActiveTable> active_table() const { return active_; }

    muse::draw::FontDataKey actual_font(const muse::draw::FontDataKey &require_key, muse::draw::Font::Type type) const {
        if (const FontRecord *exact = find_record(*active_, require_key)) {
            return exact->key;
        }

        const muse::draw::FontDataKey base_key = database_.actual_font(require_key, type);
        if (const FontRecord *overridden = find_record(*active_, base_key)) {
            return overridden->key;
        }

        // Built-in families keep priority; only a family the upstream database
        // cannot provide at all falls back to its registered runtime face.
        // FontsEngine normalises Undefined/Unknown to Text before querying, so
        // this fallback must not apply to symbol/music faces.
        if (type == muse::draw::Font::Type::TEXT && base_key.family() != require_key.family()) {
            if (const FontRecord *fallback = find_regular_family_fallback(*active_, require_key)) {
                return fallback->key;
            }
        }

        // Only the upstream database's own default fallback is accepted here
        // when no registered face can represent the requested family.
        return base_key;
    }

    std::vector<muse::draw::FontDataKey> substitution_fonts(const muse::draw::FontDataKey &require_key) const {
        std::vector<muse::draw::FontDataKey> result = database_.substitution_fonts(require_key);
        // 5.0 keys the substitution list by family instead of by font type, so
        // the 4.7.6 rule "every registered runtime face is a text fallback" has
        // no type to gate on. Keep the registered faces as extra glyph-fallback
        // candidates: the primary face is always tried first, so this only
        // changes codepoints it cannot render (for example CJK text in a score
        // that names a Latin-only family).
        result.reserve(result.size() + active_->size());
        for (const FontRecord &record : *active_) {
            if (std::find(result.begin(), result.end(), record.key) == result.end()) {
                result.push_back(record.key);
            }
        }
        return result;
    }

    muse::draw::FontData font_data(const muse::draw::FontDataKey &require_key, muse::draw::Font::Type type) const {
        const muse::draw::FontDataKey actual_key = actual_font(require_key, type);
        if (const FontRecord *record = find_record(*active_, actual_key)) {
            muse::draw::FontData data;
            data.key = record->key;
            data.data = *record->bytes;
            return data;
        }
        return database_.font_data(require_key, type);
    }

private:
    muse::draw::FontsDatabase database_;
    std::shared_ptr<const ActiveTable> active_;
};
} // namespace music
