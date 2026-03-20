#include <map>
#include <string>

extern const uint8_t KeyboardLayout_da_DK[];
extern const uint8_t KeyboardLayout_de_DE[];
extern const uint8_t KeyboardLayout_en_UK[];
extern const uint8_t KeyboardLayout_en_US[];
extern const uint8_t KeyboardLayout_es_ES[];
extern const uint8_t KeyboardLayout_fr_FR[];
extern const uint8_t KeyboardLayout_hu_HU[];
extern const uint8_t KeyboardLayout_it_IT[];
extern const uint8_t KeyboardLayout_pt_BR[];
extern const uint8_t KeyboardLayout_pt_PT[];
extern const uint8_t KeyboardLayout_si_SI[];
extern const uint8_t KeyboardLayout_sv_SE[];
extern const uint8_t KeyboardLayout_tr_TR[];

static const std::map<std::string, const uint8_t*>& get_keyboard_layouts() {
    static const std::map<std::string, const uint8_t*> layouts = {
        {"da_DK", KeyboardLayout_da_DK},
        {"de_DE", KeyboardLayout_de_DE},
        {"en_UK", KeyboardLayout_en_UK},
        {"en_US", KeyboardLayout_en_US},
        {"es_ES", KeyboardLayout_es_ES},
        {"fr_FR", KeyboardLayout_fr_FR},
        {"hu_HU", KeyboardLayout_hu_HU},
        {"it_IT", KeyboardLayout_it_IT},
        {"pt_BR", KeyboardLayout_pt_BR},
        {"pt_PT", KeyboardLayout_pt_PT},
        {"si_SI", KeyboardLayout_si_SI},
        {"sv_SE", KeyboardLayout_sv_SE},
        {"tr_TR", KeyboardLayout_tr_TR}
    };
    return layouts;
}
