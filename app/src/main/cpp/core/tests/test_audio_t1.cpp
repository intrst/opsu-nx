// T1 tests: audio interface, SilentAudioBackend, Music wrapper, MusicController, SoundController, MultiClip.
// Everything runs on a fake clock, so the timing assertions are exact and the test is instant.
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "opsu/audio/IAudioBackend.h"
#include "opsu/audio/MultiClip.h"
#include "opsu/audio/MusicController.h"
#include "opsu/audio/SilentAudioBackend.h"
#include "opsu/audio/SoundController.h"
#include "opsu/beatmap/Beatmap.h"
#include "opsu/beatmap/HitObject.h"
#include "opsu/util/JavaIO.h"

using namespace opsu;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)
static bool near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps; }

static int64_t g_now = 1000000;
static void advance(int ms) { g_now += ms; }

// ---------------------------------------------------------------------------------------------------------------------
// a recording backend: Silent music + clips that log what they were asked to do
// ---------------------------------------------------------------------------------------------------------------------
struct ClipLog {
    std::string path;
    std::vector<float> plays;                        // volumes of play()
    std::vector<std::pair<float, float>> loops;      // volume, frequency
    std::vector<int> stoppedChannels;
    std::vector<std::pair<int, float>> channelVolumes, channelFreqs;
    int stops = 0, destroys = 0;
    int nextChannel = 1;
    bool playFails = false;
    std::function<void()> lastOnEnd;
};
static std::vector<std::shared_ptr<ClipLog>> g_clipLogs;
static std::vector<std::string> g_streamUrls;

struct RecClip : ClipImplementation {
    std::shared_ptr<ClipLog> log;
    explicit RecClip(std::shared_ptr<ClipLog> l) : log(std::move(l)) {}
    void stop() override { log->stops++; }
    int play(float v, std::function<void()> onEnd) override {
        if (log->playFails) return 0;
        log->plays.push_back(v);
        log->lastOnEnd = std::move(onEnd);
        return log->nextChannel++;
    }
    void destroy() override { log->destroys++; }
    int playLoop(float v, float f) override { log->loops.push_back({v, f}); return log->nextChannel++; }
    void setChannelVolume(int c, float v) override { log->channelVolumes.push_back({c, v}); }
    void setChannelFrequency(int c, float f) override { log->channelFreqs.push_back({c, f}); }
    void stopChannel(int c) override { log->stoppedChannels.push_back(c); }
};

static int g_musicDisposes = 0;   // total backend dispose() calls (the RecMusic itself is gone after the Music is released)
struct RecMusic : AbsMusic {
    std::unique_ptr<AbsMusic> inner;
    std::vector<float> lowPass;
    int disposes = 0;
    float volume = -1;
    std::vector<std::string> calls;
    explicit RecMusic(std::unique_ptr<AbsMusic> i) : inner(std::move(i)) {}
    bool setPosition(float s) override { calls.push_back("setPosition"); return inner->setPosition(s); }
    void loop() override { calls.push_back("loop"); inner->loop(); }
    void play() override { calls.push_back("play"); inner->play(); }
    void pause() override { calls.push_back("pause"); inner->pause(); }
    void resume() override { calls.push_back("resume"); inner->resume(); }
    void stop() override { calls.push_back("stop"); inner->stop(); }
    void dispose() override { disposes++; g_musicDisposes++; inner->dispose(); }
    bool playing() override { return inner->playing(); }
    float getPosition() override { return inner->getPosition(); }
    std::string getName() override { return inner->getName(); }
    void setVolume(float v) override { volume = v; inner->setVolume(v); }
    void setPitch(float p) override { inner->setPitch(p); }
    void setLowPass(float hz) override { lowPass.push_back(hz); }
    bool getFFT(float* out) override { out[0] = 0.5f; return true; }
    void end() { fireMusicEnded(); }
};
static RecMusic* g_lastMusic = nullptr;
static bool g_failMusic = false;

struct RecBackend : IAudioBackend {
    SilentAudioBackend silent;
    std::unique_ptr<AbsMusic> createMusic(const std::string& path) override {
        if (g_failMusic) throw java::IOException("cannot open " + path);
        auto m = std::make_unique<RecMusic>(silent.createMusic(path));
        g_lastMusic = m.get();
        return m;
    }
    std::unique_ptr<ClipImplementation> createClip(const std::string& path) override {
        auto l = std::make_shared<ClipLog>();
        l->path = path;
        g_clipLogs.push_back(l);
        return std::make_unique<RecClip>(l);
    }
    std::unique_ptr<ClipImplementation> createStreamClip(const std::string& url) override {
        g_streamUrls.push_back(url);
        auto l = std::make_shared<ClipLog>();
        l->path = url;
        g_clipLogs.push_back(l);
        return std::make_unique<RecClip>(l);
    }
};

static std::shared_ptr<ClipLog> logFor(const std::string& path) {
    for (auto& l : g_clipLogs) if (l->path == path) return l;
    return nullptr;
}

static Beatmap makeMap(const std::string& audio, int preview, bool withTiming = true) {
    Beatmap b("/songs/x/map.osu");
    b.audioFilename = audio;
    b.previewTime = preview;
    b.endTime = 90000;
    if (withTiming) b.timingPoints = std::vector<TimingPoint>{TimingPoint("0,500,4,1,0,100,1,0"), TimingPoint("10000,-100,4,1,0,100,0,0"),
                                                               TimingPoint("20000,400,4,1,0,100,1,0")};
    return b;
}

