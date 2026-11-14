#pragma once
#include <array>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "opsu/downloads/Download.h"

namespace opsu {

/**
 * The three DownloadServer methods DownloadNode uses. The (unported) servers implement this.
 * `getDownloadURL` returns nullopt for Java null; empty headers = Java null / no headers.
 */
class DownloadSource {
public:
    virtual ~DownloadSource() = default;
    virtual std::optional<std::string> getDownloadURL(int beatmapSetID) = 0;
    virtual std::map<std::string, std::string> getDownloadRequestHeaders() = 0;
    virtual bool disableSSLInDownloads() = 0;
};

/** Status badge colour (Colors.GREEN / Color.cyan / magenta / orange / gray; the real colour values come with the UI). */
enum class StatusColor { GREEN, CYAN, MAGENTA, ORANGE, GRAY };

/** Kind of the notification DownloadNode asks for (Colors.GREEN = SUCCESS, Color.red = FAILURE). */
enum class NotifyKind { SUCCESS, FAILURE };

/**
 * What the card layout needs from the font backend (Fonts.LARGE / MEDIUM / BOLD / DEFAULT). Like the Java
 * `Font.getLineHeight()` / `getWidth(String)` the metrics are INTS (the layout arithmetic depends on it).
 * Strings are UTF-8. Implemented by the real font code later; tests use a deterministic fake.
 */
class CardFonts {
public:
    enum class Font { LARGE, MEDIUM, BOLD, DEFAULT };
    virtual ~CardFonts() = default;
    virtual int lineHeight(Font f) const = 0;
    virtual int width(Font f, const std::string& utf8) const = 0;
    /** Fonts.loadGlyphs (only called for Unicode metadata); default: nothing to do. */
    virtual void loadGlyphs(Font, const std::string&) const {}
};

/** A rectangle {x, y, w, h}. */
struct CardRect { float x, y, w, h; };
/** An integer clip rectangle (the Java `(int)` casts of the float layout values). */
struct ClipRect { int x, y, w, h; };

/** The parts of a Download that `animate()` reads (Java read them from the Download behind DownloadList.get()). */
struct DownloadSnapshot {
    Download::Status status;
    float progress;
};

/**
 * Node containing song data and a Download object. Port of the DATA half of itdelatrisu.opsu.downloads.DownloadNode (27 KB Java).
 *
 * **WIP (partially ported).** Ported: fields, both constructors, getters / setters, status label / colour, `createDownload`, `toString`,
 * `ease` / `smoothing`, and (batch 18) everything in the card code that needs no rendering: `init` + the list geometry getters, the card
 * geometry helpers, the hit tests (`resultContains`, `resultThumbContains`, `resultDownloadContains`, `resultAreaContains`), `getCardRect`,
 * the clip rectangle of `clipToResultArea`, `fit` / `updateFit` (text shortening over the `CardFonts` seam), the rectangles of
 * `drawDownloadIcon`, and `animate()` with its state getters (the BeatmapSetList lookup is the `loadedInLibrary` argument).
 * NOT ported (needs the backend): `drawResult`, `drawResultScrollbar` (UI.drawScrollbar), the actual `g.setClip` call, the `rect` / `rgba`
 * sprite helpers, and the `ringColors` array (needs Colors.getDifficultyColor).
 *
 * Seams / deviations
 * - Options.useUnicodeMetadata is passed as an argument (`getTitle(useUnicode)`, `getArtist(...)`, `toString(...)`), like Beatmap.
 * - Java null Strings (titleUnicode, artistUnicode, hash, source) are "" (getTitle/getArtist test `!= null && !isEmpty()`, so identical).
 *   `stars` / `difficulties` are `std::optional<std::vector>` (null != empty for `getStars()`; `hasStars()` is false for both).
 * - `createDownload(source, importDir, useUnicode, client, notify)`: Options.getImportDir() and UI.getNotificationManager() are
 *   arguments. The Download listener's messages use the title as it was at creation time (Java re-read the option on every event).
 *   The Fonts.loadGlyphs call is not ported. The node owns its Download (`unique_ptr`); `getDownload()` returns a raw pointer.
 * - Not copyable (owns the Download).
 */
class DownloadNode {
public:
    /** One difficulty of a beatmap set (as reported by the download server). Unknown values are negative / empty. */
    struct Difficulty {
        std::string version;
        double stars = -1, cs = -1, ar = -1, od = -1, hp = -1, bpm = -1;
        int length = -1;  // in seconds
        int circles = -1, sliders = -1, spinners = -1;
    };

    using Notifier = std::function<void(const std::string& message, NotifyKind kind)>;

    /** Number of result cards per row. */
    static constexpr int COLUMNS = 3;

