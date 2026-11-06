/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2024 MuseScore Limited and others
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
#include "bendchorddurationsplitter.h"

#include "log.h"

using namespace mu::engraving;

namespace mu::iex::guitarpro {
constexpr int max_denominator_constant = 64;
constexpr Fraction one_dot_duration_multiplier(3, 2);
constexpr Fraction two_dots_duration_multiplier(7, 4);

static Fraction find_closest_displayable_duration(const Fraction &total_duration, const Fraction &target_duration,
                                                  int input_max_denominator, bool skip_first_best);
static std::vector<Fraction> fill_durations_by_proportions(const Fraction &total_duration, const std::vector<Fraction> &proportions,
                                                           int input_max_denominator, bool skip_first_best);
static bool can_be_represented_as_dotted_note(const Fraction &duration, int input_max_denominator);
static std::vector<Fraction> brute_force_split(const Fraction &total_duration, const std::vector<Fraction> &proportions,
                                               int input_max_denominator);
static std::vector<Fraction> brute_force_split_in_two(const Fraction &target, const std::vector<Fraction> &proportions,
                                                      const std::vector<Fraction> &durations);
static std::vector<Fraction> brute_force_split_in_three(const Fraction &target, const std::vector<Fraction> &durations);

static std::vector<Fraction> generate_durations(int input_max_denominator);

std::vector<Fraction> BendChordDurationSplitter::find_valid_note_split(const Fraction &total_duration,
                                                                       const std::vector<Fraction> &proportions,
                                                                       int input_max_denominator) {
    if (proportions.size() == 1) {
        std::vector<Fraction> unchanged_durations;
        unchanged_durations.push_back(total_duration);
        return unchanged_durations;
    }

    bool split_reached = false;
    bool skip_first_best = false;
    std::vector<Fraction> durations;

    do {
        durations = fill_durations_by_proportions(total_duration, proportions, input_max_denominator, skip_first_best);

        Fraction accumulated_duration = std::accumulate(durations.begin(), durations.end(), Fraction(0, 1));
        Fraction last_duration = total_duration - accumulated_duration;

        if (last_duration.is_not_zero() && !last_duration.negative() &&
            can_be_represented_as_dotted_note(last_duration, input_max_denominator)) {
            durations.push_back(last_duration);
            split_reached = true;
        } else if (!skip_first_best) {
            skip_first_best = true;
        } else {
            durations = brute_force_split(total_duration, proportions, input_max_denominator);
            split_reached = true;
        }
    } while (!split_reached);

    return durations;
}

static std::vector<Fraction> fill_durations_by_proportions(const Fraction &total_duration, const std::vector<Fraction> &proportions,
                                                           int input_max_denominator, bool skip_first_best) {
    std::vector<Fraction> durations;

    for (size_t i = 0; i < proportions.size() - 1; i++) {
        Fraction target_duration = total_duration * proportions[i];
        Fraction closest_duration =
            find_closest_displayable_duration(total_duration, target_duration, input_max_denominator, i == 0 ? skip_first_best : false);
        durations.push_back(closest_duration);
    }

    return durations;
}

static std::vector<Fraction> brute_force_split(const Fraction &total_duration, const std::vector<Fraction> &proportions,
                                               int input_max_denominator) {
    auto durations = generate_durations(input_max_denominator);
    std::sort(durations.begin(), durations.end());

    if (proportions.size() == 2) {
        return brute_force_split_in_two(total_duration, proportions, durations);
    }

    return brute_force_split_in_three(total_duration, durations);
}

static std::vector<Fraction> brute_force_split_in_two(const Fraction &target, const std::vector<Fraction> &proportions,
                                                      const std::vector<Fraction> &durations) {
    if (proportions.front() < proportions.back()) {
        for (auto it1 = durations.begin(); it1 != durations.end(); ++it1) {
            for (auto it2 = it1; it2 != durations.end(); ++it2) {
                if (*it1 + *it2 == target) {
                    return {*it1, *it2};
                }
            }
        }
    } else {
        for (auto it1 = durations.rbegin(); it1 != durations.rend(); ++it1) {
            for (auto it2 = it1; it2 != durations.rend(); ++it2) {
                if (*it1 + *it2 == target) {
                    return {*it1, *it2};
                }
            }
        }
    }

    return {};
}

static std::vector<Fraction> brute_force_split_in_three(const Fraction &target, const std::vector<Fraction> &durations) {
    for (auto it1 = durations.begin(); it1 != durations.end(); ++it1) {
        for (auto it2 = it1; it2 != durations.end(); ++it2) {
            for (auto it3 = it2; it3 != durations.end(); ++it3) {
                if (*it1 + *it2 + *it3 == target) {
                    return {*it1, *it2, *it3};
                }
            }
        }
    }

    return {};
}

static std::vector<Fraction> generate_durations(int input_max_denominator) {
    std::vector<Fraction> durations;
    for (int denom = 1; denom <= std::min(input_max_denominator, ::mu::iex::guitarpro::max_denominator_constant); denom *= 2) {
        durations.emplace_back(1, denom);
        durations.emplace_back(::mu::iex::guitarpro::one_dot_duration_multiplier * denom);
        durations.emplace_back(::mu::iex::guitarpro::two_dots_duration_multiplier * denom);
    }
    return durations;
}

static Fraction find_closest_displayable_duration(const Fraction &total_duration, const Fraction &target_duration,
                                                  int requested_denominator, bool skip_first_best) {
    Fraction closest_fraction = Fraction(0, 1);
    double min_difference = std::numeric_limits<double>::max();
    int numerator = target_duration.numerator();
    int denominator = target_duration.denominator();
    const Fraction smallest_fraction(1, requested_denominator);

    bool first_best_skipped = false;

    for (int candidate_numerator = 1; candidate_numerator < requested_denominator; candidate_numerator++) {
        double original_fraction = static_cast<double>(numerator) / denominator;
        double candidate_fraction = static_cast<double>(candidate_numerator) / requested_denominator;

        double difference = std::fabs(original_fraction - candidate_fraction);

        if (difference < min_difference) {
            Fraction result_candidate_fraction(candidate_numerator, requested_denominator);
            result_candidate_fraction = std::max(result_candidate_fraction, smallest_fraction);
            result_candidate_fraction = std::min(result_candidate_fraction, total_duration - smallest_fraction);

            if (can_be_represented_as_dotted_note(result_candidate_fraction, requested_denominator)) {
                if (skip_first_best && !first_best_skipped) {
                    first_best_skipped = true;
                } else {
                    min_difference = difference;
                    closest_fraction = result_candidate_fraction;
                }
            }
        }
    }

    return closest_fraction.reduced();
}

static bool can_be_represented_as_dotted_note(const Fraction &duration, int input_max_denominator) {
    int denom = 1;
    while (denom <= std::min(input_max_denominator, ::mu::iex::guitarpro::max_denominator_constant)) {
        Fraction base(1, denom);
        if (duration == base || duration == base * ::mu::iex::guitarpro::one_dot_duration_multiplier ||
            duration == base * ::mu::iex::guitarpro::two_dots_duration_multiplier) {
            return true;
        }

        denom *= 2;
    }

    return false;
}
} // namespace mu::iex::guitarpro