// ---------------------------------------------------------------------------------------------------------------------
static void testRegistry() {
    setAudioBackend(nullptr);
    IAudioBackend& b = audioBackend();  // falls back to silent
    CHECK(dynamic_cast<SilentAudioBackend*>(&b) != nullptr);
    CHECK(&audioBackend() == &b);       // registered, not recreated
    auto rec = std::make_shared<RecBackend>();
    setAudioBackend(rec);
    CHECK(&audioBackend() == rec.get());
    setAudioBackend(nullptr);
    setAudioBackend(rec);
}

static void testSilent() {
    SilentAudioBackend sb;
    auto m = sb.createMusic("a.mp3");
    CHECK(m->getName() == "a.mp3");
    CHECK(!m->playing() && m->getPosition() == 0.f);
    m->play();
    advance(1000);
    CHECK(near(m->getPosition(), 1.0f));
    CHECK(m->playing());
    m->pause();
    advance(500);
    CHECK(!m->playing() && near(m->getPosition(), 1.0f));  // pause freezes
    m->resume();
    advance(250);
    CHECK(near(m->getPosition(), 1.25f));
    CHECK(m->setPosition(5.f));
    CHECK(near(m->getPosition(), 5.f));
    advance(100);
    CHECK(near(m->getPosition(), 5.1f));
    m->setPosition(-3.f);  // clamped to 0 like Java
    CHECK(near(m->getPosition(), 0.f));
    m->setPosition(10.f);
    m->setPitch(2.f);      // pitch change keeps the position and doubles the speed
    CHECK(near(m->getPosition(), 10.f));
    advance(500);
    CHECK(near(m->getPosition(), 11.f));
    m->setPitch(0.5f);
    advance(1000);
    CHECK(near(m->getPosition(), 11.5f));
    m->stop();
    CHECK(!m->playing() && near(m->getPosition(), 11.5f));
    m->loop();             // loop() == resume()
    CHECK(m->playing());
    m->dispose();
    CHECK(!m->playing());
    float fft[256];
    float lv[2];
    CHECK(!m->getFFT(fft) && !m->getLevels(lv));  // defaults
    m->setLowPass(300.f);                          // default no-op, must not crash

    auto c = sb.createClip("x.wav");
    CHECK(c->play(1.f, nullptr) == 0 && c->playLoop(1.f, 1.f) == 0);
    c->setChannelVolume(1, 1.f); c->setChannelFrequency(1, 1.f); c->stopChannel(1); c->stop(); c->destroy();
    CHECK(sb.createStreamClip("http://x/y.mp3") != nullptr);
}

static void testMusicWrapper() {
    auto rec = std::make_shared<RecBackend>();
    setAudioBackend(rec);
    auto m = Music::create("song.mp3");
    CHECK(g_lastMusic != nullptr && m->backend() == g_lastMusic);

    // plain play + smoothed position follows the backend
    m->play();
    advance(1000);
    CHECK(near(m->getPosition(), 1.0f, 0.02f));
    advance(1000);
    CHECK(near(m->getPosition(), 2.0f, 0.02f));
    m->pause();
    CHECK(!m->playing());
    float p = m->getPosition();
    advance(300);
    CHECK(near(m->getPosition(), p));

    // lead-in: negative position while playing, virtual clock, then the backend takes over at the right place
    m->setPosition(-2.f);
    CHECK(m->playing() == false);          // was paused
    CHECK(near(m->getPosition(), -2.f));
    m->resume();
    CHECK(m->playing());
    advance(1500);
    CHECK(near(m->getPosition(), -0.5f, 1e-3f));
    CHECK(g_lastMusic->inner->playing() == false);   // backend still held at 0
    advance(1000);                                    // virtual clock reaches +0.5: backend starts there
    float q = m->getPosition();
    CHECK(near(q, 0.5f, 0.02f));
    CHECK(g_lastMusic->inner->playing());
    CHECK(near(g_lastMusic->inner->getPosition(), 0.5f, 0.02f));
    advance(500);
    CHECK(near(m->getPosition(), 1.0f, 0.02f));

    // lead-in that is paused keeps its value; pitch rebases correctly
    m->setPosition(-1.f);
    m->pause();
    advance(777);
    CHECK(near(m->getPosition(), -1.f));
    m->resume();
    m->setPitch(2.f);                                 // rebase before the pitch changes
    advance(250);
    CHECK(near(m->getPosition(), -0.5f, 1e-3f));      // 0.25 s * pitch 2
    m->setPitch(1.f);

    // skipping out of the lead-in while the virtual clock runs starts the backend (else the game would wait forever)
    m->setPosition(-5.f);
    m->resume();
    m->setPosition(3.f);
    CHECK(g_lastMusic->inner->playing());
    CHECK(near(m->getPosition(), 3.f, 0.02f));

    // listeners + completion callback of the backend
    int ended = 0;
    m->addListener([&](Music& mm) { if (&mm == m.get()) ended++; });
    g_lastMusic->end();
    CHECK(ended == 1);

    // FFT/levels require playing
    float fft[256] = {0};
    CHECK(m->getFFT(fft) && fft[0] == 0.5f);
    m->pause();
    CHECK(!m->getFFT(fft));
    float lv[2];
    CHECK(!m->getLevels(lv));

    // dispose twice is safe, only released once
    RecMusic* rm = g_lastMusic;
    m->dispose();
    m->dispose();
    CHECK(rm->disposes == 1);
    rm->end();   // callbacks are detached after dispose
    CHECK(ended == 1);

    // load failure: IOException -> runtime_error
    g_failMusic = true;
    bool thrown = false;
    try { Music::create("bad.mp3"); } catch (const std::runtime_error& e) { thrown = std::string(e.what()).find("Could not load music: bad.mp3") == 0; }
    CHECK(thrown);
    g_failMusic = false;
}