    DownloadNode(int beatmapSetID, std::string date, std::string title, std::string titleUnicode, std::string artist,
                 std::string artistUnicode, std::string creator, std::string hash);
    DownloadNode(int beatmapSetID, std::string date, std::string title, std::string titleUnicode, std::string artist,
                 std::string artistUnicode, std::string creator);
    DownloadNode(const DownloadNode&) = delete;
    DownloadNode& operator=(const DownloadNode&) = delete;

    /** Creates the download object (no-op if there is one already, or the source has no URL for this set). */
    void createDownload(DownloadSource& source, const std::string& importDir, bool useUnicodeMetadata, HttpClient* client,
                        Notifier notify);
    Download* getDownload() { return download.get(); }
    const Download* getDownload() const { return download.get(); }
    /** Clears the associated download object, if any (destroys it: waits for its worker thread). */
    void clearDownload() { download.reset(); }

    int getID() const { return beatmapSetID; }
    const std::string& getDate() const { return date; }
    std::string getTitle(bool useUnicodeMetadata) const;
    std::string getArtist(bool useUnicodeMetadata) const;
    const std::string& getHash() const { return hash; }
    const std::string& getCreator() const { return creator; }

    /** Sets the star ratings of all difficulties in the set (sorted, lowest first); nullopt = unknown. Also builds the label. */
    void setStars(std::optional<std::vector<double>> stars);
    const std::optional<std::vector<double>>& getStars() const { return stars; }
    bool hasStars() const { return stars && !stars->empty(); }
    double getMinStars() const { return hasStars() ? stars->front() : -1; }
    double getMaxStars() const { return hasStars() ? stars->back() : -1; }
    /** Label of the highest rating ("%.2f", US locale, HALF_UP like Java), or "" if none (Java null). */
    const std::string& getStarLabel() const { return starLabel; }

    void setDifficulties(std::optional<std::vector<Difficulty>> d) { difficulties = std::move(d); }
    const std::optional<std::vector<Difficulty>>& getDifficulties() const { return difficulties; }

    void setSetInfo(double bpm, const std::string& source, int favourites, int plays, bool video, bool storyboard);
    double getBpm() const { return setBpm; }
    const std::string& getSource() const { return source; }
    int getFavourites() const { return favourites; }
    int getPlays() const { return plays; }
    bool hasVideo() const { return hasVideo_; }
    bool hasStoryboard() const { return hasStoryboard_; }

    /** Sets the ranked status (osu! API value: 1 ranked, 2 approved, 3 qualified, 4 loved, 0 pending, -1 WIP, -2 graveyard, -99 unknown). */
    void setStatus(int s) { status = s; }
    /** Status badge text, or nullopt (Java null) if unknown. */
    std::optional<std::string> getStatusLabel() const;
    StatusColor getStatusColor() const;

    /** "[id] artist - title (by creator)". */
    std::string toString(bool useUnicodeMetadata) const;

    // ---- card layout (batch 18). These are the Java statics; the layout is global state, set by init().
    /** Card geometry constants. DownloadThumbnails.ASPECT (that class is not ported). */
    static constexpr float THUMB_ASPECT = 0.9f;

    /**
     * Initializes the base coordinates for drawing (the result list spans the full width of the screen).
     * `fonts` is remembered (not owned) and must outlive every later call that measures text (`btnY` reads the DEFAULT line height each time, like Java).
     */
    static void init(int width, int height, const CardFonts& fonts);
    static float getListX() { return buttonBaseX; }
    static float getListWidth() { return buttonWidth; }
    static float getListY() { return buttonBaseY; }
    static float getListHeight() { return listHeight; }
    static float getBottomReserve() { return bottomReserve; }
    static int maxResultsShown() { return maxResultsShown_; }
    static float getButtonOffset() { return buttonOffset; }
    /** Left edge of the card in the given column. */
    static float cardX(int col) { return buttonBaseX + col * (cardWidth + cardGap); }
    static float getCardWidth() { return cardWidth; }
    static float getCardGap() { return cardGap; }
    static float getCardHeight() { return buttonHeight; }
    // Shared by drawing and hit testing (private in Java; public here because the future drawing code and the tests need them).
    static float margin() { return buttonHeight * 0.07f; }
    static float thumbW() { return buttonHeight * THUMB_ASPECT; }
    static float textLeft(int col) { return cardX(col) + thumbW() + cardWidth * 0.025f; }
    static float textRight(int col) { return cardX(col) + cardWidth - margin() * 1.5f; }
    /** Needs the font seam given to init() (Java: NPE before Fonts is set up; here std::logic_error). */
    static float btnH();
    static float btnY(int row) { return buttonBaseY + row * buttonOffset + buttonHeight - margin() - btnH(); }

