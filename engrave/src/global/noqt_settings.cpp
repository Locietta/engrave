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

Settings::Key::Key(std::string moduleName, std::string key) : moduleName(std::move(moduleName)), key(std::move(key)) {}

bool Settings::Key::isNull() const { return key.empty(); }

bool Settings::Key::operator==(const Key &k) const { return key == k.key; }

bool Settings::Key::operator<(const Key &k) const { return key < k.key; }

const Settings::Items &Settings::items() const { return isTransactionStarted_ ? localSettings_ : items_; }

void Settings::load() {
    auto loaded = readItems();
    for (const auto &[k, v] : loaded) {
        items_[k] = v;
    }
}

void Settings::reload() { load(); }

void Settings::reset(bool keepDefaultSettings, bool notifyAboutChanges, bool notifyOtherInstances) {
    (void) keepDefaultSettings;
    (void) notifyOtherInstances;
    (void) notifyAboutChanges;
    localSettings_.clear();
    isTransactionStarted_ = false;
    for (auto &[k, item] : items_) {
        item.value = item.defaultValue;
    }
}

const Val &Settings::value(const Key &key) const { return findItem(key).value; }

const Val &Settings::defaultValue(const Key &key) const { return findItem(key).defaultValue; }

void Settings::setSharedValue(const Key &key, const Val &value) { setLocalValue(key, value); }

void Settings::setLocalValue(const Key &key, const Val &value) {
    if (key.isNull()) {
        return;
    }
    Item &item = findItem(key);
    if (item.isNull()) {
        insertNewItem(key, value);
    } else {
        if (item.value == value) {
            return;
        }
        item.value = value;
    }
    if (!isTransactionStarted_) {
        writeValue(key, value);
    }
}

void Settings::setDefaultValue(const Key &key, const Val &value) {
    Item &item = findItem(key);
    if (item.isNull()) {
        (isTransactionStarted_ ? localSettings_ : items_)[key] = Item{key, value, value, "", false, Val(), Val()};
    } else {
        item.defaultValue = value;
        item.value.setType(value.type());
    }
}

void Settings::setDescription(const Key &key, const std::string &value) {
    Item &item = findItem(key);
    if (!item.isNull()) {
        item.description = value;
    }
}

void Settings::setCanBeManuallyEdited(const Settings::Key &key, bool canBeManuallyEdited, const Val &minValue, const Val &maxValue) {
    Item &item = findItem(key);
    if (item.isNull()) {
        (isTransactionStarted_ ? localSettings_ : items_)[key] = Item{key, Val(), Val(), "", canBeManuallyEdited, minValue, maxValue};
    } else {
        item.canBeManuallyEdited = canBeManuallyEdited;
        item.minValue = minValue;
        item.maxValue = maxValue;
    }
}

void Settings::beginTransaction(bool notifyToOtherInstances) {
    (void) notifyToOtherInstances;
    if (isTransactionStarted_) {
        throw std::logic_error("Transaction is already started");
    }
    localSettings_ = items_;
    isTransactionStarted_ = true;
}

void Settings::commitTransaction(bool notifyToOtherInstances) {
    (void) notifyToOtherInstances;
    if (!isTransactionStarted_) return;
    items_ = std::move(localSettings_);
    isTransactionStarted_ = false;
    localSettings_.clear();
}

void Settings::rollbackTransaction(bool notifyToOtherInstances) {
    (void) notifyToOtherInstances;
    isTransactionStarted_ = false;
    localSettings_.clear();
}

io::path_t Settings::filePath() const { return io::path_t(); }

Settings::Item &Settings::findItem(const Key &key) const {
    Items &items = isTransactionStarted_ ? localSettings_ : items_;
    auto it = items.find(key);
    if (it == items.end()) {
        static Item null;
        return null;
    }
    return it->second;
}

void Settings::insertNewItem(const Settings::Key &key, const Val &value) {
    Item item = Item{key, value, value, "", false, Val(), Val()};
    if (isTransactionStarted_) {
        localSettings_[key] = item;
    } else {
        items_[key] = item;
    }
}

Settings::Items Settings::readItems() const { return items_; }

void Settings::writeValue(const Key &key, const Val &value) {
    (void) key;
    (void) value;
}

} // namespace muse
