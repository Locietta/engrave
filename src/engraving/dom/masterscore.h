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

#pragma once

#include <array>
#include <memory>

#include "../infrastructure/eidregister.h"
#include "engraving/infrastructure/localfileinfoprovider.h"
#include "instrument.h"
#include "score.h"

namespace mu::engraving {
struct EngravingProject;
struct MscReader;
struct MscWriter;
struct MscLoader;
struct TransactionManager;
} // namespace mu::engraving

namespace mu::engraving::compat {
struct ScoreAccess;
struct ReadStyleHook;
} // namespace mu::engraving::compat

namespace mu::engraving {
struct Excerpt;
struct MasterScore;
struct Part;
struct RepeatList;
struct RuntimeTempoState;
struct RepeatSegmentInfo;
struct Revisions;
struct TempoTimeline;
struct TimeSigMap;
struct UndoStack;
struct ScoreAutomationController;

struct MidiMapping {
    Part *part() { return part_; }

    const Part *part() const { return part_; }

    InstrChannel *articulation() { return articulation_.get(); }

    const InstrChannel *articulation() const { return articulation_.get(); }

    signed char port() const { return port_; }

    signed char channel() const { return channel_; }

private:

    MidiMapping() = default; // should be created only within MasterScore
    friend struct MasterScore;

    Part *part_ = nullptr;
    std::unique_ptr<InstrChannel> articulation_;
    signed char port_ = 0;
    signed char channel_ = 0;
    InstrChannel *master_channel_ = nullptr;
    PartChannelSettingsLink link_;
};

struct MasterScore : Score {

public:

    ~MasterScore();
    MasterScore *clone();

    Score *create_score();
    Score *create_score(const MStyle &s);

    std::weak_ptr<EngravingProject> project() const { return project_; }

    bool is_master() const override { return true; }

    EIDRegister *eid_register() { return &eid_register_; }

    const EIDRegister *eid_register() const { return &eid_register_; }

    bool read_only() const override { return read_only_; }

    void set_read_only(bool ro) { read_only_ = ro; }

    TransactionManager *transaction_manager() const { return transaction_manager_.get(); }

    UndoStack *undo_stack() const { return undo_stack_; }

    TimeSigMap *sigmap() const override { return sigmap_; }

    void add_changes_listener(PlaybackModel *listener);
    void remove_changes_listener(PlaybackModel *listener);
    void notify_changes(const ScoreChanges &changes);

    AutomationDataConstPtr automation_data() const override;
    void set_automation_data(AutomationDataPtr data);
    void edit_automation_points(const AutomationCurveKey &key, AutomationPointEdits &edits, bool undoable = true) override;

    /// Always call this before calling `repeatList()`
    /// No need to set it back after use, because everyone always calls it before using `repeatList()`
    void set_expand_repeats(bool expand_repeats);

    bool expand_repeats() const { return expand_repeats_; }

    void update_repeat_list_tempo();
    // Rebuild both lists before optionally recording stale tie-jump repairs.
    void update_repeat_list(bool update_ties = true);

    const RepeatList &repeat_list() const;
    const RepeatList &repeat_list(bool expand_repeats) const;

    void invalidate_repeat_list();

    const TempoTimeline &tempo_timeline() const;
    const TempoTimeline &tempo_timeline(bool expand_repeats) const;
    bool set_tempo_multiplier(BeatsPerSecond val);
    void set_tempo_timeline_override(std::optional<TempoTimeline> timeline);
    RuntimeTempoState runtime_tempo_state() const;
    void restore_runtime_tempo_state(RuntimeTempoState state);

    std::vector<Excerpt *> &excerpts() { return excerpts_; }

    const std::vector<Excerpt *> &excerpts() const { return excerpts_; }

    void set_update_all() override;

    void set_layout_all(staff_idx_t staff = muse::nidx, const EngravingItem *e = nullptr);
    void set_layout(const Fraction &tick, staff_idx_t staff, const EngravingItem *e = nullptr);
    void set_layout(const Fraction &tick1, const Fraction &tick2, staff_idx_t staff1, staff_idx_t staff2, const EngravingItem *e = nullptr);

    CmdState &cmd_state() override { return cmd_state_; }

    const CmdState &cmd_state() const override { return cmd_state_; }

    void add_layout_flags(LayoutFlags val) override;

    void set_instruments_changed(bool val) override { cmd_state_.instruments_changed = val; }

    void set_excerpts_changed(bool val) { cmd_state_.excerpts_changed = val; }

    bool excerpts_changed() const { return cmd_state_.excerpts_changed; }

    bool instruments_changed() const { return cmd_state_.instruments_changed; }

    void update() { update(true); }

    void lock_updates(bool locked);

    int midi_port_count() const { return midi_port_count_; }

    void set_midi_port_count(int val) { midi_port_count_ = val; }

    std::vector<MidiMapping> &midi_mapping() { return midi_mapping_; }

