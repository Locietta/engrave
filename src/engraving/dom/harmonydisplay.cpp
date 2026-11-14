// SPDX-License-Identifier: GPL-3.0-only
#include "harmony.h"
#include "staff.h"

using namespace mu::engraving;

namespace {
using Action = Harmony::DisplayAction;
using Kind = Action::Kind;

struct DisplayBuilder {
    const Harmony &harmony;
    std::vector<Action> actions;
    int tpc = Tpc::TPC_INVALID;
    NoteSpellingType spelling = NoteSpellingType::STANDARD;
    NoteCaseType note_case = NoteCaseType::AUTO;
    double scale = 1.0;

    void text(String value, int font_idx = 0, bool literal = false) {
        actions.push_back({Kind::TEXT, {}, value, font_idx, scale, literal});
    }

    void action(const RenderActionPtr &command) {
        const ChordList *list = harmony.score()->chord_list();
        switch (command->action_type()) {
            case RenderAction::RenderActionType::SET: {
                const auto set = std::static_pointer_cast<RenderActionSet>(command);
                const ChordSymbol symbol = list->symbol(set->text());
                actions.push_back(
                    {Kind::SET, command, symbol.is_valid() ? symbol.value : set->text(), symbol.is_valid() ? symbol.font_idx : 0, scale});
                break;
            }
            case RenderAction::RenderActionType::NOTE: {
                if (!tpc_is_valid(tpc)) break;
                String note;
                AccidentalVal accidental;
                if (harmony.harmony_type() == HarmonyType::STANDARD) {
                    tpc2name(tpc, spelling, note_case, note, accidental);
                } else if (harmony.harmony_type() == HarmonyType::NASHVILLE) {
                    String acc;
                    const Staff *staff = harmony.staff();
                    tpc2_function(tpc, staff ? staff->key(harmony.tick()) : Key::INVALID, acc, note);
                }
                if (note.empty()) break;
                ChordSymbol symbol = list->symbol(u"note" + note);
                if (!symbol.is_valid()) symbol = list->symbol(note);
                text(symbol.is_valid() ? symbol.value : note, symbol.is_valid() ? symbol.font_idx : 0);
                break;
            }
            case RenderAction::RenderActionType::ACCIDENTAL: {
                if (!tpc_is_valid(tpc)) break;
                String note, acc;
                if (harmony.harmony_type() == HarmonyType::STANDARD) {
                    tpc2name(tpc, spelling, note_case, note, acc);
                } else if (harmony.harmony_type() == HarmonyType::NASHVILLE) {
                    const Staff *staff = harmony.staff();
                    tpc2_function(tpc, staff ? staff->key(harmony.tick()) : Key::INVALID, acc, note);
                }
                if (acc.empty()) break;
                const ChordToken token = list->token(acc, ChordTokenClass::ACCIDENTAL);
                if (token.is_valid()) {
                    for (const auto &child : token.render_list) action(child);
                    break;
                }
                String context = tpc == Tpc::TPC_B_B && spelling == NoteSpellingType::GERMAN ? u"german_B" : u"accidental";
                ChordSymbol symbol = list->symbol(context + acc);
                if (!symbol.is_valid()) symbol = list->symbol(acc);
                text(symbol.is_valid() ? symbol.value : note, symbol.is_valid() ? symbol.font_idx : 0);
                break;
            }
            case RenderAction::RenderActionType::SCALE: scale *= std::static_pointer_cast<RenderActionScale>(command)->scale(); break;
            case RenderAction::RenderActionType::PAREN: actions.push_back({Kind::PAREN, command}); break;
            default: actions.push_back({Kind::COMMAND, command, {}, 0, scale}); break;
        }
    }

    void render(const std::vector<RenderActionPtr> &commands, int pitch, NoteSpellingType note_spelling = NoteSpellingType::STANDARD,
                NoteCaseType note_case_value = NoteCaseType::AUTO, double mag = 1.0) {
        tpc = pitch;
        spelling = note_spelling;
        note_case = note_case_value;
        scale = mag;
        actions.push_back({Kind::RESET});
        for (const auto &command : commands) action(command);
    }

