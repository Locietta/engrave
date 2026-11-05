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

#include "../infrastructure/ifileinfoprovider.h"
#include "../infrastructure/eidregister.h"

#include "instrument.h"
#include "score.h"

namespace mu::engraving {
class EngravingProject;
class MscReader;
class MscWriter;
class MscLoader;
class TransactionManager;
} // namespace mu::engraving

namespace mu::engraving::compat {
class ScoreAccess;
class ReadStyleHook;
} // namespace mu::engraving::compat

namespace mu::engraving {
class Excerpt;
class MasterScore;
class Part;
class RepeatList;
struct RepeatSegmentInfo;
class Revisions;
class TempoTimeline;
class TimeSigMap;
class UndoStack;
class ScoreAutomationController;

class MidiMapping {
public:
    Part *part() { return part_; }

    const Part *part() const { return part_; }

    InstrChannel *articulation() { return articulation_.get(); }

    const InstrChannel *articulation() const { return articulation_.get(); }

    signed char port() const { return port_; }

    signed char channel() const { return channel_; }

private:

    MidiMapping() = default; // should be created only within MasterScore
    friend class MasterScore;

    Part *part_ = nullptr;
    std::unique_ptr<InstrChannel> articulation_;
    signed char port_ = 0;
    signed char channel_ = 0;
    InstrChannel *masterChannel_ = nullptr;
    PartChannelSettingsLink link_;
};

class MasterScore : public Score {
    OBJECT_ALLOCATOR(engraving, MasterScore)

public:

    ~MasterScore();
    MasterScore *clone();

    Score *createScore();
    Score *createScore(const MStyle &s);

    std::weak_ptr<EngravingProject> project() const { return project_; }

    bool isMaster() const override { return true; }

    EIDRegister *eidRegister() { return &eidRegister_; }

    const EIDRegister *eidRegister() const { return &eidRegister_; }

    bool readOnly() const override { return readOnly_; }

    void setReadOnly(bool ro) { readOnly_ = ro; }

    TransactionManager *transactionManager() const { return transactionManager_.get(); }

    UndoStack *undoStack() const { return undoStack_; }

    TimeSigMap *sigmap() const override { return sigmap_; }

    void addChangesListener(IScoreChangesListener *listener);
    void removeChangesListener(IScoreChangesListener *listener);
    void notifyChanges(const ScoreChanges &changes);

    AutomationDataConstPtr automationData() const override;
    void setAutomationData(AutomationDataPtr data);
    void editAutomationPoints(const AutomationCurveKey &key, AutomationPointEdits &edits, bool undoable = true) override;

    /// Always call this before calling `repeatList()`
    /// No need to set it back after use, because everyone always calls it before using `repeatList()`
    void setExpandRepeats(bool expandRepeats);

    bool expandRepeats() const { return expandRepeats_; }

    void updateRepeatListTempo();
    void updateRepeatList();

    const RepeatList &repeatList() const;
    const RepeatList &repeatList(bool expandRepeats, bool updateTies = true) const;

    void invalidateRepeatList();

    const TempoTimeline &tempoTimeline() const;
    const TempoTimeline &tempoTimeline(bool expandRepeats) const;
    bool setTempoMultiplier(BeatsPerSecond val);
    void setTempoTimelineOverride(std::optional<TempoTimeline> timeline);

    std::vector<Excerpt *> &excerpts() { return excerpts_; }

    const std::vector<Excerpt *> &excerpts() const { return excerpts_; }

    void setUpdateAll() override;

    void setLayoutAll(staff_idx_t staff = muse::nidx, const EngravingItem *e = nullptr);
    void setLayout(const Fraction &tick, staff_idx_t staff, const EngravingItem *e = nullptr);
    void setLayout(const Fraction &tick1, const Fraction &tick2, staff_idx_t staff1, staff_idx_t staff2, const EngravingItem *e = nullptr);

    CmdState &cmdState() override { return cmdState_; }

    const CmdState &cmdState() const override { return cmdState_; }

    void addLayoutFlags(LayoutFlags val) override { cmdState_.layoutFlags |= val; }

    void setInstrumentsChanged(bool val) override { cmdState_.instrumentsChanged = val; }

    void setExcerptsChanged(bool val) { cmdState_.excerptsChanged = val; }

    bool excerptsChanged() const { return cmdState_.excerptsChanged; }

    bool instrumentsChanged() const { return cmdState_.instrumentsChanged; }

    void update() { update(true); }

    void lockUpdates(bool locked);

    int midiPortCount() const { return midiPortCount_; }

    void setMidiPortCount(int val) { midiPortCount_ = val; }

    std::vector<MidiMapping> &midiMapping() { return midiMapping_; }

    MidiMapping *midiMapping(int channel) { return &midiMapping_[channel]; }