static void testFades() {
    setAudioBackend(std::make_shared<RecBackend>());
    auto m = Music::create("f.mp3");
    m->play();
    // pitch fade 1 -> 0.5 over 1000 ms, ticked
    m->pitchFade(1000, 0.5f);
    advance(500);
    m->updateFades();
    CHECK(near(m->getPitch(), 0.75f, 1e-3f));
    advance(400);
    m->updateFades();
    CHECK(near(m->getPitch(), 0.55f, 1e-3f));
    advance(200);   // past the end: Java stops without applying that step
    m->updateFades();
    CHECK(near(m->getPitch(), 0.55f, 1e-3f));
    advance(500);
    m->updateFades();
    CHECK(near(m->getPitch(), 0.55f, 1e-3f));  // fade is over

    // volume fade: pauses at the end when asked to
    m->setPitch(1.f);
    m->fade(300, 0.f, true);
    advance(100); m->updateFades();
    CHECK(m->playing());
    advance(300); m->updateFades();
    CHECK(!m->playing());

    // dispose cancels fades
    m->resume();
    m->fade(100, 0.f, true);
    m->dispose();
    advance(500);
    m->updateFades();   // must not touch the released track
}

static void testMusicController() {
    auto rec = std::make_shared<RecBackend>();
    setAudioBackend(rec);
    MusicController::reset();
    std::map<std::string, bool> files{{"/songs/x/a.mp3", true}, {"/songs/x/b.mp3", true}, {"/songs/x/c.flac", true}};
    std::vector<std::string> bars, errors;
    float musicVol = 0.5f, masterVol = 0.8f;
    int offset = 25;
    MusicController::Host h;
    h.musicVolume = [&] { return musicVol; };
    h.masterVolume = [&] { return masterVol; };
    h.musicOffset = [&] { return offset; };
    h.trackExists = [&](const std::string& p) { return files.count(p) > 0; };
    h.barNotification = [&](const std::string& t) { bars.push_back(t); };
    h.error = [&](const std::string& m, const std::exception*, bool) { errors.push_back(m); };
    MusicController::setHost(h);

    CHECK(!MusicController::trackExists());
    CHECK(MusicController::getPosition(false) == 0);
    CHECK(MusicController::getDuration() == -1);
    CHECK(MusicController::getBeatmap() == nullptr);

    // missing track: notification, no track, lastBeatmap untouched
    Beatmap missing = makeMap("/songs/x/none.mp3", 1000);
    MusicController::play(missing, false, true);
    CHECK(bars.size() == 1 && bars[0] == "Could not find track 'none.mp3'.");
    CHECK(!MusicController::trackExists() && MusicController::getBeatmap() == nullptr);

    // unsupported extension: reset ran, no track, but lastBeatmap is set (Java)
    Beatmap flac = makeMap("/songs/x/c.flac", 0);
    MusicController::play(flac, false, true);
    CHECK(!MusicController::trackExists() && MusicController::getBeatmap() == &flac);

    // normal play at the preview time, volume = music * master
    Beatmap a = makeMap("/songs/x/a.mp3", 5000);
    MusicController::play(a, false, true);
    CHECK(MusicController::trackExists() && MusicController::isPlaying());
    CHECK(near(g_lastMusic->volume, 0.4f));
    CHECK(MusicController::getBeatmap() == &a);
    CHECK(MusicController::getPosition(false) >= 5000 && MusicController::getPosition(false) < 5030);
    CHECK(MusicController::getTotalOffset() == 25);
    int withOff = MusicController::getPosition(true), without = MusicController::getPosition(false);
    CHECK(withOff - without == 25);
    CHECK(MusicController::getDuration() == 90000);

    // same file, same preview: nothing happens; other preview time: seek
    RecMusic* firstTrack = g_lastMusic;
    advance(1000);
    MusicController::play(a, false, true);
    Beatmap a2 = makeMap("/songs/x/a.mp3", 8000);
    MusicController::play(a2, false, true);
    CHECK(g_lastMusic == firstTrack);   // no new track
    CHECK(MusicController::getPosition(false) >= 8000 && MusicController::getPosition(false) < 8030);
    CHECK(MusicController::getBeatmap() == &a2);

    // local offset of the beatmap counts; setTrackPosition inverts getPosition(true)
    a2.localMusicOffset = -10;
    CHECK(MusicController::getTotalOffset() == 15);
    MusicController::setTrackPosition(12000);
    int pos = MusicController::getPosition(true);
    CHECK(pos >= 12000 && pos < 12030);
    MusicController::setPosition(3000);
    CHECK(MusicController::getPosition(false) >= 3000 && MusicController::getPosition(false) < 3030);

    // pause / resume / isPaused / stop
    MusicController::pause();
    CHECK(!MusicController::isPlaying() && MusicController::isPaused());
    int frozen = MusicController::getPosition(false);
    advance(500);
    CHECK(MusicController::getPosition(false) == frozen);
    MusicController::resume();
    CHECK(MusicController::isPlaying() && !MusicController::isPaused());
    advance(500);
    CHECK(MusicController::getPosition(false) >= frozen + 480);

    // pitch
    MusicController::setPitch(2.f);
    int p0 = MusicController::getPosition(false);
    advance(500);
    int p1 = MusicController::getPosition(false);
    CHECK(p1 - p0 >= 980 && p1 - p0 <= 1020);
    MusicController::setPitch(1.f);

    // dim
    MusicController::toggleTrackDimmed(0.25f);
    CHECK(MusicController::isTrackDimmed() && near(g_lastMusic->volume, 0.5f * 0.8f * 0.25f));
    MusicController::toggleTrackDimmed(0.25f);
    CHECK(!MusicController::isTrackDimmed() && near(g_lastMusic->volume, 0.4f));

    // track end -> trackEnded, loopTrackIfEnded restarts at preview
    CHECK(!MusicController::trackEnded());
    g_lastMusic->end();
    CHECK(MusicController::trackEnded());
    MusicController::loopTrackIfEnded(true);
    CHECK(!MusicController::trackEnded());
    CHECK(MusicController::getPosition(false) >= 8000 && MusicController::getPosition(false) < 8030);

    // FFT / levels
    float fft[256];
    CHECK(MusicController::getFFT(fft) && fft[0] == 0.5f);
    float lv[2];
    CHECK(!MusicController::getLevels(lv));

    // start volume scale
    MusicController::setStartVolumeScale(0.f);
    Beatmap b = makeMap("/songs/x/b.mp3", 0);
    MusicController::play(b, true, false);
    CHECK(near(g_lastMusic->volume, 0.f));
    CHECK(g_lastMusic->calls.size() >= 1 && g_lastMusic->calls[0] == "loop");
    MusicController::setStartVolumeScale(1.f);
    CHECK(MusicController::getPlayer() != nullptr);

    // load failure goes through the error seam, no track
    g_failMusic = true;
    Beatmap a3 = makeMap("/songs/x/a.mp3", 0);
    MusicController::play(a3, false, false);
    CHECK(!MusicController::trackExists() && errors.size() == 1 && errors[0] == "Could not play track 'a.mp3'.");
    g_failMusic = false;

    // reset releases the track
    // (Java quirk kept: after the failed load above lastBeatmap is already `a3`, so play() of the same file would not retry)
    MusicController::reset();
    MusicController::play(a, false, false);
    int disposesBefore = g_musicDisposes;
    CHECK(MusicController::trackExists());
    MusicController::reset();
    CHECK(!MusicController::trackExists() && g_musicDisposes == disposesBefore + 1 && MusicController::getBeatmap() == nullptr);

    // theme song
    h.themeBeatmap = [&]() -> const Beatmap* { return &a; };
    MusicController::setHost(h);
    MusicController::playThemeSong();
    CHECK(MusicController::isThemePlaying() && MusicController::isPlaying());
    MusicController::reset();
    CHECK(!MusicController::isThemePlaying());
}

