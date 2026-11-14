#pragma once
#include <cstdint>
#include <string>

namespace opsu {

/** User profile. Port of itdelatrisu.opsu.user.User. */
class User {
public:
    static constexpr int MAX_BIO_LENGTH = 90;
    /** Banner value meaning "use the custom banner image". */
    static constexpr int BANNER_CUSTOM = -1;

    User(std::string name, int icon);
    User(std::string name, int64_t score, double accuracy, int playsPassed, int playsTotal, int icon);
    User(std::string name, int64_t score, double accuracy, int playsPassed, int playsTotal, int icon,
         int banner, const std::string* bannerFile, const std::string* avatarFile, const std::string* bio);

    /** Adds stats from a passed play. */
    void add(int64_t score, double accuracy);
    /** Adds stats from a failed play. */
    void add(int64_t score);

    const std::string& getName() const { return name; }
    int64_t getScore() const { return score; }
    double getAccuracy() const { return accuracy; }
    int getPassedPlays() const { return playsPassed; }
    int getTotalPlays() const { return playsTotal; }
    int getIconId() const { return icon; }
    int getLevel() const { return level; }
    /** Progress to the next level in [0,1). */
    double getNextLevelProgress() const { return levelProgress; }

    void setName(std::string n) { name = std::move(n); }
    void setIconId(int id) { icon = id; }
    int getBanner() const { return banner; }
    const std::string& getBannerFile() const { return bannerFile; }
    const std::string& getAvatarFile() const { return avatarFile; }
    const std::string& getBio() const { return bio; }
    void setBanner(int preset);
    /** nullptr == Java null. */
    void setBannerFile(const std::string* file);
    void setAvatarFile(const std::string* file);
    /** Truncated to MAX_BIO_LENGTH UTF-16 units. */
    void setBio(const std::string* bio);

    /** Java compareTo: case-insensitive name order. */
    int compareTo(const User& other) const;
    bool operator<(const User& o) const { return compareTo(o) < 0; }

    /** Total score needed for a given level (osu! wiki "Score#Level"). */
    static int64_t getScoreForLevel(int level);

private:
    void calculateLevel();

    std::string name;
    int64_t score;
    double accuracy;
    int playsPassed;
    int playsTotal;
    int level = 1;
    double levelProgress = 0.0;
    int icon;
    int banner = 0;
    std::string bannerFile;
    std::string avatarFile;
    std::string bio;
};

}  // namespace opsu
