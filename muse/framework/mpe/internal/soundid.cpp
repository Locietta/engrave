// SPDX-License-Identifier: GPL-3.0-only
#include "soundid.h"

#include <algorithm>

#include <ankerl/unordered_dense.h>

namespace muse::mpe {
namespace {

const ankerl::unordered_dense::map<SoundId, String> &id_strings() {
    static const ankerl::unordered_dense::map<SoundId, String> map{{SoundId::UNDEFINED, String(u"undefined")},
                                                                   {SoundId::UNKNOWN, String(u"unknown")},
                                                                   {SoundId::ACCORDION, String(u"accordion")},
                                                                   {SoundId::BANDONEON, String(u"bandoneon")},
                                                                   {SoundId::CONCERTINA, String(u"concertina")},
                                                                   {SoundId::HARMONICA, String(u"harmonica")},
                                                                   {SoundId::MELODICA, String(u"melodica")},
                                                                   {SoundId::SHENG, String(u"sheng")},
                                                                   {SoundId::CELESTA, String(u"celesta")},
                                                                   {SoundId::CLAVICHORD, String(u"clavichord")},
                                                                   {SoundId::HARPSICHORD, String(u"harpsichord")},
                                                                   {SoundId::VIRGINAL, String(u"virginal")},
                                                                   {SoundId::PIANO, String(u"piano")},
                                                                   {SoundId::ORGAN, String(u"organ")},
                                                                   {SoundId::HARMONIUM, String(u"harmonium")},
                                                                   {SoundId::SYNTHESIZER, String(u"synthesizer")},
                                                                   {SoundId::THEREMIN, String(u"theremin")},
                                                                   {SoundId::ONDES_MARTENOT, String(u"ondes_martenot")},

                                                                   {SoundId::HARP, String(u"harp")},
                                                                   {SoundId::CAVAQUINHO, String(u"cavaquinho")},
                                                                   {SoundId::GUITAR, String(u"guitar")},
                                                                   {SoundId::BASS_GUITAR, String(u"bass_guitar")},
                                                                   {SoundId::BANJO, String(u"banjo")},
                                                                   {SoundId::UKULELE, String(u"ukulele")},
                                                                   {SoundId::MANDOLIN, String(u"mandolin")},
                                                                   {SoundId::MTN_DULCIMER, String(u"mtn_dulcimer")},
                                                                   {SoundId::LUTE, String(u"lute")},
                                                                   {SoundId::THEORBO, String(u"theorbo")},
                                                                   {SoundId::ARCHLUTE, String(u"archlute")},
                                                                   {SoundId::BALALAIKA, String(u"balalaika")},
                                                                   {SoundId::GUZHENG, String(u"guzheng")},
                                                                   {SoundId::GUQIN, String(u"guqin")},
                                                                   {SoundId::PIPA, String(u"pipa")},
                                                                   {SoundId::YANGQIN, String(u"yangqin")},
                                                                   {SoundId::KOTO, String(u"koto")},
                                                                   {SoundId::OUD, String(u"oud")},
                                                                   {SoundId::SHAMISEN, String(u"shamisen")},
                                                                   {SoundId::SITAR, String(u"sitar")},
                                                                   {SoundId::PRIM, String(u"prim")},
                                                                   {SoundId::BRAC, String(u"brac")},
                                                                   {SoundId::BUGARIJA, String(u"bugarija")},
                                                                   {SoundId::BERDA, String(u"berda")},
                                                                   {SoundId::CELO, String(u"celo")},
                                                                   {SoundId::BANDURRIA, String(u"bandurria")},
                                                                   {SoundId::LAUD, String(u"laud")},
                                                                   {SoundId::STRINGS_GROUP, String(u"strings_group")},
                                                                   {SoundId::VIOLIN, String(u"violin")},
                                                                   {SoundId::VIOLA, String(u"viola")},
                                                                   {SoundId::VIOLA_DA_GAMBA, String(u"viola_dagamba")},
                                                                   {SoundId::VIOLONCELLO, String(u"violoncello")},
                                                                   {SoundId::VIOL, String(u"viol")},
                                                                   {SoundId::PARDESSUS_VIOL, String(u"pardessus_viol")},
                                                                   {SoundId::BARYTON, String(u"baryton")},
                                                                   {SoundId::VIOLONE, String(u"violone")},
                                                                   {SoundId::NYCKELHARPA, String(u"nyckelharpa")},
                                                                   {SoundId::ERHU, String(u"erhu")},
                                                                   {SoundId::AJAENG, String(u"ajaeng")},
                                                                   {SoundId::GAYAGEUM, String(u"gayageum")},
                                                                   {SoundId::CONTRABASS, String(u"contrabass")},
                                                                   {SoundId::OCTOBASS, String(u"octobass")},
                                                                   {SoundId::TIMPLE_CANARIO, String(u"timple_canario")},

                                                                   {SoundId::WINDS_GROUP, String(u"winds_group")},
                                                                   {SoundId::PICCOLO, String(u"piccolo")},
                                                                   {SoundId::DULZAINA, String(u"dulzaina")},
                                                                   {SoundId::HECKELPHONE, String(u"heckelphone")},
                                                                   {SoundId::HECKELPHONE_CLARINET, String(u"heckelphone_clarinet")},
                                                                   {SoundId::OBOE, String(u"oboe")},
                                                                   {SoundId::LUPOPHONE, String(u"lupophone")},
                                                                   {SoundId::FLUTE, String(u"flute")},
                                                                   {SoundId::PAN_FLUTE, String(u"pan_flute")},
                                                                   {SoundId::DANSO, String(u"danso")},
                                                                   {SoundId::DAEGEUM, String(u"daegeum")},
                                                                   {SoundId::TRAVERSO, String(u"traverso")},
                                                                   {SoundId::DIZI, String(u"dizi")},
                                                                   {SoundId::SHAKUHACHI, String(u"shakuhachi")},
                                                                   {SoundId::HICHIRIKI, String(u"hichiriki")},
                                                                   {SoundId::SHO, String(u"sho")},
                                                                   {SoundId::FIFE, String(u"fife")},
                                                                   {SoundId::WHISTLE, String(u"whistle")},
                                                                   {SoundId::FLAGEOLET, String(u"flageolet")},
                                                                   {SoundId::RECORDER, String(u"recorder")},
                                                                   {SoundId::OCARINA, String(u"ocarina")},
                                                                   {SoundId::GEMSHORN, String(u"gemshorn")},
                                                                   {SoundId::QUENA, String(u"quena")},
                                                                   {SoundId::HORN, String(u"horn")},
                                                                   {SoundId::BASSETHORN, String(u"bassethorn")},
                                                                   {SoundId::SHAWM, String(u"shawm")},
                                                                   {SoundId::CROMORNE, String(u"cromorne")},
                                                                   {SoundId::CRUMHORN, String(u"crumhorn")},
                                                                   {SoundId::CORNAMUSE, String(u"cornamuse")},
                                                                   {SoundId::KELHORN, String(u"kelhorn")},
                                                                   {SoundId::RAUSCHPFEIFE, String(u"rauschpfeife")},
                                                                   {SoundId::DUDUK, String(u"duduk")},
                                                                   {SoundId::SHENAI, String(u"shenai")},
                                                                   {SoundId::CLARINET, String(u"clarinet")},
                                                                   {SoundId::CHALUMEAU, String(u"chalumeau")},
                                                                   {SoundId::XAPHOON, String(u"xaphoon")},
                                                                   {SoundId::TAROGATO, String(u"tarogato")},
                                                                   {SoundId::OCTAVIN, String(u"octavin")},
                                                                   {SoundId::SAXOPHONE, String(u"saxophone")},
                                                                   {SoundId::AULOCHROME, String(u"aulochrome")},
                                                                   {SoundId::BASSOON, String(u"bassoon")},
                                                                   {SoundId::CONTRABASSOON, String(u"contrabassoon")},
                                                                   {SoundId::DULCIAN, String(u"dulcian")},
                                                                   {SoundId::RACKETT, String(u"rackett")},
                                                                   {SoundId::SARRUSOPHONE, String(u"sarrusophone")},
                                                                   {SoundId::BAGPIPE, String(u"bagpipe")},
                                                                   {SoundId::TUBA, String(u"tuba")},
                                                                   {SoundId::CORNET, String(u"cornet")},
                                                                   {SoundId::POSTHORN, String(u"posthorn")},
                                                                   {SoundId::BRASS_GROUP, String(u"brass_group")},
                                                                   {SoundId::TRUMPET, String(u"trumpet")},
                                                                   {SoundId::BUGLE, String(u"bugle")},
                                                                   {SoundId::MELLOPHONE_BUGLE, String(u"mellophone_bugle")},
                                                                   {SoundId::EUPHONIUM_BUGLE, String(u"euphonium_bugle")},
                                                                   {SoundId::EUPHONIUM, String(u"euphonium")},
                                                                   {SoundId::FISCORN, String(u"fiscorn")},
                                                                   {SoundId::FLUGELHORN, String(u"flugelhorn")},
                                                                   {SoundId::KUHLOHORN, String(u"kuhlohorn")},
                                                                   {SoundId::OPHICLEIDE, String(u"ophicleide")},
                                                                   {SoundId::CORNETTINO, String(u"cornettino")},
                                                                   {SoundId::CORNETT, String(u"cornett")},
                                                                   {SoundId::SERPENT, String(u"serpent")},
                                                                   {SoundId::TROMBONE, String(u"trombone")},
                                                                   {SoundId::CIMBASSO, String(u"cimbasso")},
                                                                   {SoundId::SACKBUT, String(u"sackbut")},
                                                                   {SoundId::SOUSAPHONE, String(u"sousaphone")},
                                                                   {SoundId::HELICON, String(u"helicon")},
                                                                   {SoundId::CONCH, String(u"conch")},
                                                                   {SoundId::SAXHORN, String(u"saxhorn")},
                                                                   {SoundId::HORAGAI, String(u"horagai")},
                                                                   {SoundId::ALPHORN, String(u"alphorn")},
                                                                   {SoundId::RAG_DUNG, String(u"rag_dung")},
                                                                   {SoundId::DIDGERIDOO, String(u"didgeridoo")},
                                                                   {SoundId::SHOFAR, String(u"shofar")},
                                                                   {SoundId::VUVUZELA, String(u"vuvuzela")},

                                                                   {SoundId::TIMPANI, String(u"timpani")},
                                                                   {SoundId::ROTO_TOMS, String(u"roto_toms")},
                                                                   {SoundId::TUBAPHONE, String(u"tubaphone")},
                                                                   {SoundId::STEEL_DRUMS, String(u"steel_drums")},
                                                                   {SoundId::GLOCKENSPIEL, String(u"glockenspiel")},
                                                                   {SoundId::XYLOPHONE, String(u"xylophone")},
                                                                   {SoundId::XYLOMARIMBA, String(u"xylomarimba")},
                                                                   {SoundId::VIBRAPHONE, String(u"vibraphone")},
                                                                   {SoundId::DULCIMER, String(u"dulcimer")},
                                                                   {SoundId::CIMBALOM, String(u"cimbalom")},
                                                                   {SoundId::MARIMBA, String(u"marimba")},
                                                                   {SoundId::CROTALES, String(u"crotales")},
                                                                   {SoundId::CHIMES, String(u"chimes")},
                                                                   {SoundId::CARILLON, String(u"carillon")},
                                                                   {SoundId::GONG, String(u"gong")},
                                                                   {SoundId::METALLOPHONE, String(u"metallophone")},
                                                                   {SoundId::FLEXATONE, String(u"flexatone")},
                                                                   {SoundId::MUSICAL_SAW, String(u"musical_saw")},
                                                                   {SoundId::MUSICAL_GLASSES, String(u"musical_glasses")},
                                                                   {SoundId::KLAXON_HORNS, String(u"klaxon_horns")},
                                                                   {SoundId::KALIMBA, String(u"kalimba")},
                                                                   {SoundId::BONGOS, String(u"bongos")},
                                                                   {SoundId::XIAOGU, String(u"xiaogu")},
                                                                   {SoundId::BANGU, String(u"bangu")},
                                                                   {SoundId::DAGU, String(u"dagu")},
                                                                   {SoundId::DALUO, String(u"daluo")},
                                                                   {SoundId::XIAOLUO, String(u"xiaoluo")},
                                                                   {SoundId::BAN, String(u"ban")},
                                                                   {SoundId::DABO, String(u"dabo")},
                                                                   {SoundId::NAOBO, String(u"naobo")},
                                                                   {SoundId::BANGZI, String(u"bangzi")},
                                                                   {SoundId::JANGGU, String(u"janggu")},
                                                                   {SoundId::BUK, String(u"buk")},
                                                                   {SoundId::SAMUL_BUK, String(u"samul_buk")},
                                                                   {SoundId::SAMUL_JANGGU, String(u"samul_janggu")},
                                                                   {SoundId::SANJO_JANGGU, String(u"sanjo_janggu")},
                                                                   {SoundId::SORI_BUK, String(u"sori_buk")},
                                                                   {SoundId::SOGO, String(u"sogo")},
                                                                   {SoundId::KKWAENGGWARI, String(u"kkwaenggwari")},
                                                                   {SoundId::JING, String(u"jing")},
                                                                   {SoundId::TOM_TOMS, String(u"tom_toms")},
                                                                   {SoundId::KO_TSUZUMI, String(u"ko_tsuzumi")},
                                                                   {SoundId::O_TSUZUMI, String(u"o_tsuzumi")},
                                                                   {SoundId::KAKKO, String(u"kakko")},
                                                                   {SoundId::SHIME_DAIKO, String(u"shime_daiko")},
                                                                   {SoundId::MIYA_DAIKO, String(u"miya_daiko")},
                                                                   {SoundId::TSURI_DAIKO, String(u"tsuri_daiko")},
                                                                   {SoundId::OKEDO_DAIKO, String(u"okedo_daiko")},
                                                                   {SoundId::KANE, String(u"kane")},
                                                                   {SoundId::SHOKO, String(u"shoko")},
                                                                   {SoundId::CONGA, String(u"conga")},
                                                                   {SoundId::DJEMBE, String(u"djembe")},
                                                                   {SoundId::DOUMBEK, String(u"doumbek")},
                                                                   {SoundId::CAJON, String(u"cajon")},
                                                                   {SoundId::CUICA, String(u"cuica")},
                                                                   {SoundId::DRUMSET, String(u"drumset")},
                                                                   {SoundId::DRUM, String(u"drum")},
                                                                   {SoundId::TABLAS, String(u"tablas")},
                                                                   {SoundId::TIMBALES, String(u"timbales")},
                                                                   {SoundId::ANVIL, String(u"anvil")},
                                                                   {SoundId::BELL_TREE, String(u"bell_tree")},
                                                                   {SoundId::BELL, String(u"bell")},
                                                                   {SoundId::CHAIN, String(u"chain")},
                                                                   {SoundId::CYMBAL, String(u"cymbal")},
                                                                   {SoundId::HI_HAT, String(u"hi_hat")},
                                                                   {SoundId::PIPE, String(u"pipe")},
                                                                   {SoundId::MARK_TREE, String(u"mark_tree")},
                                                                   {SoundId::CASTANET, String(u"castanet")},
                                                                   {SoundId::TAM_TAM, String(u"tam_tam")},
                                                                   {SoundId::THUNDERSHEET, String(u"thunder_sheet")},
                                                                   {SoundId::TRIANGLE, String(u"triangle")},
                                                                   {SoundId::CLAVES, String(u"claves")},
                                                                   {SoundId::GUIRO, String(u"guiro")},
                                                                   {SoundId::BLOCK, String(u"block")},
                                                                   {SoundId::CABASA, String(u"cabasa")},
                                                                   {SoundId::MARACA, String(u"maraca")},
                                                                   {SoundId::QUIJADA, String(u"quijada")},
                                                                   {SoundId::RATCHET, String(u"ratchet")},
                                                                   {SoundId::SHAKER, String(u"shaker")},
                                                                   {SoundId::SHEKERE, String(u"shekere")},
                                                                   {SoundId::STONES, String(u"stones")},
                                                                   {SoundId::TAMBOURINE, String(u"tambourine")},
                                                                   {SoundId::TUBO, String(u"tubo")},
                                                                   {SoundId::VIBRASLAP, String(u"vibraslap")},
                                                                   {SoundId::WHIP, String(u"whip")},
                                                                   {SoundId::CANNON, String(u"cannon")},
                                                                   {SoundId::BIRD_CALL, String(u"bird_call")},
                                                                   {SoundId::SNAP, String(u"snap")},
                                                                   {SoundId::CLAP, String(u"clap")},
                                                                   {SoundId::SLAP, String(u"slap")},
                                                                   {SoundId::STAMP, String(u"stamp")},
                                                                   {SoundId::CHOIR, String(u"choir")},
                                                                   {SoundId::KAZOO, String(u"kazoo")},
                                                                   {SoundId::TAIKO, String(u"taiko")},
                                                                   {SoundId::METRONOME, String(u"metronome")},
                                                                   {SoundId::LAST, String(u"last")}};
    return map;
}

const ankerl::unordered_dense::map<SoundCategory, String> &category_strings() {
    static const ankerl::unordered_dense::map<SoundCategory, String> map{
        {SoundCategory::KEYBOARDS, String(u"keyboards")}, {SoundCategory::STRINGS, String(u"strings")},
        {SoundCategory::WINDS, String(u"winds")},         {SoundCategory::PERCUSSIONS, String(u"percussions")},
        {SoundCategory::VOICES, String(u"voices")},       {SoundCategory::LAST, String(u"last")}};
    return map;
}

const ankerl::unordered_dense::map<SoundSubCategory, String> &subcategory_strings() {
    static const ankerl::unordered_dense::map<SoundSubCategory, String> map{
        {SoundSubCategory::UNDEFINED, String(u"undefined")},
        {SoundSubCategory::UNKNOWN, String(u"unknown")},
        {SoundSubCategory::ENGLISH, String(u"english")},
        {SoundSubCategory::ARMENIAN, String(u"armenian")},
        {SoundSubCategory::ALPINE, String(u"alpine")},
        {SoundSubCategory::AUSTRALIAN, String(u"australian")},
        {SoundSubCategory::IRISH, String(u"irish")},
        {SoundSubCategory::FRENCH, String(u"french")},
        {SoundSubCategory::CHINESE, String(u"chinese")},
        {SoundSubCategory::VIENNA, String(u"vienna")},
        {SoundSubCategory::GREEK, String(u"greek")},
        {SoundSubCategory::JAPANESE, String(u"japanese")},
        {SoundSubCategory::KOREAN, String(u"korean")},
        {SoundSubCategory::TIBETAN, String(u"tibetan")},
        {SoundSubCategory::AFRICAN, String(u"african")},
        {SoundSubCategory::INDIAN, String(u"indian")},
        {SoundSubCategory::SPANISH, String(u"spanish")},
        {SoundSubCategory::CASTILIAN, String(u"castilian")},
        {SoundSubCategory::SWEDISH, String(u"swedish")},
        {SoundSubCategory::HUNGARIAN, String(u"hungarian")},
        {SoundSubCategory::ROMANIAN, String(u"romanian")},
        {SoundSubCategory::CENTRAL_EUROPEAN, String(u"central_european")},

        {SoundSubCategory::BAROQUE, String(u"baroque")},
        {SoundSubCategory::CLASSICAL, String(u"classical")},
        {SoundSubCategory::MODERN, String(u"modern")},
        {SoundSubCategory::ORCHESTRAL, String(u"orchestral")},

        {SoundSubCategory::HAMMOND, String(u"hammond")},
        {SoundSubCategory::WAGNER, String(u"wagner")},
        {SoundSubCategory::ORFF, String(u"orff")},
        {SoundSubCategory::HUANG, String(u"huang")},
        {SoundSubCategory::HOHNER, String(u"hohner")},

        {SoundSubCategory::PERCUSSIVE, String(u"percussive")},
        {SoundSubCategory::PIPED, String(u"piped")},
        {SoundSubCategory::ROTARY, String(u"rotary")},
        {SoundSubCategory::REED, String(u"reed")},
        {SoundSubCategory::FOOT, String(u"foot")},
        {SoundSubCategory::HAND, String(u"hand")},
        {SoundSubCategory::FINGER, String(u"finger")},
        {SoundSubCategory::BOY, String(u"boy")},
        {SoundSubCategory::GIRL, String(u"girl")},
        {SoundSubCategory::MALE, String(u"male")},
        {SoundSubCategory::FEMALE, String(u"female")},
        {SoundSubCategory::PAD, String(u"pad")},
        {SoundSubCategory::PLUCKED, String(u"plucked")},

        {SoundSubCategory::FOUR_PIECE, String(u"4_piece")},
        {SoundSubCategory::FIVE_PIECE, String(u"5_piece")},

        {SoundSubCategory::TEMPLE, String(u"temple")},
        {SoundSubCategory::MILITARY, String(u"military")},
        {SoundSubCategory::RIDE, String(u"ride")},
        {SoundSubCategory::SLEIGH, String(u"sleigh")},
        {SoundSubCategory::COW, String(u"cow")},
        {SoundSubCategory::AGOGO, String(u"agogo")},
        {SoundSubCategory::MARCHING, String(u"marching")},
        {SoundSubCategory::SHOW_STYLE, String(u"show_style")},
        {SoundSubCategory::OPERA, String(u"opera")},
        {SoundSubCategory::CHINA, String(u"china")},

        {SoundSubCategory::SPLASH, String(u"splash")},
        {SoundSubCategory::CRASH, String(u"crash")},
        {SoundSubCategory::PLATE, String(u"plate")},
        {SoundSubCategory::BOWL, String(u"bowl")},
        {SoundSubCategory::FRAME, String(u"frame")},
        {SoundSubCategory::SLIT, String(u"slit")},
        {SoundSubCategory::LOG, String(u"log")},
        {SoundSubCategory::OCEAN, String(u"ocean")},
        {SoundSubCategory::FIELD, String(u"field")},
        {SoundSubCategory::SNARE, String(u"snare")},
        {SoundSubCategory::BRAKE, String(u"brake")},
        {SoundSubCategory::SLIDE, String(u"slide")},
        {SoundSubCategory::POCKET, String(u"pocket")},
        {SoundSubCategory::GARKLEIN, String(u"garklein")},
        {SoundSubCategory::TOY, String(u"toy")},
        {SoundSubCategory::STRING, String(u"string")},
        {SoundSubCategory::TWELVE_STRING, String(u"twelve_string")},

        {SoundSubCategory::GRAND, String(u"grand")},
        {SoundSubCategory::HONKY_TONK, String(u"honky_tonk")},
        {SoundSubCategory::UPRIGHT, String(u"upright")},
        {SoundSubCategory::PRIMA, String(u"prima")},
        {SoundSubCategory::SECUNDA, String(u"secunda")},

        {SoundSubCategory::ELECTRIC, String(u"electric")},
        {SoundSubCategory::ELECTRONIC, String(u"electronic")},
        {SoundSubCategory::ACOUSTIC, String(u"acoustic")},
        {SoundSubCategory::FRETLESS, String(u"fretless")},
        {SoundSubCategory::PEDAL, String(u"pedal")},
        {SoundSubCategory::STEEL, String(u"steel")},
        {SoundSubCategory::METAL, String(u"metal")},
        {SoundSubCategory::IRON, String(u"iron")},
        {SoundSubCategory::BRASS, String(u"brass")},
        {SoundSubCategory::TIN, String(u"tin")},
        {SoundSubCategory::NYLON, String(u"nylon")},
        {SoundSubCategory::WOODEN, String(u"wooden")},
        {SoundSubCategory::SANDPAPER, String(u"sandpaper")},
        {SoundSubCategory::GLASS, String(u"glass")},
        {SoundSubCategory::SHELL, String(u"shell")},
        {SoundSubCategory::WIND, String(u"wind")},
        {SoundSubCategory::SOFT, String(u"soft")},

        {SoundSubCategory::TREBLE, String(u"treble")},
        {SoundSubCategory::DIATONIC, String(u"diatonic")},
        {SoundSubCategory::CHROMATIC, String(u"chromatic")},
        {SoundSubCategory::OCTAVE, String(u"octave")},

        {SoundSubCategory::PICCOLO, String(u"piccolo")},
        {SoundSubCategory::ALTO, String(u"alto")},
        {SoundSubCategory::TENOR, String(u"tenor")},
        {SoundSubCategory::BARITONE, String(u"baritone")},
        {SoundSubCategory::SOPRANO, String(u"soprano")},
        {SoundSubCategory::MEZZO_SOPRANO, String(u"mezzo_soprano")},
        {SoundSubCategory::SOPRANINO, String(u"sopranino")},
        {SoundSubCategory::SOPRANISSIMO, String(u"sopranissimo")},
        {SoundSubCategory::COUNTER_TENOR, String(u"counter_tenor")},
        {SoundSubCategory::CONTRA, String(u"contra")},
        {SoundSubCategory::CONTRA_ALTO, String(u"contra_alto")},
        {SoundSubCategory::SUB_CONTRA_ALTO, String(u"sub_contra_alto")},
        {SoundSubCategory::CONTRA_BASS, String(u"contra_bass")},
        {SoundSubCategory::SUB_CONTRA_BASS, String(u"sub_contra_bass")},
        {SoundSubCategory::DOUBLE_CONTRA_BASS, String(u"double_contra_bass")},
        {SoundSubCategory::BASS, String(u"bass")},
        {SoundSubCategory::BOWED, String(u"bowed")},
        {SoundSubCategory::GREAT_BASS, String(u"great_bass")},
        {SoundSubCategory::HYPER_BASS, String(u"hyper_bass")},
        {SoundSubCategory::MELODY, String(u"melody")},

        {SoundSubCategory::FX_GOBLINS, String(u"fx_goblins")},
        {SoundSubCategory::FX_ATMOSPHERE, String(u"fx_atmosphere")},
        {SoundSubCategory::FX_BRIGHTNESS, String(u"fx_brightness")},
        {SoundSubCategory::FX_CRYSTAL, String(u"fx_crystal")},
        {SoundSubCategory::FX_ECHOES, String(u"fx_echoes")},
        {SoundSubCategory::FX_RAIN, String(u"fx_rain")},
        {SoundSubCategory::FX_SCI_FI, String(u"fx_scifi")},
        {SoundSubCategory::FX_SOUND_TRACK, String(u"fx_soundtrack")},

        {SoundSubCategory::SWEEP, String(u"sweep")},
        {SoundSubCategory::WARM, String(u"warm")},
        {SoundSubCategory::NEW_AGE, String(u"new_age")},
        {SoundSubCategory::POLYSYNTH, String(u"polysynth")},
        {SoundSubCategory::HALO, String(u"halo")},
        {SoundSubCategory::METALLIC, String(u"metallic")},
        {SoundSubCategory::CHOIR, String(u"choir")},

        {SoundSubCategory::SINE_WAVE, String(u"sine_wave")},
        {SoundSubCategory::SQUARE_WAVE, String(u"square_wave")},
        {SoundSubCategory::SAWTOOTH_WAVE, String(u"sawtooth_wave")},

        {SoundSubCategory::PRIMARY, String(u"primary")},
        {SoundSubCategory::SECONDARY, String(u"secondary")},
        {SoundSubCategory::SECTION, String(u"section")},

        {SoundSubCategory::IN_C, String(u"in_c")},
        {SoundSubCategory::IN_D, String(u"in_d")},
        {SoundSubCategory::IN_D_FLAT, String(u"in_d_flat")},
        {SoundSubCategory::IN_E, String(u"in_e")},
        {SoundSubCategory::IN_E_FLAT, String(u"in_e_flat")},
        {SoundSubCategory::IN_F, String(u"in_f")},
        {SoundSubCategory::IN_G, String(u"in_g")},
        {SoundSubCategory::IN_G_FLAT, String(u"in_g_flat")},
        {SoundSubCategory::IN_A, String(u"in_a")},
        {SoundSubCategory::IN_A_FLAT, String(u"in_a_flat")},
        {SoundSubCategory::IN_B, String(u"in_b")},
        {SoundSubCategory::IN_B_FLAT, String(u"in_b_flat")},

        {SoundSubCategory::LAST, String(u"last")}};
    return map;
}

} // namespace

const String &sound_id_to_string(const SoundId id) {
    const auto &map = id_strings();
    const auto search = map.find(id);
    if (search == map.cend()) {
        return map.at(SoundId::UNKNOWN);
    }
    return search->second;
}

SoundId sound_id_from_string(const String &str) {
    if (str.empty()) {
        return SoundId::UNDEFINED;
    }
    const auto &map = id_strings();
    const auto search = std::find_if(map.cbegin(), map.cend(), [&str](const auto &pair) { return pair.second == str; });
    if (search == map.cend()) {
        return SoundId::UNKNOWN;
    }
    return search->first;
}

const String &sound_category_to_string(const SoundCategory category) {
    const auto &map = category_strings();
    const auto search = map.find(category);
    if (search == map.cend()) {
        static const String undefined_category_str(u"undefined");
        return undefined_category_str;
    }
    return search->second;
}

SoundCategory sound_category_from_string(const String &str) {
    const auto &map = category_strings();
    const auto search = std::find_if(map.cbegin(), map.cend(), [&str](const auto &pair) { return pair.second == str; });
    if (search == map.cend()) {
        return SoundCategory::UNDEFINED;
    }
    return search->first;
}

const String &sound_sub_category_to_string(const SoundSubCategory &sub_category) {
    const auto &map = subcategory_strings();
    const auto search = map.find(sub_category);
    if (search == map.cend()) {
        return map.at(SoundSubCategory::UNKNOWN);
    }
    return search->second;
}

SoundSubCategory sound_sub_category_from_string(const String &str) {
    if (str.empty()) {
        return SoundSubCategory::UNDEFINED;
    }
    const auto &map = subcategory_strings();
    const auto search = std::find_if(map.cbegin(), map.cend(), [&str](const auto &pair) { return pair.second == str; });
    if (search == map.cend()) {
        return SoundSubCategory::UNKNOWN;
    }
    return search->first;
}

} // namespace muse::mpe