static void testTiming() {
    setAudioBackend(std::make_shared<RecBackend>());
    MusicController::reset();
    MusicController::Host h;
    h.trackExists = [](const std::string&) { return true; };
    MusicController::setHost(h);

    // nothing playing: 60 bpm from the clock, no measure, default timing points
    float bp = MusicController::getBeatProgress();
    CHECK(bp >= 0.f && bp < 1.f);
    CHECK(!MusicController::getMeasureProgress().has_value());
    CHECK(MusicController::getLastTimingPoint().getBeatLength() == TimingPoint::DEFAULT.getBeatLength());

    Beatmap m = makeMap("/s/a.mp3", 0);
    MusicController::play(m, false, false);
    MusicController::setPosition(750);
    // beat length 500, first point at 0: 750 ms is half a beat in
    CHECK(near(MusicController::getBeatProgress(), 0.5f, 0.01f));
    // measure: 4 beats = 2000 ms, 750/2000
    CHECK(near(*MusicController::getMeasureProgress(), 0.375f, 0.01f));
    CHECK(near(*MusicController::getMeasureProgress(2), 0.1875f, 0.01f));

    // inherited point (10000) does not replace the non-inherited one, but is the "last timing point"
    MusicController::setPosition(10500);
    CHECK(MusicController::getLastNonInheritedTimingPoint().getBeatLength() == 500.f);
    CHECK(MusicController::getLastTimingPoint().isInherited());
    // the third point (20000, beat length 400) takes over
    MusicController::setPosition(20100);
    CHECK(MusicController::getLastNonInheritedTimingPoint().getBeatLength() == 400.f);

    // paused: no timing info
    MusicController::pause();
    CHECK(!MusicController::getMeasureProgress().has_value());

    // no timing points at all
    MusicController::reset();
    Beatmap noTiming = makeMap("/s/a.mp3", 0, false);
    MusicController::play(noTiming, false, false);
    CHECK(!MusicController::getMeasureProgress().has_value());
    MusicController::reset();
}