    void addMidiMapping(InstrChannel *channel, Part *part, int midiPort, int midiChannel);
    void updateMidiMapping(InstrChannel *channel, Part *part, int midiPort, int midiChannel);

    int midiPort(int idx) const { return midiMapping_[idx].port(); }

    int midiChannel(int idx) const { return midiMapping_[idx].channel(); }

    void rebuildMidiMapping();
    void checkMidiMapping();

    bool exportMidiMapping() { return !isSimpleMidiMapping_; }

    int getNextFreeMidiMapping(std::set<int> &occupiedMidiChannels, unsigned int &searchMidiMappingFrom, int p = -1, int ch = -1);
    int getNextFreeDrumMidiMapping(std::set<int> &occupiedMidiChannels);
    void rebuildAndUpdateExpressive(Synthesizer *synth);
    void updateExpressive(Synthesizer *synth);
    void updateExpressive(Synthesizer *synth, bool expressive, bool force = false);

    using Score::loopBoundaryTick;
    Fraction loopBoundaryTick(LoopBoundaryType type) const;
    void setLoopBoundaryTick(LoopBoundaryType type, Fraction tick);

    void addExcerpt(Excerpt *, size_t index = muse::nidx);
    void removeExcerpt(Excerpt *);
    void deleteExcerpt(Excerpt *);

    void initAndAddExcerpt(Excerpt *, bool);
    void initExcerpt(Excerpt *);
    void initEmptyExcerpt(Excerpt *);

    void initAutomation(); // TODO: Placeholder?

    void setPlaybackScore(Score *);

    Score *playbackScore() { return playbackScore_; }

    const Score *playbackScore() const { return playbackScore_; }

    InstrChannel *playbackChannel(const InstrChannel *c) { return midiMapping_[c->channel()].articulation(); }

    const InstrChannel *playbackChannel(const InstrChannel *c) const { return midiMapping_[c->channel()].articulation(); }

    MasterScore *unrollRepeats();

    MeasureBase *insertMeasure(MeasureBase *beforeMeasure = nullptr, const InsertMeasureOptions &options = InsertMeasureOptions());

    IFileInfoProviderPtr fileInfo() const;
    void setFileInfoProvider(IFileInfoProviderPtr fileInfoProvider);

    String name() const override;

    muse::Ret sanityCheck();

private:
    void update(bool resetCmdState, bool layoutAllParts = false);

    void updateAutomation(const ScoreChanges &changes);

    void onTimeInserted(const Fraction &tick, const Fraction &len, const std::vector<RepeatSegmentInfo> &oldSegments) override;

    void reorderMidiMapping();
    void rebuildExcerptsMidiMapping();
    void removeDeletedMidiMapping();

    int updateMidiMapping();
    void doUpdateMidiMapping(int &maxport, std::set<int> &occupiedMidiChannels, unsigned int &searchMidiMappingFrom, Part *part,
                             InstrChannel *channel, bool useDrumset);

    friend class EngravingProject;
    friend class compat::ScoreAccess;
    friend class read114::Read114;
    friend class read400::Read400;
    friend class TransactionManager;

    MasterScore(const EngravingServices &services, std::weak_ptr<EngravingProject> project = std::weak_ptr<EngravingProject>());
    MasterScore(const EngravingServices &services, const MStyle &,
                std::weak_ptr<EngravingProject> project = std::weak_ptr<EngravingProject>());

    void initParts(Excerpt *);

    EIDRegister eidRegister_;
    std::unique_ptr<TransactionManager> transactionManager_;
    UndoStack *undoStack_ = nullptr;
    TimeSigMap *sigmap_ = nullptr;
    RepeatList *expandedRepeatList_ = nullptr;
    RepeatList *nonExpandedRepeatList_ = nullptr;
    ScoreAutomationController *automationController_ = nullptr;
    bool expandRepeats_ = true;

    std::vector<Excerpt *> excerpts_;
    std::vector<PartChannelSettingsLink> playbackSettingsLinks_;
    Score *playbackScore_ = nullptr;
    std::vector<IScoreChangesListener *> changesListeners_;

    bool readOnly_ = false;

    CmdState cmdState_; // modified during cmd processing
    bool updatesLocked_ = false;

    std::array<Fraction, 2> loopBoundaries_; ///< 0 - LoopIn, 1 - LoopOut

    int midiPortCount_ = 0; // A count of ALSA midi out ports
    std::vector<MidiMapping> midiMapping_;
    bool isSimpleMidiMapping_ = false; // midi mapping is simple if all ports and channels
    // don't decrease and don't have gaps

    std::weak_ptr<EngravingProject> project_;

    // FIXME: Move to EngravingProject
    // We can't yet, because project_ is not set on every MasterScore
    IFileInfoProviderPtr fileInfoProvider_;
};
} // namespace mu::engraving