    MidiMapping *midi_mapping(int channel) { return &midi_mapping_[channel]; }

    void add_midi_mapping(InstrChannel *channel, Part *part, int midi_port, int midi_channel);
    void update_midi_mapping(InstrChannel *channel, Part *part, int midi_port, int midi_channel);

    int midi_port(int idx) const { return midi_mapping_[idx].port(); }

    int midi_channel(int idx) const { return midi_mapping_[idx].channel(); }

    void rebuild_midi_mapping();
    void check_midi_mapping();

    bool export_midi_mapping() { return !is_simple_midi_mapping_; }

    int get_next_free_midi_mapping(std::set<int> &occupied_midi_channels, unsigned int &search_midi_mapping_from, int p = -1, int ch = -1);
    int get_next_free_drum_midi_mapping(std::set<int> &occupied_midi_channels);
    void rebuild_and_update_expressive(Synthesizer *synth);
    void update_expressive(Synthesizer *synth);
    void update_expressive(Synthesizer *synth, bool expressive, bool force = false);

    using Score::loop_boundary_tick;
    Fraction loop_boundary_tick(LoopBoundaryType type) const;
    void set_loop_boundary_tick(LoopBoundaryType type, Fraction tick);

    void add_excerpt(Excerpt *, size_t index = muse::nidx);
    void remove_excerpt(Excerpt *);
    void delete_excerpt(Excerpt *);

    void init_and_add_excerpt(Excerpt *, bool);
    void init_excerpt(Excerpt *);
    void init_empty_excerpt(Excerpt *);

    void init_automation(); // TODO: Placeholder?

    void set_playback_score(Score *);

    Score *playback_score() { return playback_score_; }

    const Score *playback_score() const { return playback_score_; }

    InstrChannel *playback_channel(const InstrChannel *c) { return midi_mapping_[c->channel()].articulation(); }

    const InstrChannel *playback_channel(const InstrChannel *c) const { return midi_mapping_[c->channel()].articulation(); }

    MasterScore *unroll_repeats();

    MeasureBase *insert_measure(MeasureBase *before_measure = nullptr, const InsertMeasureOptions &options = InsertMeasureOptions());

    LocalFileInfoProviderPtr file_info() const;
    void set_file_info_provider(LocalFileInfoProviderPtr file_info_provider);

    String name() const override;

    muse::Ret sanity_check();

private:
    void update(bool reset_cmd_state, bool layout_all_parts = false, NormalizeMode mode = NormalizeMode::TRANSACTION);

    void update_automation(const ScoreChanges &changes);

    void on_time_inserted(const Fraction &tick, const Fraction &len, const std::vector<RepeatSegmentInfo> &old_segments) override;

    void reorder_midi_mapping();
    void rebuild_excerpts_midi_mapping();
    void remove_deleted_midi_mapping();

    int update_midi_mapping();
    void do_update_midi_mapping(int &maxport, std::set<int> &occupied_midi_channels, unsigned int &search_midi_mapping_from, Part *part,
                                InstrChannel *channel, bool use_drumset);

    friend struct EngravingProject;
    friend struct compat::ScoreAccess;
    friend struct read114::Read114;
    friend struct read400::Read400;
    friend struct TransactionManager;

    MasterScore(const EngravingServices &services, std::weak_ptr<EngravingProject> project = std::weak_ptr<EngravingProject>());
    MasterScore(const EngravingServices &services, const MStyle &,
                std::weak_ptr<EngravingProject> project = std::weak_ptr<EngravingProject>());

    void init_parts(Excerpt *);

    EIDRegister eid_register_;
    std::unique_ptr<TransactionManager> transaction_manager_;
    UndoStack *undo_stack_ = nullptr;
    TimeSigMap *sigmap_ = nullptr;
    RepeatList *expanded_repeat_list_ = nullptr;
    RepeatList *non_expanded_repeat_list_ = nullptr;
    bool repeat_ties_dirty_ = false;
    ScoreAutomationController *automation_controller_ = nullptr;
    bool expand_repeats_ = true;

    std::vector<Excerpt *> excerpts_;
    std::vector<PartChannelSettingsLink> playback_settings_links_;
    Score *playback_score_ = nullptr;
    std::vector<PlaybackModel *> changes_listeners_;

    bool read_only_ = false;

    CmdState cmd_state_; // modified during cmd processing
    bool updates_locked_ = false;

    std::array<Fraction, 2> loop_boundaries_; ///< 0 - LoopIn, 1 - LoopOut

    int midi_port_count_ = 0; // A count of ALSA midi out ports
    std::vector<MidiMapping> midi_mapping_;
    bool is_simple_midi_mapping_ = false; // midi mapping is simple if all ports and channels
    // don't decrease and don't have gaps

    std::weak_ptr<EngravingProject> project_;

    // FIXME: Move to EngravingProject
    // We can't yet, because project_ is not set on every MasterScore
    LocalFileInfoProviderPtr file_info_provider_;
};
} // namespace mu::engraving