static void testMuffle() {
    setAudioBackend(std::make_shared<RecBackend>());
    MusicController::reset();
    MusicController::Host h;
    h.trackExists = [](const std::string&) { return true; };
    MusicController::setHost(h);
    // (a muffle left over from an earlier test must not exist)
    MusicController::setMuffled(false);
    for (int i = 0; i < 100; i++) MusicController::updateMuffle(16);

    Beatmap m = makeMap("/s/a.mp3", 0);
    MusicController::play(m, false, false);
    CHECK(g_lastMusic->lowPass.size() == 1 && g_lastMusic->lowPass[0] >= MusicController::LOWPASS_OFF);  // forced once for a new player
    g_lastMusic->lowPass.clear();

    // clear: no calls
    for (int i = 0; i < 10; i++) MusicController::updateMuffle(16);
    CHECK(g_lastMusic->lowPass.empty());

    // sweep in: 500 ms to 300 Hz; halfway (eased 0.5) is the log-frequency midpoint
    MusicController::setMuffled(true);
    MusicController::updateMuffle(250);   // delta is clamped to 50 ms per frame
    CHECK(g_lastMusic->lowPass.size() == 1);  // 50 ms only
    g_lastMusic->lowPass.clear();
    for (int i = 0; i < 4; i++) MusicController::updateMuffle(50);   // total 250 ms = amount 0.5
    float mid = g_lastMusic->lowPass.back();
    CHECK(near(mid, std::sqrt(20000.f * 300.f), 1.f));
    for (int i = 0; i < 10; i++) MusicController::updateMuffle(50);  // reaches 300 and stays
    CHECK(near(g_lastMusic->lowPass.back(), 300.f, 0.5f));
    // monotonic decreasing sweep
    for (size_t i = 1; i < g_lastMusic->lowPass.size(); i++) CHECK(g_lastMusic->lowPass[i] <= g_lastMusic->lowPass[i - 1] + 1e-3f);
    size_t n = g_lastMusic->lowPass.size();
    MusicController::updateMuffle(16);
    CHECK(g_lastMusic->lowPass.size() == n);   // no change -> no call (it is called every frame)

    // volume adjusted while muffled: opens up, stays open for 1500 ms, then muffles again
    g_lastMusic->lowPass.clear();
    MusicController::onVolumeAdjusted();
    for (int i = 0; i < 14; i++) MusicController::updateMuffle(50);   // 700 ms: sweeping out
    CHECK(g_lastMusic->lowPass.back() >= MusicController::LOWPASS_OFF);
    for (int i = 0; i < 30; i++) MusicController::updateMuffle(50);   // hold (1500 ms) is over, then 500 ms sweep in: muffled again
    CHECK(g_lastMusic->lowPass.back() < 400.f);

    // restore
    MusicController::setMuffled(false);
    g_lastMusic->lowPass.clear();
    for (int i = 0; i < 20; i++) MusicController::updateMuffle(50);
    CHECK(g_lastMusic->lowPass.back() >= MusicController::LOWPASS_OFF);
    size_t k = g_lastMusic->lowPass.size();
    for (int i = 0; i < 5; i++) MusicController::updateMuffle(50);
    CHECK(g_lastMusic->lowPass.size() == k);

    // a track created while muffled starts muffled from the first sample
    MusicController::setMuffled(true);
    for (int i = 0; i < 20; i++) MusicController::updateMuffle(50);
    Beatmap m2 = makeMap("/s/other.mp3", 0);
    MusicController::play(m2, false, false);
    CHECK(!g_lastMusic->lowPass.empty() && g_lastMusic->lowPass[0] < 400.f);
    MusicController::setMuffled(false);
    MusicController::reset();
}

static void testFadeOutViaController() {
    setAudioBackend(std::make_shared<RecBackend>());
    MusicController::reset();
    MusicController::Host h;
    h.trackExists = [](const std::string&) { return true; };
    MusicController::setHost(h);
    Beatmap m = makeMap("/s/a.mp3", 0);
    MusicController::play(m, false, false);
    MusicController::fadeOut(400);
    advance(200);
    MusicController::updateMuffle(16);
    CHECK(MusicController::isPlaying());
    advance(300);
    MusicController::updateMuffle(16);
    CHECK(!MusicController::isPlaying());
    MusicController::resume();
    MusicController::pitchFadeOut(1000);
    advance(500);
    MusicController::updateMuffle(16);
    CHECK(near(MusicController::getPlayer()->getPitch(), 0.5f, 1e-3f));
    MusicController::reset();
}

// ---------------------------------------------------------------------------------------------------------------------
static HitObject circle(int sound, const std::string& tail) {
    return HitObject("100,100,1000,1," + std::to_string(sound) + "," + tail);
}