    void single(HarmonyInfo *info, int root, int bass) {
        const ChordList *list = info->chord_list();
        if (!list) return;
        const auto &style = harmony.style();
        const auto spelling = style.style_v(Sid::CHORD_SYMBOL_SPELLING).value<NoteSpellingType>();
        const auto root_case = harmony.root_render_case(info);
        const auto bass_case = harmony.bass_render_case();
        const ChordDescription *description = info->descr();
        const bool stacked = style.style_b(Sid::VERTICALLY_STACK_MODIFIERS) && !harmony.do_not_stack_modifiers();
        if (harmony.harmony_type() == HarmonyType::STANDARD && tpc_is_valid(root)) {
            render(list->render_list_root, root, spelling, root_case);
            if (description) render(stacked ? description->render_list_stacked : description->render_list, 0);
        } else if (harmony.harmony_type() == HarmonyType::NASHVILLE && tpc_is_valid(root)) {
            render(list->render_list_function, root, spelling, bass_case);
            actions.push_back({Kind::NOMINAL_ADJUST});
            if (description) render(stacked ? description->render_list_stacked : description->render_list, 0);
        } else {
            text(info->text_name(), 0, true);
        }
        if (tpc_is_valid(bass)) {
            const auto &commands = style.style_b(Sid::CHORD_BASS_NOTE_STAGGER) ? list->render_list_bass_offset : list->render_list_bass;
            static const std::wregex pattern_69(L"6[,/]?9");
            const bool is69 = info->text_name().contains(pattern_69);
            const bool modifier_stack = stacked && info->parsed_chord() && info->parsed_chord()->modifier_list().size() > 1;
            if (modifier_stack || is69)
                render({std::make_shared<RenderActionMove>(0.05, 0.0)}, bass, spelling, bass_case, harmony.bass_scale());
            render(commands, bass, spelling, bass_case, harmony.bass_scale());
        }
    }

    void chord(HarmonyInfo *info) {
        const auto &style = harmony.style();
        const int root = info->root_tpc(), bass = info->bass_tpc();
        const auto display = style.style_v(Sid::DISPLAY_CAPO_CHORDS).value<DisplayCapoChordType>();
        if (display == DisplayCapoChordType::BOTH || display == DisplayCapoChordType::CONCERT) single(info, root, bass);
        const int capo = style.style_i(Sid::CAPO_POSITION);
        if (!capo) return;
        int capo_root = Tpc::TPC_INVALID, capo_bass = Tpc::TPC_INVALID;
        if (tpc_is_valid(root) && capo > 0 && capo < 12) {
            const int offsets[] = {0, 5, -2, 3, -4, 1, 6, -1, 4, -3, 2, -5};
            capo_root = root + offsets[capo];
            capo_bass = bass;
            if (tpc_is_valid(capo_bass)) capo_bass += offsets[capo];
            // Keep guitar roots and basses in the same enharmonic range as rendering.
            if (capo_root < 8 || (tpc_is_valid(capo_bass) && capo_bass < 6)) {
                capo_root += 12;
                if (tpc_is_valid(capo_bass)) capo_bass += 12;
            } else if (capo_root > 24 || (tpc_is_valid(capo_bass) && capo_bass > 26)) {
                capo_root -= 12;
                if (tpc_is_valid(capo_bass)) capo_bass -= 12;
            }
        }
        if (display == DisplayCapoChordType::BOTH) action(std::make_shared<RenderActionParenLeft>());
        if (display == DisplayCapoChordType::BOTH || display == DisplayCapoChordType::TRANSPOSED) single(info, capo_root, capo_bass);
        if (display == DisplayCapoChordType::BOTH) action(std::make_shared<RenderActionParenRight>());
    }
};
} // namespace

String Harmony::display_name(std::vector<std::vector<DisplayAction>> *output) const {
    std::vector<std::vector<DisplayAction>> groups;
    if (harmony_type() == HarmonyType::ROMAN) {
        if (!chords_.empty()) groups.push_back({{DisplayAction::Kind::TEXT, {}, chords_.front()->text_name(), 0, 1.0, true}});
    } else {
        for (auto i = chords_.rbegin(); i != chords_.rend(); ++i) {
            DisplayBuilder builder{*this};
            builder.chord(*i);
            groups.push_back(std::move(builder.actions));
        }
    }
    String name;
    for (const auto &group : groups) {
        for (const auto &action : group) {
            if (action.kind == DisplayAction::Kind::TEXT ||
                (action.kind == DisplayAction::Kind::SET && std::static_pointer_cast<RenderActionSet>(action.command)->render_text()))
                name += action.text;
            else if (action.kind == DisplayAction::Kind::PAREN)
                name += std::static_pointer_cast<RenderActionParen>(action.command)->direction() == DirectionH::LEFT ? u"(" : u")";
        }
    }
    name.replace(u"\uE87C", u"/");
    if (output) *output = std::move(groups);
    return name;
}
