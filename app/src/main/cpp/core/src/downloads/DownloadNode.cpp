#include "opsu/downloads/DownloadNode.h"
#include <cmath>
#include <filesystem>
#include <stdexcept>
#include "opsu/downloads/DownloadList.h"
#include "opsu/util/JavaCompat.h"
#include "opsu/util/JavaIO.h"
#include "opsu/util/JavaMath.h"

namespace opsu {

DownloadNode::DownloadNode(int id, std::string date_, std::string title_, std::string titleUnicode_, std::string artist_,
                           std::string artistUnicode_, std::string creator_, std::string hash_)
    : beatmapSetID(id), date(std::move(date_)), title(std::move(title_)), titleUnicode(std::move(titleUnicode_)),
      hash(std::move(hash_)), artist(std::move(artist_)), artistUnicode(std::move(artistUnicode_)), creator(std::move(creator_)) {}

DownloadNode::DownloadNode(int id, std::string date_, std::string title_, std::string titleUnicode_, std::string artist_,
                           std::string artistUnicode_, std::string creator_)
    : DownloadNode(id, std::move(date_), std::move(title_), std::move(titleUnicode_), std::move(artist_),
                   std::move(artistUnicode_), std::move(creator_), "") {}

void DownloadNode::createDownload(DownloadSource& src, const std::string& importDir, bool useUnicode, HttpClient* client,
                                  Notifier notify) {
    if (download)
        return;
    std::optional<std::string> url = src.getDownloadURL(beatmapSetID);
    if (!url)
        return;
    std::string path = (std::filesystem::u8path(importDir) / std::to_string(beatmapSetID)).u8string();
    std::string rename = std::to_string(beatmapSetID) + " " + artist + " - " + title + ".osz";  // raw (non-unicode) fields
    auto dl = std::make_unique<Download>(*url, path, rename);
    const std::string shownTitle = getTitle(useUnicode);
    Download::Listener l;
    l.completed = [notify, shownTitle] {
        if (notify) notify("Download complete: " + shownTitle, NotifyKind::SUCCESS);
    };
    l.error = [notify] {
        if (notify) notify("Download failed due to a server error. Please select another beatmap", NotifyKind::FAILURE);
    };
    dl->setListener(std::move(l));
    dl->setRequestHeaders(src.getDownloadRequestHeaders());
    dl->setSSLCertValidation(!src.disableSSLInDownloads());
    dl->setHttpClient(client);
    download = std::move(dl);
}

std::string DownloadNode::getTitle(bool u) const { return (u && !titleUnicode.empty()) ? titleUnicode : title; }
std::string DownloadNode::getArtist(bool u) const { return (u && !artistUnicode.empty()) ? artistUnicode : artist; }

void DownloadNode::setStars(std::optional<std::vector<double>> s) {
    stars = std::move(s);
    if (!stars || stars->empty()) {
        starLabel.clear();
        return;
    }
    starLabel = java::formatFixed(stars->back(), 2);
}

void DownloadNode::setSetInfo(double bpm, const std::string& src, int fav, int pl, bool video, bool storyboard) {
    setBpm = bpm;
    source = src;
    favourites = fav;
    plays = pl;
    hasVideo_ = video;
    hasStoryboard_ = storyboard;
}

std::optional<std::string> DownloadNode::getStatusLabel() const {
    switch (status) {
    case 1: return "Ranked";
    case 2: return "Approved";
    case 3: return "Qualified";
    case 4: return "Loved";
    case 0: return "Pending";
    case -1: return "WIP";
    case -2: return "Graveyard";
    default: return std::nullopt;
    }
}

StatusColor DownloadNode::getStatusColor() const {
    switch (status) {
    case 1: case 2: return StatusColor::GREEN;
    case 3: return StatusColor::CYAN;
    case 4: return StatusColor::MAGENTA;
    case 0: case -1: return StatusColor::ORANGE;
    default: return StatusColor::GRAY;
    }
}

std::string DownloadNode::toString(bool u) const {
    return "[" + std::to_string(beatmapSetID) + "] " + getArtist(u) + " - " + getTitle(u) + " (by " + creator + ")";
}

float DownloadNode::ease(float v, float target, float k) {
    float d = target - v;
    return (d > -0.002f && d < 0.002f) ? target : v + d * k;
}

float DownloadNode::smoothing(int delta) { return 1.f - 1.f / (1.f + static_cast<float>(delta) / 70.f); }

// ---------------------------------------------------------------------------------------------------- card layout (batch 18)

float DownloadNode::cardWidth = 0, DownloadNode::cardGap = 0;
float DownloadNode::buttonBaseX = 0, DownloadNode::buttonBaseY = 0, DownloadNode::buttonWidth = 0, DownloadNode::buttonHeight = 0,
      DownloadNode::buttonOffset = 0;
int DownloadNode::maxResultsShown_ = 0;
float DownloadNode::listHeight = 0, DownloadNode::bottomReserve = 0;
const CardFonts* DownloadNode::fonts_ = nullptr;

void DownloadNode::init(int width, int height, const CardFonts& fonts) {
    fonts_ = &fonts;
    using F = CardFonts::Font;
    float searchY = (static_cast<float>(height) * 0.04f) + static_cast<float>(fonts.lineHeight(F::LARGE));

    // download result cards (full width, COLUMNS columns)
    buttonBaseX = static_cast<float>(width) * 0.024f;
    buttonWidth = static_cast<float>(width) - buttonBaseX * 2.f;
    buttonHeight = static_cast<float>(fonts.lineHeight(F::BOLD)) + static_cast<float>(fonts.lineHeight(F::DEFAULT)) * 5.9f;
    cardGap = buttonWidth * 0.010f;
    cardWidth = (buttonWidth - cardGap * (COLUMNS - 1)) / COLUMNS;
    buttonBaseY = searchY + static_cast<float>(fonts.lineHeight(F::MEDIUM)) + static_cast<float>(fonts.lineHeight(F::BOLD)) +
                  static_cast<float>(fonts.lineHeight(F::BOLD));
    buttonOffset = buttonHeight * 1.07f;

    bottomReserve = static_cast<float>(height) * 0.09f;
    // the list uses all the room above the bottom buttons (a row may be cut off while scrolling, never at rest)
    listHeight = java::max(buttonOffset, static_cast<float>(height) - buttonBaseY - bottomReserve);
    maxResultsShown_ = java::max(1, java::toInt(std::ceil(static_cast<double>(listHeight / buttonOffset))));
}

float DownloadNode::btnH() {
    if (!fonts_) throw std::logic_error("DownloadNode::init() has not been called");
    return static_cast<float>(fonts_->lineHeight(CardFonts::Font::DEFAULT)) * 1.7f;
}

bool DownloadNode::resultContains(float cx, float cy, int row, int col) {
    float x = cardX(col), y = buttonBaseY + (row * buttonOffset);
    return ((cx > x && cx < x + cardWidth) && (cy > y && cy < y + buttonHeight));
}

bool DownloadNode::resultThumbContains(float cx, float cy, int row, int col) {
    float x = cardX(col), y = buttonBaseY + (row * buttonOffset);
    return ((cx > x && cx < x + thumbW()) && (cy > y && cy < y + buttonHeight));
}

bool DownloadNode::resultDownloadContains(float cx, float cy, int row, int col) {
    float x = textLeft(col), y = btnY(row);
    return ((cx > x && cx < textRight(col)) && (cy > y && cy < y + btnH()));
}

std::array<float, 4> DownloadNode::getCardRect(int zone, int col) {
    if (zone == 1) return {cardX(col), buttonBaseY, thumbW(), buttonHeight};
    if (zone == 2) return {textLeft(col), btnY(0), textRight(col) - textLeft(col), btnH()};
    return {cardX(col), buttonBaseY, cardWidth, buttonHeight};
}

bool DownloadNode::resultAreaContains(float cx, float cy) {
    return ((cx > buttonBaseX && cx < buttonBaseX + buttonWidth) && (cy > buttonBaseY && cy < buttonBaseY + listHeight));
}

ClipRect DownloadNode::resultAreaClip() {
    return {java::toInt(buttonBaseX), java::toInt(buttonBaseY), java::toInt(buttonWidth), java::toInt(listHeight)};
}

std::string DownloadNode::fit(const CardFonts& fonts, CardFonts::Font font, const std::string& str, float maxW) {
    if (static_cast<float>(fonts.width(font, str)) <= maxW) return str;
    int end = static_cast<int>(java::utf16Length(str));
    while (end > 1 && static_cast<float>(fonts.width(font, java::utf16Prefix(str, static_cast<size_t>(end)) + "...")) > maxW) end--;
    return java::utf16Prefix(str, static_cast<size_t>(end)) + "...";
}

std::vector<CardRect> DownloadNode::downloadIconRects(float cx, float cy, float bh) {
    float s = bh * 0.26f;
    float shaftW = java::max(3.f, s * 0.42f);
    std::vector<CardRect> r;
    auto rect = [&r](float x, float y, float w, float h) {
        if (w > 0.f && h > 0.f) r.push_back({x, y, w, h});  // Java rect(): skipped when empty
    };
    rect(cx - shaftW / 2, cy - s * 1.05f, shaftW, s * 1.0f);       // shaft
    rect(cx - s * 0.95f, cy - s * 0.1f, s * 1.9f, s * 0.36f);      // arrow head, stepped
    rect(cx - s * 0.62f, cy + s * 0.26f, s * 1.24f, s * 0.36f);
    rect(cx - s * 0.28f, cy + s * 0.62f, s * 0.56f, s * 0.34f);
    rect(cx - s * 1.05f, cy + s * 1.15f, s * 2.1f, java::max(2.f, s * 0.2f));  // base line
    return r;
}

void DownloadNode::updateFit(const CardFonts& fonts, bool useUnicode, float textW) {
    using F = CardFonts::Font;
    std::string t = getTitle(useUnicode), a = getArtist(useUnicode);
    // Java compared the String REFERENCES; comparing values rebuilds in exactly the same situations except
    // when two different String objects hold equal text, where the rebuilt result is identical anyway.
    if (textW == fitW && fitSrcTitle && *fitSrcTitle == t && fitSrcArtist && *fitSrcArtist == a) return;
    if (useUnicode) {  // load glyphs
        fonts.loadGlyphs(F::BOLD, t);
        fonts.loadGlyphs(F::DEFAULT, a);
    }
    fitW = textW;
    fitSrcTitle = t;
    fitSrcArtist = a;
    chipW = !starLabel.empty()
                ? static_cast<float>(fonts.width(F::DEFAULT, starLabel)) + static_cast<float>(fonts.lineHeight(F::DEFAULT)) * 1.45f
                : 0.f;  // padding + star.png
    fTitle = fit(fonts, F::BOLD, t, textW - ((chipW > 0.f) ? chipW + margin() : 0.f));
    fArtist = fit(fonts, F::DEFAULT, a, textW);
    fMapper = fit(fonts, F::DEFAULT, "Mapped by " + creator, textW);
}

void DownloadNode::animate(float k, float glowTarget, float previewTarget, bool loadedInLibrary, std::optional<DownloadSnapshot> dl) {
    glow = ease(glow, glowTarget, k);
    previewMix = ease(previewMix, previewTarget, k);

    loaded = loadedInLibrary;
    dlActive = dl.has_value();
    if (dlActive) {
        dlStatus = dl->status;
        float target = (dl->status == Download::Status::COMPLETE) ? 100.f : java::max(0.f, dl->progress);
        shownPct = ease(shownPct, target, k);
        int p = java::toInt(shownPct);
        if (p != pctShown) {
            pctShown = p;
            pctLabel = std::to_string(p) + "%";
        }
    } else {
        shownPct = 0.f;
        pctShown = -1;
    }
}

void DownloadNode::animate(float k, float glowTarget, float previewTarget, bool loadedInLibrary) {
    std::optional<DownloadSnapshot> snap;
    if (const Download* d = DownloadList::get().getDownload(beatmapSetID)) snap = DownloadSnapshot{d->getStatus(), d->getProgress()};
    animate(k, glowTarget, previewTarget, loadedInLibrary, snap);
}

}  // namespace opsu