static void testSoundController() {
    namespace fs = std::filesystem;
    g_clipLogs.clear();
    auto rec = std::make_shared<RecBackend>();
    setAudioBackend(rec);
    SoundController::resetForTesting();
    hitsound::resetForTesting();

    fs::path root = fs::temp_directory_path() / "opsu_t1_audio_test";
    fs::remove_all(root);
    fs::create_directories(root / "skin");
    fs::create_directories(root / "beatmap");
    auto touch = [](const fs::path& p) { std::ofstream(p) << "x"; };
    touch(root / "skin" / "menuclick.wav");
    touch(root / "skin" / "applause.ogg");

    std::set<std::string> res{"menuclick.wav", "applause.wav", "normal-hitnormal.wav", "normal-hitfinish.wav", "normal-hitwhistle.wav",
                              "normal-hitclap.wav", "normal-sliderslide.wav", "normal-slidertick.wav", "normal-sliderwhistle.wav",
                              "soft-hitnormal.wav", "soft-hitwhistle.wav", "spinnerspin.wav"};
    std::vector<std::string> errors, reds;
    float effect = 0.5f, hitVol = 0.6f, master = 0.5f, music = 0.4f;
    bool layered = true, gameplayOff = false;
    SoundController::Host h;
    h.effectVolume = [&] { return effect; };
    h.hitSoundVolume = [&] { return hitVol; };
    h.masterVolume = [&] { return master; };
    h.musicVolume = [&] { return music; };
    h.skinDirectory = [&] { return (root / "skin").string(); };
    h.layeredHitSounds = [&] { return layered; };
    h.gameplaySoundDisabled = [&] { return gameplayOff; };
    h.resourceExists = [&](const std::string& n) { return res.count(n) > 0; };
    h.error = [&](const std::string& m, const std::exception*, bool) { errors.push_back(m); };
    h.redNotification = [&](const std::string& t) { reds.push_back(t); };
    SoundController::setHost(h);

    CHECK(SoundController::getLoadingProgress() == -1);
    SoundController::init();
    CHECK(SoundController::getLoadingProgress() == -1 && !SoundController::getCurrentFileName().has_value());
    // skin files first (absolute path), wav before mp3 before ogg, then the resources
    CHECK(SoundController::getClip(SoundEffect::MENUCLICK) != nullptr);
    CHECK(logFor((root / "skin" / "menuclick.wav").string()) != nullptr);
    CHECK(logFor((root / "skin" / "applause.ogg").string()) != nullptr);   // the skin's .ogg beats the resource .wav
    CHECK(logFor("applause.wav") == nullptr);
    CHECK(logFor("spinnerspin.wav") != nullptr);
    // missing files are reported, the others still load
    CHECK(SoundController::getClip(SoundEffect::SHUTTER) == nullptr);
    CHECK(!errors.empty() && errors[0] == "Could not find sound file 'combobreak'.");
    bool hitErr = false;
    for (auto& e : errors) if (e == "Could not find hit sound file 'drum-hitclap'.") hitErr = true;
    CHECK(hitErr);
    CHECK(reds.empty());   // loadClip never fails (a failed load is a silent clip)
    CHECK(SoundController::getClip(HitSound::NORMAL, SampleSet::NORMAL) != nullptr);
    CHECK(SoundController::getClip(HitSound::NORMAL, SampleSet::DRUM) == nullptr);

    // playSound: effect * master; muted / zero volume do nothing
    SoundController::playSound(SoundEffect::MENUCLICK);
    auto lc = logFor((root / "skin" / "menuclick.wav").string());
    CHECK(lc->plays.size() == 1 && near(lc->plays[0], 0.25f));
    SoundController::playSound(SoundEffect::SHUTTER);  // no clip: nothing, no crash
    SoundController::mute(true);
    SoundController::playSound(SoundEffect::MENUCLICK);
    CHECK(lc->plays.size() == 1);
    CHECK(SoundController::playSoundChecked(SoundEffect::MENUCLICK));   // muted: "true" so no retry
    SoundController::mute(false);
    CHECK(SoundController::playSoundChecked(SoundEffect::MENUCLICK) && lc->plays.size() == 2);
    lc->playFails = true;
    CHECK(!SoundController::playSoundChecked(SoundEffect::MENUCLICK));  // backend could not start it
    lc->playFails = false;
    SoundController::stopSound(SoundEffect::MENUCLICK);
    CHECK(lc->stops == 1);
    effect = 0.f;
    SoundController::playSound(SoundEffect::MENUCLICK);
    CHECK(lc->plays.size() == 2);
    effect = 0.5f;

    // HitSound.getClip() (no argument): null until a sample set is selected
    auto nrm = logFor("normal-hitnormal.wav");
    SoundController::playSound(HitSound::NORMAL);
    CHECK(nrm->plays.empty());
    hitsound::setSampleSet(1);
    SoundController::playSound(HitSound::NORMAL);
    CHECK(nrm->plays.size() == 1);
    SoundController::setSampleVolume(0.5f);
    SoundController::setSampleVolume(2.f);   // ignored
    SoundController::playHitSound(HitSound::NORMAL);
    CHECK(nrm->plays.size() == 2 && near(nrm->plays[1], 0.6f * 0.5f * 0.5f));
    SoundController::setSampleVolume(1.f);

    // hit objects: normal + whistle + finish + clap, volume = hit * object/sample volume * master
    hitsound::setDefaultSampleSetType(1);
    nrm->plays.clear();
    auto whistle = logFor("normal-hitwhistle.wav"), finish = logFor("normal-hitfinish.wav"), clap = logFor("normal-hitclap.wav");
    HitObject hoPlain = circle(0, "0:0:0:0:");
    SoundController::playHitSound(hoPlain, 0);
    CHECK(nrm->plays.size() == 1 && near(nrm->plays[0], 0.6f * 1.f * 0.5f));
    HitObject hoAll = circle(2 | 4 | 8, "0:0:0:0:");   // whistle + finish + clap, no normal bit -> layered normal still plays (layered on)
    SoundController::playHitSound(hoAll, 0);
    CHECK(nrm->plays.size() == 2 && whistle->plays.size() == 1 && finish->plays.size() == 1 && clap->plays.size() == 1);
    layered = false;                                    // skin turns layered normal off when the type has no normal bit
    SoundController::playHitSound(hoAll, 0);
    CHECK(nrm->plays.size() == 2 && whistle->plays.size() == 2);
    HitObject hoNormalBit = circle(1 | 2, "0:0:0:0:");  // has the normal bit -> always plays
    SoundController::playHitSound(hoNormalBit, 0);
    CHECK(nrm->plays.size() == 3);
    layered = true;
    // own volume in the extras: "sampleSet:addSet:index:volume:file"
    HitObject hoVol = circle(0, "0:0:0:50:");
    SoundController::playHitSound(hoVol, 0);
    CHECK(near(nrm->plays.back(), 0.6f * 0.5f * 0.5f));
    // minimum sample volume 5%
    HitObject hoQuiet = circle(0, "0:0:0:1:");
    SoundController::playHitSound(hoQuiet, 0);
    CHECK(near(nrm->plays.back(), 0.6f * 0.05f * 0.5f));
    // sample set 2 of the object -> "soft-hitnormal"
    auto soft = logFor("soft-hitnormal.wav");
    HitObject hoSoft = circle(0, "2:0:0:0:");
    SoundController::playHitSound(hoSoft, 0);
    CHECK(soft->plays.size() == 1);

    // slider tick uses the object's own set
    SoundController::playSliderTick(hoPlain);
    CHECK(logFor("normal-slidertick.wav")->plays.size() == 1);

    // beatmap samples: matching names + referenced names, wav preferred, custom index suffix
    touch(root / "beatmap" / "normal-hitnormal.wav");
    touch(root / "beatmap" / "normal-hitnormal2.wav");
    touch(root / "beatmap" / "Soft-HitWhistle.WAV");   // case-insensitive
    touch(root / "beatmap" / "custom.mp3");            // only loaded when referenced
    touch(root / "beatmap" / "readme.txt");
    touch(root / "beatmap" / "unrelated.wav");
    size_t before = g_clipLogs.size();
    std::set<std::string> referenced{"custom"};
    SoundController::loadBeatmapSamples((root / "beatmap").string(), &referenced);
    CHECK(g_clipLogs.size() == before + 4);            // 2 hitnormal + whistle + custom
    SoundController::setTimingPointCustomIndex(1);
    nrm->plays.clear();
    SoundController::playHitSound(hoPlain, 0);
    CHECK(nrm->plays.empty());                         // the beatmap's own normal-hitnormal.wav replaces the skin one
    auto bmNormal = logFor(absolute(root / "beatmap" / "normal-hitnormal.wav").string());
    CHECK(bmNormal && bmNormal->plays.size() == 1);
    SoundController::setTimingPointCustomIndex(2);
    SoundController::playHitSound(hoPlain, 0);
    auto bmNormal2 = logFor(absolute(root / "beatmap" / "normal-hitnormal2.wav").string());
    CHECK(bmNormal2 && bmNormal2->plays.size() == 1);
    SoundController::setTimingPointCustomIndex(0);
    // an object naming a sample file replaces its normal sample
    HitObject hoFile = circle(0, "0:0:0:0:custom.mp3");
    SoundController::playHitSound(hoFile, 0);
    auto custom = logFor(absolute(root / "beatmap" / "custom.mp3").string());
    CHECK(custom && custom->plays.size() == 1);
    SoundController::clearBeatmapSamples();
    CHECK(bmNormal->destroys >= 1 && custom->destroys >= 1);
    SoundController::loadBeatmapSamples("", nullptr);   // null dir: just clear

    // loops
    layered = true;
    {
        HitObject slider("100,100,1000,2,2,L|200:200,1,100,2|0,0:0|0:0,0:0:0:0:");
        SoundController::SlidingSound ss(slider);
        ss.start();
        auto slide = logFor("normal-sliderslide.wav"), sw = logFor("normal-sliderwhistle.wav");
        CHECK(slide->loops.size() == 1 && sw->loops.size() == 1);
        ss.start();                                    // already playing: no second loop
        CHECK(slide->loops.size() == 1);
        SoundController::stopAllLoops();
        CHECK(slide->stoppedChannels.size() == 1 && sw->stoppedChannels.size() == 1);
        ss.start();
        CHECK(slide->loops.size() == 2);
        gameplayOff = true;
        ss.stop();
        ss.start();                                    // gameplay sounds disabled
        CHECK(slide->loops.size() == 2);
        gameplayOff = false;
        SoundController::mute(true);
        ss.start();
        CHECK(slide->loops.size() == 2);
        SoundController::mute(false);
        ss.start();
        CHECK(slide->loops.size() == 3);
        // mute stops running loops
        SoundController::mute(true);
        CHECK(slide->stoppedChannels.size() == 3);
        SoundController::mute(false);
        // a Loop that goes out of scope stops itself (and leaves no dangling pointer in the active set)
    }
    SoundController::stopAllLoops();
    {
        SoundController::Loop spin;
        auto clip = SoundController::getSpinnerSpinClip();
        CHECK(clip != nullptr);
        spin.start(clip, SoundController::getEffectLoopVolume(), 1.f);
        CHECK(spin.isPlaying());
        spin.setVolume(0.3f);
        spin.setFrequency(1.5f);
        auto sl = logFor("spinnerspin.wav");
        CHECK(sl->channelVolumes.size() == 1 && sl->channelFreqs.size() == 1 && near(sl->channelFreqs[0].second, 1.5f));
        spin.stop();
        spin.setVolume(1.f);                           // after stop: no call
        CHECK(sl->channelVolumes.size() == 1);
        spin.start(clip, 0.f, 1.f);                    // zero volume: no loop
        CHECK(!spin.isPlaying());
    }

    // intro sounds
    SoundController::preloadIntroSounds();
    SoundController::playIntroHit(true);
    CHECK(logFor("normal-hitfinish.wav")->plays.size() >= 1);

    // preview track from a URL
    std::vector<std::string> bars;
    h.barNotification = [&](const std::string& t) { bars.push_back(t); };
    SoundController::setHost(h);
    int ended = 0;
    CHECK(SoundController::playTrack("https://b.example/preview/1.mp3", "1.mp3", [&] { ended++; }));
    auto prev = logFor("https://b.example/preview/1.mp3");
    CHECK(prev && prev->plays.size() == 1 && near(prev->plays[0], 0.4f * 0.5f));
    prev->lastOnEnd();
    CHECK(ended == 1);
    CHECK(SoundController::playTrack("https://b.example/preview/2.mp3", "2.mp3", nullptr));  // stops the previous one
    CHECK(prev->destroys >= 1);
    CHECK(!SoundController::playTrack("not a url", "x", nullptr));
    CHECK(bars.size() == 1 && bars[0] == "Failed to load clip 'not a url'.");
    SoundController::stopTrack();

    // sound disabled: init does nothing, beatmap samples are not loaded
    SoundController::resetForTesting();
    h.soundDisabled = [] { return true; };
    SoundController::setHost(h);
    SoundController::init();
    CHECK(SoundController::getClip(SoundEffect::MENUCLICK) == nullptr);
    SoundController::preloadIntroSounds();

    // re-init replaces and destroys the previous clips
    h.soundDisabled = [] { return false; };
    SoundController::setHost(h);
    SoundController::init();
    auto oldClip = SoundController::getClip(SoundEffect::MENUCLICK);
    auto oldLog = logFor((root / "skin" / "menuclick.wav").string());
    SoundController::init();
    CHECK(SoundController::getClip(SoundEffect::MENUCLICK) != oldClip);
    CHECK(oldLog->destroys >= 1);

    fs::remove_all(root);
    SoundController::resetForTesting();
    hitsound::resetForTesting();
}

