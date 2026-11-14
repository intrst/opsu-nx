#include "opsu/user/User.h"
#include <cmath>
#include <limits>
#include "opsu/util/JavaIO.h"

namespace opsu {

User::User(std::string name, int icon) : User(std::move(name), 0LL, 0.0, 0, 0, icon) {}

User::User(std::string name, int64_t score, double accuracy, int playsPassed, int playsTotal, int icon)
    : name(std::move(name)), score(score), accuracy(accuracy), playsPassed(playsPassed), playsTotal(playsTotal), icon(icon) {
    calculateLevel();
}

User::User(std::string name, int64_t score, double accuracy, int playsPassed, int playsTotal, int icon,
           int banner, const std::string* bannerFile, const std::string* avatarFile, const std::string* bio)
    : User(std::move(name), score, accuracy, playsPassed, playsTotal, icon) {
    this->banner = banner;
    this->bannerFile = bannerFile ? *bannerFile : "";
    this->avatarFile = avatarFile ? *avatarFile : "";
    this->bio = bio ? *bio : "";
}

void User::add(int64_t s, double acc) {
    score += s;
    accuracy = ((accuracy * playsPassed) + acc) / (playsPassed + 1);
    playsPassed++;
    playsTotal++;
    calculateLevel();
}

void User::add(int64_t s) {
    score += s;
    playsTotal++;
    calculateLevel();
}

void User::calculateLevel() {
    if (score == 0) {
        level = 1;
        levelProgress = 0.0;
        return;
    }
    int l;
    for (l = 1; score >= getScoreForLevel(l); l++) {}
    l--;
    level = l;
    int64_t baseScore = getScoreForLevel(l);
    levelProgress = static_cast<double>(score - baseScore) / static_cast<double>(getScoreForLevel(l + 1) - baseScore);
}

int64_t User::getScoreForLevel(int level) {
    if (level <= 1) return 1LL;
    if (level <= 100) {
        double v = 5000.0 / 3 * (4 * std::pow(static_cast<double>(level), 3.0) - 3 * std::pow(static_cast<double>(level), 2.0) - level) +
                   1.25 * std::pow(1.8, level - 60);
        // Java (long) cast: NaN -> 0, saturating
        if (v != v) return 0;
        if (v >= 9223372036854775807.0) return std::numeric_limits<int64_t>::max();
        if (v <= -9223372036854775808.0) return std::numeric_limits<int64_t>::min();
        return static_cast<int64_t>(v);
    }
    return 26931190829LL + 100000000000LL * (level - 100);
}

void User::setBanner(int preset) {
    banner = preset;
    bannerFile = "";
}
void User::setBannerFile(const std::string* file) {
    banner = BANNER_CUSTOM;
    bannerFile = file ? *file : "";
}
void User::setAvatarFile(const std::string* file) { avatarFile = file ? *file : ""; }
void User::setBio(const std::string* b) {
    std::string s = b ? *b : "";
    bio = (java::utf16Length(s) > static_cast<size_t>(MAX_BIO_LENGTH)) ? java::utf16Prefix(s, MAX_BIO_LENGTH) : s;
}

int User::compareTo(const User& other) const { return java::compareToIgnoreCase(name, other.name); }

}  // namespace opsu
