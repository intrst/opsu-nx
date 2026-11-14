#include "opsu/audio/SoundEffect.h"

namespace opsu {

const char* getFileName(SoundEffect e) {
    switch (e) {
        case SoundEffect::APPLAUSE:       return "applause";
        case SoundEffect::COMBOBREAK:     return "combobreak";
        case SoundEffect::COUNT1:         return "count1s";
        case SoundEffect::COUNT2:         return "count2s";
        case SoundEffect::COUNT3:         return "count3s";
        case SoundEffect::FAIL:           return "failsound";
        case SoundEffect::FOUNTAIN_LOOP:  return "fountain-loop";
        case SoundEffect::FOUNTAIN_SHOOT: return "fountain-shoot";
        case SoundEffect::GO:             return "gos";
        case SoundEffect::MENUBACK:       return "menuback";
        case SoundEffect::MENUCLICK:      return "menuclick";
        case SoundEffect::MENUHIT:        return "menuhit";
        case SoundEffect::READY:          return "readys";
        case SoundEffect::SECTIONFAIL:    return "sectionfail";
        case SoundEffect::SECTIONPASS:    return "sectionpass";
        case SoundEffect::SHUTTER:        return "shutter";
        case SoundEffect::SPINNERBONUS:   return "spinnerbonus";
        case SoundEffect::SPINNEROSU:     return "spinner-osu";
        case SoundEffect::SPINNERSPIN:    return "spinnerspin";
    }
    return "";
}

}  // namespace opsu