    /** True if the coordinates are within the result card at the given visible row and column. */
    static bool resultContains(float cx, float cy, int row, int col);
    /** True if the coordinates are within the cover thumbnail of the card (tapping it toggles the preview). */
    static bool resultThumbContains(float cx, float cy, int row, int col);
    /** True if the coordinates are within the download button of the card. */
    static bool resultDownloadContains(float cx, float cy, int row, int col);
    /** Bounds {x, y, w, h} of a part of the card in the first visible row. zone: 0 = card, 1 = cover, 2 = download button (anything else = card). */
    static std::array<float, 4> getCardRect(int zone, int col);
    /** True if the coordinates are within the download result area. */
    static bool resultAreaContains(float cx, float cy);
    /** The rectangle `clipToResultArea` passes to `g.setClip` (the `(int)` casts of the layout values). */
    static ClipRect resultAreaClip();

    /** Shortens `str` (adding "...") so that it fits within `maxW`. (Never splits a surrogate pair, Java's substring may.) */
    static std::string fit(const CardFonts& fonts, CardFonts::Font font, const std::string& str, float maxW);
    /** The sprite rectangles `drawDownloadIcon` draws (the `rect()` calls with w > 0 and h > 0, in drawing order; the colour / alpha is the caller's). */
    static std::vector<CardRect> downloadIconRects(float cx, float cy, float bh);

    /** Rebuilds the cached card text if the available width or the displayed metadata changed. */
    void updateFit(const CardFonts& fonts, bool useUnicodeMetadata, float textW);
    const std::string& getFittedTitle() const { return fTitle; }
    const std::string& getFittedArtist() const { return fArtist; }
    const std::string& getFittedMapper() const { return fMapper; }
    /** Width of the star chip (padding + star.png), 0 without a rating. */
    float getChipWidth() const { return chipW; }

    /**
     * Advances this card's animation state (call once per update for every card on screen).
     * @param k smoothing factor for this frame (see smoothing())
     * @param glowTarget 0 idle, around 0.5 hovered, 1 selected
     * @param previewTarget 1 while this card's track preview is loading / playing, else 0
     * @param loadedInLibrary BeatmapSetList.get().containsBeatmapSetID(id) (BeatmapSetList is not ported)
     * @param dl this set's download (DownloadList.get().getDownload(id)), or nullopt
     */
    void animate(float k, float glowTarget, float previewTarget, bool loadedInLibrary, std::optional<DownloadSnapshot> dl);
    /** Same, looking the download up in the global DownloadList like the Java. */
    void animate(float k, float glowTarget, float previewTarget, bool loadedInLibrary);

    // State read by the info panel / drawing (valid after animate()).
    float getGlow() const { return glow; }
    float getPreviewMix() const { return previewMix; }
    bool isLoadedInLibrary() const { return loaded; }
    bool isDownloadActive() const { return dlActive; }
    /** Status seen by the last animate() that found a download; like Java it keeps the stale value afterwards (nullopt = Java null). */
    std::optional<Download::Status> getDownloadStatus() const { return dlStatus; }
    float getShownProgress() const { return shownPct; }
    const std::string& getProgressLabel() const { return pctLabel; }

    /** Moves `v` toward `target` by a fraction `k` of the remaining distance (private `ease` in Java). */
    static float ease(float v, float target, float k);
    /** Smoothing factor for a frame of the given length (about 70 ms to cover most of the distance). */
    static float smoothing(int delta);

private:
    std::unique_ptr<Download> download;
    int beatmapSetID;
    std::string date, title, titleUnicode, hash, artist, artistUnicode, creator;
    std::optional<std::vector<double>> stars;
    std::string starLabel;
    std::optional<std::vector<Difficulty>> difficulties;
    double setBpm = -1;
    std::string source;
    int favourites = -1, plays = -1;
    bool hasVideo_ = false, hasStoryboard_ = false;
    int status = -99;

    // per-card animation state (batch 18)
    float glow = 0.f, previewMix = 0.f, shownPct = 0.f;
    bool loaded = false, dlActive = false;
    std::optional<Download::Status> dlStatus;
    int pctShown = -1;
    std::string pctLabel;
    // cached, shortened card text
    float fitW = -1.f, chipW = 0.f;
    std::optional<std::string> fitSrcTitle, fitSrcArtist;
    std::string fTitle, fArtist, fMapper;

    // layout (static, set by init())
    static float cardWidth, cardGap;
    static float buttonBaseX, buttonBaseY, buttonWidth, buttonHeight, buttonOffset;
    static int maxResultsShown_;
    static float listHeight, bottomReserve;
    static const CardFonts* fonts_;
};

}  // namespace opsu
