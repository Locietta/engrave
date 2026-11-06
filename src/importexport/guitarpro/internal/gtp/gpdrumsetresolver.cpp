#include "gpdrumsetresolver.h"

#include "log.h"

namespace mu::iex::guitarpro {
void GPDrumSetResolver::init_gp_drum() {
    add_drum(GPDrum(35, 0, 0));
    add_drum(GPDrum(36, 0, 0));
    add_drum(GPDrum(36, 6, 1));
    add_drum(GPDrum(37, 1, 2));
    add_drum(GPDrum(38, 1, 0));
    add_drum(GPDrum(42, 10, 0));
    add_drum(GPDrum(43, 5, 0));
    add_drum(GPDrum(44, 11, 0));
    add_drum(GPDrum(45, 6, 0));
    add_drum(GPDrum(46, 10, 2));
    add_drum(GPDrum(47, 7, 0));
    add_drum(GPDrum(48, 8, 0));
    add_drum(GPDrum(49, 13, 0));
    add_drum(GPDrum(50, 9, 0));
    add_drum(GPDrum(51, 15, 0));
    add_drum(GPDrum(52, 16, 0));
    add_drum(GPDrum(53, 15, 2));
    add_drum(GPDrum(55, 14, 0));
    add_drum(GPDrum(56, 3, 0));
    add_drum(GPDrum(57, 12, 0));
    add_drum(GPDrum(91, 1, 1));
    add_drum(GPDrum(92, 10, 1));
    add_drum(GPDrum(93, 15, 1));
    add_drum(GPDrum(99, 2, 0));
    add_drum(GPDrum(102, 4, 0));

    add_drum(GPDrum(0, 0, 0, u"rvs-cymb"));

    add_drum(GPDrum(37, 0, 0, u"snr"));
    add_drum(GPDrum(38, 1, 0, u"snr"));

    add_drum(GPDrum(39, 0, 0, u"hclap"));

    add_drum(GPDrum(87, 2, 1, u"latinoKit"));
    add_drum(GPDrum(54, 3, 0, u"latinoKit"));
    add_drum(GPDrum(54, 3, 1, u"latinoKit"));
    add_drum(GPDrum(112, 3, 3, u"latinoKit"));

    add_drum(GPDrum(58, 0, 0, u"vbrslp"));

    add_drum(GPDrum(67, 0, 0, u"agogoKit"));
    add_drum(GPDrum(68, 1, 0, u"agogoKit"));

    add_drum(GPDrum(63, 4, 0, u"africaKit"));
    add_drum(GPDrum(73, 2, 0, u"africaKit"));
    add_drum(GPDrum(74, 2, 2, u"africaKit"));
    add_drum(GPDrum(109, 3, 2, u"africaKit"));
    add_drum(GPDrum(110, 4, 1, u"africaKit"));

    add_drum(GPDrum(62, 1, 2, u"cngKit"));
    add_drum(GPDrum(63, 1, 0, u"cngKit"));
    add_drum(GPDrum(64, 0, 0, u"cngKit"));

    add_drum(GPDrum(65, 0, 0, u"snthdrm"));

    add_drum(GPDrum(65, 1, 0, u"tmblKit"));
    add_drum(GPDrum(66, 0, 0, u"tmblKit"));

    add_drum(GPDrum(65, 0, 0, u"mldctm"));

    add_drum(GPDrum(66, 0, 0, u"taiko"));

    add_drum(GPDrum(69, 0, 0, u"cbs"));
    add_drum(GPDrum(69, 0, 1, u"cbs"));
    add_drum(GPDrum(117, 0, 2, u"cbs"));

    add_drum(GPDrum(70, 0, 0, u"mrcs"));

    add_drum(GPDrum(71, 0, 0, u"whstlKit"));
    add_drum(GPDrum(72, 1, 0, u"whstlKit"));

    add_drum(GPDrum(73, 0, 0, u"guiro"));
    add_drum(GPDrum(73, 0, 1, u"guiro"));
    add_drum(GPDrum(74, 0, 2, u"guiro"));

    add_drum(GPDrum(75, 0, 0, u"clvs"));
    add_drum(GPDrum(75, 5, 0, u"clvs"));

    add_drum(GPDrum(76, 0, 0, u"wdblckKit"));
    add_drum(GPDrum(77, 1, 0, u"wdblckKit"));

    add_drum(GPDrum(78, 0, 0, u"cuicaKit"));
    add_drum(GPDrum(79, 1, 0, u"cuicaKit"));

    add_drum(GPDrum(80, 0, 1, u"trngl"));
    add_drum(GPDrum(81, 0, 0, u"trngl"));

    add_drum(GPDrum(82, 0, 0, u"shkr"));
    add_drum(GPDrum(82, 0, 1, u"shkr"));

    add_drum(GPDrum(83, 0, 0, u"jngl-bell"));

    add_drum(GPDrum(84, 0, 0, u"bell-tree"));
    add_drum(GPDrum(123, 0, 2, u"bell-tree"));

    add_drum(GPDrum(85, 0, 0, u"cstnt"));

    add_drum(GPDrum(86, 0, 1, u"surdo"));
    add_drum(GPDrum(86, 0, 0, u"surdo"));

    add_drum(GPDrum(102, 0, 0, u"cowbell"));

    add_drum(GPDrum(51, 0, 0, u"sambaKit"));
    add_drum(GPDrum(93, 0, 1, u"sambaKit"));
    add_drum(GPDrum(53, 0, 2, u"sambaKit"));
    add_drum(GPDrum(102, 1, 0, u"sambaKit"));
    add_drum(GPDrum(56, 2, 0, u"sambaKit"));
    add_drum(GPDrum(99, 3, 0, u"sambaKit"));
    add_drum(GPDrum(99, 3, 0, u"sambaKit"));
    add_drum(GPDrum(77, 4, 0, u"sambaKit"));
    add_drum(GPDrum(76, 5, 0, u"sambaKit"));
    add_drum(GPDrum(66, 6, 0, u"sambaKit"));
    add_drum(GPDrum(65, 7, 0, u"sambaKit"));

    add_drum(GPDrum(61, 0, 0, u"bngKit"));
    add_drum(GPDrum(106, 0, 2, u"bngKit"));
    add_drum(GPDrum(107, 0, 1, u"bngKit"));
    add_drum(GPDrum(60, 1, 0, u"bngKit"));
    add_drum(GPDrum(104, 1, 2, u"bngKit"));
    add_drum(GPDrum(105, 1, 1, u"bngKit"));
    add_drum(GPDrum(108, 2, 1, u"bngKit"));
    add_drum(GPDrum(64, 2, 0, u"bngKit"));
    add_drum(GPDrum(109, 2, 2, u"bngKit"));
    add_drum(GPDrum(63, 3, 0, u"bngKit"));
    add_drum(GPDrum(110, 3, 1, u"bngKit"));
    add_drum(GPDrum(62, 3, 2, u"bngKit"));

    add_drum(GPDrum(61, 0, 0, u"conbon"));
    add_drum(GPDrum(106, 0, 2, u"conbon"));
    add_drum(GPDrum(107, 0, 1, u"conbon"));
    add_drum(GPDrum(60, 1, 0, u"conbon"));
    add_drum(GPDrum(104, 1, 2, u"conbon"));
    add_drum(GPDrum(105, 1, 1, u"conbon"));
    add_drum(GPDrum(108, 2, 1, u"conbon"));
    add_drum(GPDrum(64, 2, 0, u"conbon"));
    add_drum(GPDrum(109, 2, 2, u"conbon"));
    add_drum(GPDrum(63, 3, 0, u"conbon"));
    add_drum(GPDrum(110, 3, 1, u"conbon"));
    add_drum(GPDrum(62, 3, 2, u"conbon"));

    add_drum(GPDrum(108, 0, 1, u"cngKit"));
    add_drum(GPDrum(109, 0, 2, u"cngKit"));
    add_drum(GPDrum(110, 1, 1, u"cngKit"));
    add_drum(GPDrum(111, 0, 2, u"tmbrn"));
    add_drum(GPDrum(112, 0, 3, u"tmbrn"));

    add_drum(GPDrum(113, 0, 4, u"tmbrn"));
    add_drum(GPDrum(54, 0, 0, u"tmbrn"));

    add_drum(GPDrum(114, 0, 0, u"ptt"));
    add_drum(GPDrum(115, 0, 0, u"grcss"));
    add_drum(GPDrum(118, 0, 2, u"mrcs"));

    add_drum(GPDrum(119, 0, 0, u"2Mrcs"));
    add_drum(GPDrum(122, 0, 2, u"shkr"));
}

int32_t GPDrumSetResolver::pitch(int32_t element, int32_t variation, const muse::String &name) const {
    if (drum_value.find(name) == drum_value.end()) {
        return 0;
    }

    for (const auto &drum : drum_value.at(name)) {
        if (drum.name_value.contains(name)) {
            if (drum.element == element && drum.variation == variation) {
                return drum.pitch;
            }
        }
    }

    LOGD() << "Absent drum " << "element: " << element << " variation: " << variation << " name: " << name << " key: " << 0;

    return 0;
}

void GPDrumSetResolver::add_drum(const GPDrum &drum) { drum_value[drum.name_value].push_back(drum); }
} // namespace mu::iex::guitarpro
