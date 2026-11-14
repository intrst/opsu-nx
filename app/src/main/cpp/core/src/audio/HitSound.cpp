#include "opsu/audio/HitSound.h"
#include "opsu/util/JavaIO.h"

namespace opsu {

const char* getFileName(HitSound h) {
    switch (h) {
        case HitSound::CLAP: return "hitclap";
        case HitSound::FINISH: return "hitfinish";
        case HitSound::NORMAL: return "hitnormal";
        case HitSound::WHISTLE: return "hitwhistle";
        case HitSound::SLIDERSLIDE: return "sliderslide";
        case HitSound::SLIDERTICK: return "slidertick";
        case HitSound::SLIDERWHISTLE: return "sliderwhistle";
    }
    return "";
}

const char* getName(SampleSet s) {
    switch (s) {
        case SampleSet::NORMAL: return "normal";
        case SampleSet::SOFT: return "soft";
        case SampleSet::DRUM: return "drum";
    }
    return "";
}

int getIndex(SampleSet s) { return (int)s + 1; }

namespace hitsound {

// Plain statics like the Java fields (not thread-safe, as documented there).
static SampleSet g_default = SampleSet::NORMAL;
static std::optional<SampleSet> g_active;

void setDefaultSampleSet(const std::string& sampleSet) {
    g_default = SampleSet::NORMAL;
    for (SampleSet ss : sampleSetValues()) {
        if (java::compareToIgnoreCase(sampleSet, getName(ss)) == 0) {
            g_default = ss;
            return;
        }
    }
}

void setDefaultSampleSetType(int8_t sampleType) {
    g_default = SampleSet::NORMAL;
    for (SampleSet ss : sampleSetValues()) {
        if (sampleType == getIndex(ss)) {
            g_default = ss;
            return;
        }
    }
}

SampleSet defaultSampleSet() { return g_default; }

SampleSet resolveSampleSet(int8_t sampleType) {
    for (SampleSet ss : sampleSetValues())
        if (sampleType == getIndex(ss)) return ss;
    return g_default;
}

void setSampleSet(int8_t sampleType) {
    g_active = g_default;
    for (SampleSet ss : sampleSetValues()) {
        if (sampleType == getIndex(ss)) {
            g_active = ss;
            return;
        }
    }
}

std::optional<SampleSet> activeSampleSet() { return g_active; }

void resetForTesting() {
    g_default = SampleSet::NORMAL;
    g_active.reset();
}

}  // namespace hitsound
}  // namespace opsu