static void testMultiClip() {
    g_clipLogs.clear();
    g_streamUrls.clear();
    setAudioBackend(std::make_shared<RecBackend>());
    {
        MultiClip c("a.wav");
        auto l = logFor("a.wav");
        CHECK(c.getLastPlayId() == 0);
        c.start(0.7f);
        CHECK(c.getLastPlayId() == 1 && near(l->plays[0], 0.7f));
        int id = c.startLoop(0.5f, 1.25f);
        CHECK(id != 0 && near(l->loops[0].second, 1.25f));
        c.setLoopVolume(0, 1.f); c.setLoopFrequency(0, 1.f); c.stopLoop(0);      // id 0 is ignored
        CHECK(l->channelVolumes.empty() && l->channelFreqs.empty() && l->stoppedChannels.empty());
        c.setLoopVolume(id, 0.2f); c.setLoopFrequency(id, 2.f); c.stopLoop(id);
        CHECK(l->channelVolumes.size() == 1 && l->channelFreqs.size() == 1 && l->stoppedChannels.size() == 1);
        c.stop();
        CHECK(l->stops == 1);
        MultiClip::destroyExtraClips();
        CHECK(l->destroys == 0);
    }
    CHECK(logFor("a.wav")->destroys == 1);   // destroyed with the clip
    MultiClip s("http://h/p.mp3", Clip::StreamTag{});
    CHECK(g_streamUrls.size() == 1 && g_streamUrls[0] == "http://h/p.mp3");

    // silent backend (no backend registered): everything is a harmless no-op
    setAudioBackend(nullptr);
    MultiClip silent("x.wav");
    silent.start(1.f);
    CHECK(silent.getLastPlayId() == 0 && silent.startLoop(1.f, 1.f) == 0);
}

int main() {
    setAudioClockForTesting([] { return g_now; });
    testRegistry();
    testSilent();
    testMusicWrapper();
    testFades();
    testMusicController();
    testTiming();
    testMuffle();
    testFadeOutViaController();
    testSoundController();
    testMultiClip();
    setAudioBackend(nullptr);
    setAudioClockForTesting(nullptr);
    if (failures == 0) std::printf("test_audio_t1: all passed\n");
    else std::printf("test_audio_t1: %d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
