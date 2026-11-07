#include "settings.h"

#include <stdexcept>
#include <utility>

namespace muse {

Settings *Settings::instance() {
    static Settings s;
    return &s;
}

Settings::Settings() = default;
Settings::~Settings() = default;

Settings::Key::Key(std::string module_name, std::string key) : module_name(std::move(module_name)), key(std::move(key)) {}

bool Settings::Key::is_null() const { return key.empty(); }

bool Settings::Key::operator==(const Key &k) const { return key == k.key; }

bool Settings::Key::operator<(const Key &k) const { return key < k.key; }

const Settings::Items &Settings::items() const { return is_transaction_started_ ? local_settings_ : items_; }

void Settings::load() {
    auto loaded = read_items();
    for (const auto &[k, v] : loaded) {
        items_[k] = v;
    }
}

void Settings::reload() { load(); }

void Settings::reset(bool keep_default_settings, bool notify_about_changes, bool notify_other_instances) {
    (void) keep_default_settings;
    (void) notify_other_instances;
    (void) notify_about_changes;
    local_settings_.clear();
    is_transaction_started_ = false;
    for (auto &[k, item] : items_) {
        item.value = item.default_value;
    }
}

const Val &Settings::value(const Key &key) const { return find_item(key).value; }

const Val &Settings::default_value(const Key &key) const { return find_item(key).default_value; }

void Settings::set_shared_value(const Key &key, const Val &value) { set_local_value(key, value); }

void Settings::set_local_value(const Key &key, const Val &value) {
    if (key.is_null()) {
        return;
    }
    Item &item = find_item(key);
    if (item.is_null()) {
        insert_new_item(key, value);
    } else {
        if (item.value == value) {
            return;
        }
        item.value = value;
    }
    if (!is_transaction_started_) {
        write_value(key, value);
    }
}

void Settings::set_default_value(const Key &key, const Val &value) {
    Item &item = find_item(key);
    if (item.is_null()) {
        (is_transaction_started_ ? local_settings_ : items_)[key] = Item{key, value, value, "", false, Val(), Val()};
    } else {
        item.default_value = value;
        item.value.set_type(value.type());
    }
}

void Settings::set_description(const Key &key, const std::string &value) {
    Item &item = find_item(key);
    if (!item.is_null()) {
        item.description = value;
    }
}

void Settings::set_can_be_manually_edited(const Settings::Key &key, bool can_be_manually_edited, const Val &min_value,
                                          const Val &max_value) {
    Item &item = find_item(key);
    if (item.is_null()) {
        (is_transaction_started_ ? local_settings_ : items_)[key] =
            Item{key, Val(), Val(), "", can_be_manually_edited, min_value, max_value};
    } else {
        item.can_be_manually_edited = can_be_manually_edited;
        item.min_value = min_value;
        item.max_value = max_value;
    }
}

void Settings::begin_transaction(bool notify_to_other_instances) {
    (void) notify_to_other_instances;
    if (is_transaction_started_) {
        throw std::logic_error("Transaction is already started");
    }
    local_settings_ = items_;
    is_transaction_started_ = true;
}

void Settings::commit_transaction(bool notify_to_other_instances) {
    (void) notify_to_other_instances;
    if (!is_transaction_started_) return;
    items_ = std::move(local_settings_);
    is_transaction_started_ = false;
    local_settings_.clear();
}

void Settings::rollback_transaction(bool notify_to_other_instances) {
    (void) notify_to_other_instances;
    is_transaction_started_ = false;
    local_settings_.clear();
}

io::PathT Settings::file_path() const { return io::PathT(); }

Settings::Item &Settings::find_item(const Key &key) const {
    Items &items = is_transaction_started_ ? local_settings_ : items_;
    auto it = items.find(key);
    if (it == items.end()) {
        static Item null;
        return null;
    }
    return it->second;
}

void Settings::insert_new_item(const Settings::Key &key, const Val &value) {
    Item item = Item{key, value, value, "", false, Val(), Val()};
    if (is_transaction_started_) {
        local_settings_[key] = item;
    } else {
        items_[key] = item;
    }
}

Settings::Items Settings::read_items() const { return items_; }

void Settings::write_value(const Key &key, const Val &value) {
    (void) key;
    (void) value;
}

} // namespace muse
