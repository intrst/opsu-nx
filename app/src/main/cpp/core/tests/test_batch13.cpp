// Tests for batch 13 (HitSound). Behavioural, no JVM golden (plain enum/string logic).
#include <cstdio>
#include <cstring>
#include "opsu/audio/HitSound.h"

using namespace opsu;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

int main() {
    // enum names/order/file names, checked against the Java source
    {
        const char* names[] = {"hitclap", "hitfinish", "hitnormal", "hitwhistle", "sliderslide", "slidertick", "sliderwhistle"};
        auto v = hitSoundValues();
        CHECK(v.size() == HitSound_SIZE && HitSound_SIZE == 7);
        for (size_t i = 0; i < v.size(); i++) CHECK((size_t)v[i] == i && std::strcmp(getFileName(v[i]), names[i]) == 0);
        CHECK(SampleSet_SIZE == 3);
        CHECK(std::strcmp(getName(SampleSet::NORMAL), "normal") == 0 && getIndex(SampleSet::NORMAL) == 1);
        CHECK(std::strcmp(getName(SampleSet::SOFT), "soft") == 0 && getIndex(SampleSet::SOFT) == 2);
        CHECK(std::strcmp(getName(SampleSet::DRUM), "drum") == 0 && getIndex(SampleSet::DRUM) == 3);
    }
    // initial state
    hitsound::resetForTesting();
    CHECK(hitsound::defaultSampleSet() == SampleSet::NORMAL);
    CHECK(!hitsound::activeSampleSet().has_value());

    // default by name: case-insensitive, unknown (incl. "auto", empty) resets to NORMAL
    {
        hitsound::setDefaultSampleSet("Soft");  CHECK(hitsound::defaultSampleSet() == SampleSet::SOFT);
        hitsound::setDefaultSampleSet("DRUM");  CHECK(hitsound::defaultSampleSet() == SampleSet::DRUM);
        hitsound::setDefaultSampleSet("auto");  CHECK(hitsound::defaultSampleSet() == SampleSet::NORMAL);
        hitsound::setDefaultSampleSet("drum");  CHECK(hitsound::defaultSampleSet() == SampleSet::DRUM);
        hitsound::setDefaultSampleSet("");      CHECK(hitsound::defaultSampleSet() == SampleSet::NORMAL);
        hitsound::setDefaultSampleSet("drum");
        hitsound::setDefaultSampleSet("taiko"); CHECK(hitsound::defaultSampleSet() == SampleSet::NORMAL);   // not a set
        hitsound::setDefaultSampleSet("soft ");  CHECK(hitsound::defaultSampleSet() == SampleSet::NORMAL);  // no trimming
    }
    // default by index: 1..3, everything else (0 auto, 4 taiko, negatives) -> NORMAL
    {
        hitsound::setDefaultSampleSetType(2);   CHECK(hitsound::defaultSampleSet() == SampleSet::SOFT);
        hitsound::setDefaultSampleSetType(3);   CHECK(hitsound::defaultSampleSet() == SampleSet::DRUM);
        hitsound::setDefaultSampleSetType(0);   CHECK(hitsound::defaultSampleSet() == SampleSet::NORMAL);
        hitsound::setDefaultSampleSetType(3);
        hitsound::setDefaultSampleSetType(4);   CHECK(hitsound::defaultSampleSet() == SampleSet::NORMAL);
        hitsound::setDefaultSampleSetType(3);
        hitsound::setDefaultSampleSetType(-127); CHECK(hitsound::defaultSampleSet() == SampleSet::NORMAL);
        hitsound::setDefaultSampleSetType(1);   CHECK(hitsound::defaultSampleSet() == SampleSet::NORMAL);
    }
    // resolve: explicit index wins, 0/invalid falls back to the default, and resolving never changes state
    {
        hitsound::setDefaultSampleSetType(3);   // drum
        CHECK(hitsound::resolveSampleSet(1) == SampleSet::NORMAL);
        CHECK(hitsound::resolveSampleSet(2) == SampleSet::SOFT);
        CHECK(hitsound::resolveSampleSet(3) == SampleSet::DRUM);
        CHECK(hitsound::resolveSampleSet(0) == SampleSet::DRUM);
        CHECK(hitsound::resolveSampleSet(4) == SampleSet::DRUM);
        CHECK(hitsound::resolveSampleSet(-1) == SampleSet::DRUM);
        hitsound::setDefaultSampleSetType(2);   // soft
        CHECK(hitsound::resolveSampleSet(0) == SampleSet::SOFT);
        CHECK(hitsound::resolveSampleSet(1) == SampleSet::NORMAL);
        CHECK(hitsound::defaultSampleSet() == SampleSet::SOFT);
        CHECK(!hitsound::activeSampleSet().has_value());
    }
    // deprecated global setSampleSet: starts from the default, explicit index overrides; default changes don't move it
    {
        hitsound::setDefaultSampleSetType(3);
        hitsound::setSampleSet(0);  CHECK(hitsound::activeSampleSet() == SampleSet::DRUM);
        hitsound::setSampleSet(2);  CHECK(hitsound::activeSampleSet() == SampleSet::SOFT);
        hitsound::setDefaultSampleSetType(1);
        CHECK(hitsound::activeSampleSet() == SampleSet::SOFT);
        hitsound::setSampleSet(9);  CHECK(hitsound::activeSampleSet() == SampleSet::NORMAL);   // invalid -> default (now normal)
        hitsound::setDefaultSampleSetType(2);
        hitsound::setSampleSet(-5); CHECK(hitsound::activeSampleSet() == SampleSet::SOFT);
        hitsound::setSampleSet(3);  CHECK(hitsound::activeSampleSet() == SampleSet::DRUM);
    }
    hitsound::resetForTesting();
    CHECK(hitsound::defaultSampleSet() == SampleSet::NORMAL && !hitsound::activeSampleSet());

    if (failures) { std::printf("batch13: %d FAILED\n", failures); return 1; }
    std::printf("batch13: all tests passed\n");
    return 0;
}
