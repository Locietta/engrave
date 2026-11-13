#pragma once
#include <functional>
#include <memory>
#include <span>
#include <variant>

#include "dom/score.h"
#include "global/types/translatablestring.h"

namespace mu::engraving {
struct Part;
struct Staff;
struct MStaff;
struct SharedPart;
struct StringTunings;
struct StringData;
struct Instrument;
struct Excerpt;
struct BracketItem;
struct TextBase;
struct Segment;
struct Measure;
struct MeasureBase;
struct NoteParenthesisInfo;

struct StyleValue {
    Sid id;
    PropertyValue value;
};

struct DetachedRoot {
    SourceHandle owner;
    std::variant<SourceHandle, MStaff *, Excerpt *, NoteParenthesisInfo *, const RangeLock *> object;
};

void defer_root(EngravingObject &owner, decltype(DetachedRoot::object) object);

void apply_property(EngravingObject &object, Pid id, const PropertyValue &value, PropertyFlags flags, bool reset_path);
void apply_text_line_property(EngravingObject &object, const PropertyValue &value);
void apply_text_format(TextBase &text, Pid id, const PropertyValue &value, PropertyFlags flags);
void apply_part_membership(Part &part, size_t index, bool adding);
void apply_part_label(Part &part, Fraction tick, const String &value, bool long_name);
void exchange_part_instrument(Part &part, std::unique_ptr<Instrument> &held);
void refresh_part_instrument(Part &part);
void apply_staff_membership(Staff &staff, staff_idx_t index, bool adding);
void apply_system_staff(Staff &staff, bool adding);
void apply_staff_hide(Measure &measure, staff_idx_t staff, AutoOnOff value);
void apply_staff_brackets(Staff &staff, size_t level, BracketType type, size_t span, bool adding);
void apply_staff_tracks(Measure &measure, staff_idx_t first, staff_idx_t last, bool adding);
bool style_value_refreshes_chords(Sid id);
void apply_style_values(Score &score, std::span<const StyleValue> values);
void apply_style_replacement(Score &score, const MStyle &style, bool overlap = false);

struct Transaction {
    void change_property(EngravingObject *object, Pid id, const PropertyValue &value, PropertyFlags flags = PropertyFlags::NOSTYLE,
                         bool reset_path = false);
    void change_style_values(Score &score, std::span<const StyleValue> values);
    void replace_style(Score &score, const MStyle &style, bool overlap = false);
    void change_bracket_property(BracketItem &item, Pid id, const PropertyValue &value, PropertyFlags flags);
    void change_text_line_property(EngravingObject &object, const PropertyValue &value);
    void change_text_format(TextBase &text, Pid id, const PropertyValue &value, PropertyFlags flags);
    void add_element(EngravingItem *item);
    void remove_element(EngravingItem *item);
    void replace_element(EngravingItem *before, EngravingItem *after);
    void reparent_element(EngravingItem *item, EngravingItem *parent, staff_idx_t staff);
    void reparent_segment(Segment *item, Measure *parent, Fraction tick);
    void link_objects(EngravingObject *item, EngravingObject *peer);
    void unlink_object(EngravingObject *item);
    void change_mstaff(Measure *measure, MStaff *row, staff_idx_t index, bool adding);
    void add_mstaff(Measure *measure, std::unique_ptr<MStaff> row, staff_idx_t index);
    void change_staff_tracks(Measure *measure, staff_idx_t first, staff_idx_t last, bool adding);
    void change_staff_brackets(Staff *staff, size_t level, BracketType type, size_t span, bool adding);
    void change_staff_membership(Staff *staff, staff_idx_t index, bool adding);
    void change_system_staff(Staff *staff, bool adding);
    void change_staff_order(Score *score, const std::vector<staff_idx_t> &order);
    void change_staff_hide(Measure *measure, staff_idx_t staff, AutoOnOff value);

    void change_part_membership(Part *part, size_t index, bool adding);
    void change_part_label(Fraction tick, Part *part, const String &value, bool long_name);
    void replace_part_instrument(Part *part, std::unique_ptr<Instrument> value);
    void change_string_data(StringTunings *item, const StringData &value);
    void change_shared_origin(SharedPart *shared, Part *origin, bool connecting);
    void change_excerpt_membership(Excerpt *excerpt, bool adding);

    void insert_time(Score &score, Fraction tick, Fraction length, bool unmanaged = false);
    void change_measures(MeasureBase *first, MeasureBase *last, bool adding, bool move_staff_type_changes = true);
    void change_measure_length(Measure *measure, Fraction length);
    void change_measure_repeat_count(Measure *measure, int count, staff_idx_t staff);
};

struct TransactionManager {
    explicit TransactionManager(MasterScore *master) : master_score_(master) {}

    ~TransactionManager();
    void transaction(const muse::TranslatableString &description, std::function<void(Transaction &)> func);
    void begin_transaction(const muse::TranslatableString &description);
    void end_transaction(bool failed = false);
    ScoreChanges prepare_commit();
    void commit_prepared(const ScoreChanges &changes);

    Transaction *current_transaction() { return active_ ? &transaction_ : nullptr; }

    Transaction &current_or_dummy_transaction() { return transaction_; }

    void cleanup_detached();
    std::vector<DetachedRoot> detached_roots;
private:
    MasterScore *master_score_;
    Transaction transaction_;
    bool active_ = false;
};
} // namespace mu::engraving
