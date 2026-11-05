// Audio side of the app: one soundcard callback that runs the amp engine, the metronome, the 4-track recorder and playback.
// Everything that touches audio samples happens here, sample-accurately, on the audio thread.
#pragma once
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_core/juce_core.h>
#include <array>
#include <memory>
#include "Engine.h"

namespace alt
{
constexpr int NumTracks = 4;
constexpr int PeakBucket = 1024;

// One recorded take: the clean guitar (dry) and the amp sound as heard (wet), already aligned to the click.
struct TrackData
{
    std::vector<float> dry, wl, wr, peaks;
    int64_t length() const { return (int64_t) dry.size(); }
    void makePeaks()
    {
        const size_t nb = (dry.size() + PeakBucket - 1) / PeakBucket; peaks.assign (nb, 0.f);
        for (size_t b = 0; b < nb; ++b)
        {
            float m = 0; const size_t e = std::min (dry.size(), (b + 1) * (size_t) PeakBucket);
            for (size_t i = b * (size_t) PeakBucket; i < e; ++i) m = std::max (m, std::max (std::fabs (wl[i]), std::fabs (wr[i])));
            peaks[b] = m;
        }
    }
};

struct TrackState
{
    std::atomic<float> vol { 1.f };
    std::atomic<bool> mute { false }, solo { false }, reamp { false };
    juce::SpinLock lock;
    std::shared_ptr<const TrackData> data;
    std::shared_ptr<const TrackData> get() { const juce::SpinLock::ScopedLockType l (lock); return data; }
    std::shared_ptr<const TrackData> tryGet() { const juce::SpinLock::ScopedTryLockType l (lock); return l.isLocked() ? data : nullptr; }
    void set (std::shared_ptr<const TrackData> d) { const juce::SpinLock::ScopedLockType l (lock); data = std::move (d); }
};

enum class Transport { Idle = 0, Play = 1, Rec = 2 };

class RigAudio : public juce::AudioIODeviceCallback
{
public:
    RigAudio() : writer (*this) { writer.startThread(); }
    ~RigAudio() override { writer.stopThread (2000); }

    // ---- settings from the interface ----
    void setParams (const Params& p) { const juce::SpinLock::ScopedLockType l (paramLock); params = p; ++paramVersion; }
    void setInputGainDb (double db) { gainTarget = (float) std::pow (10.0, db / 20); }
    void setInputChannel (int ch) { inChannel = ch; }          // -1 = mix of all active inputs
    void requestLearn() { learnReq = true; }
    Engine& getEngine() { return engine; }

    // ---- transport ----
    std::atomic<int> cmd { 0 };                                // 1 play, 2 rec, 3 stop
    std::atomic<int> state { 0 };
    std::atomic<int64_t> posOut { 0 };                         // timeline position in samples (negative during count-in)
    std::atomic<int> armed { 0 };
    std::atomic<int> compSamples { 0 };                        // latency compensation applied to the next take
    std::atomic<bool> metOn { false }, countIn { true };
    std::atomic<int> bpm { 100 }, beats { 4 };
    std::atomic<float> clickVol { 0.5f };
    std::atomic<int> beatFlash { 0 };                          // 1 = beat, 2 = accent; the interface clears it
    std::atomic<int> beatSamplesOut { 24000 };
    std::array<TrackState, NumTracks> tracks;

    // ---- meters ----
    std::atomic<float> peakIn { 0 }, peakOut { 0 };
    std::atomic<int> overruns { 0 };

    // ---- live recording display ----
    static constexpr int MaxLivePeaks = 60000;
    std::vector<float> livePeaks = std::vector<float> (MaxLivePeaks, 0.f);
    std::atomic<int> livePeakN { 0 };
    std::atomic<int> takeReady { -1 };                         // track index of a finished take waiting to be committed
    std::atomic<bool> takeEmpty { false };
    std::shared_ptr<TrackData> readyTake;

    double sampleRate() const { return sr; }

    // ---- tuner: the last 16384 input samples, plus a switch to silence the output while tuning ----
    static constexpr int TunerN = 16384;
    std::atomic<bool> muteOut { false };
    void copyTunerWindow (float* dst, int n) const
    {
        const uint32_t end = tunerPos.load (std::memory_order_acquire);
        for (int i = 0; i < n; ++i) dst[i] = tunerBuf[(end - (uint32_t) n + (uint32_t) i) & (TunerN - 1)];
    }

    // ---- noise sample: records the raw input and the amp output for a few seconds so it can be saved and examined ----
    std::atomic<bool> diagOn { false }, diagDone { false };
    std::vector<float> diagRaw, diagWet;
    void startDiag (double seconds)
    {
        diagOn = false; diagDone = false;
        const int n = (int) (seconds * sr); diagRaw.assign ((size_t) n, 0.f); diagWet.assign ((size_t) n, 0.f);
        diagLen = n; diagN = 0; diagOn = true;
    }

    void commitTake()
    {
        const int i = takeReady.load();
        if (i < 0) return;
        auto t = readyTake; readyTake.reset();
        if (t) { graveyard.push_back (tracks[(size_t) i].get()); tracks[(size_t) i].set (t); }
        takeReady = -1;
    }
    void clearTrack (int i) { graveyard.push_back (tracks[(size_t) i].get()); tracks[(size_t) i].set (nullptr); tracks[(size_t) i].reamp = false; }
    void collectGarbage() { graveyard.erase (std::remove_if (graveyard.begin(), graveyard.end(), [] (auto& p) { return p.use_count() <= 1; }), graveyard.end()); }
    int64_t longestTrack()
    {
        int64_t m = 0; for (auto& t : tracks) if (auto d = t.get()) m = std::max (m, d->length());
        return m;
    }

    // ---- AudioIODeviceCallback ----
    void audioDeviceAboutToStart (juce::AudioIODevice* d) override
    {
        sr = d->getCurrentSampleRate();
        engine.prepare (sr); seenVersion = -1; gainSmooth = gainTarget; state = 0; cmd = 0; tpos = 0; freeCount = 0; clickRemain = 0;
    }
    void audioDeviceStopped() override { state = 0; }

    void audioDeviceIOCallbackWithContext (const float* const* in, int nIn, float* const* out, int nOut, int n, const juce::AudioIODeviceCallbackContext&) override
    {
        juce::ScopedNoDenormals noDenormals;
        if (paramVersion != seenVersion)
        {
            const juce::SpinLock::ScopedTryLockType l (paramLock);
            if (l.isLocked()) { seenVersion = paramVersion; engine.setParams (params); }
        }
        if (learnReq.exchange (false)) engine.startLearn();
        handleCommands();

        int pos = 0;
        while (pos < n)
        {
            const int c = std::min (n - pos, kChunk);
            chunk (in, nIn, out, nOut, pos, c);
            pos += c;
        }
    }

private:
    static constexpr int kChunk = 256;
    double sr = 48000;
    Engine engine;
    juce::SpinLock paramLock; Params params; int paramVersion = 0, seenVersion = -1;
    std::atomic<bool> learnReq { false };
    std::atomic<float> gainTarget { 1.f }; float gainSmooth = 1.f;
    std::atomic<int> inChannel { 0 };
    float tunerBuf[TunerN] = {}; std::atomic<uint32_t> tunerPos { 0 }; std::atomic<int> diagLen { 0 }; int diagN = 0;
    int64_t tpos = 0, freeCount = 0, beatLen = 24000, countLen = 0, recLimit = 0;
    std::vector<std::shared_ptr<const TrackData>> graveyard;
    // click voice
    int clickRemain = 0, clickTotal = 0; double clickPhase = 0, clickFreq = 1000; float clickAmp = 0;
    // scratch
    float mono[kChunk], dryTap[kChunk], wetL[kChunk], wetR[kChunk];
    std::shared_ptr<const TrackData> td[NumTracks];
    bool useTd[NumTracks] = {};

    // ---- recording hand-off to a writer thread ----
    juce::AbstractFifo fifo { 1 << 17 };
    std::vector<float> fb[3] = { std::vector<float> (1 << 17), std::vector<float> (1 << 17), std::vector<float> (1 << 17) };
    std::atomic<bool> startReq { false }, finishReq { false };
    int recTrack = 0;

    struct Writer : juce::Thread
    {
        Writer (RigAudio& o) : juce::Thread ("take writer"), owner (o) { cur[0].reserve (1 << 22); cur[1].reserve (1 << 22); cur[2].reserve (1 << 22); }
        RigAudio& owner; std::vector<float> cur[3]; int peakCount = 0;
        void run() override
        {
            while (! threadShouldExit())
            {
                if (owner.startReq.exchange (false)) { for (auto& v : cur) v.clear(); peakCount = 0; owner.livePeakN = 0; }
                const bool fin = owner.finishReq.load();
                bool any = false;
                for (;;)
                {
                    const int ready = owner.fifo.getNumReady(); if (ready <= 0) break;
                    int s1, n1, s2, n2; owner.fifo.prepareToRead (ready, s1, n1, s2, n2);
                    for (int ch = 0; ch < 3; ++ch)
                    {
                        cur[ch].insert (cur[ch].end(), owner.fb[ch].begin() + s1, owner.fb[ch].begin() + s1 + n1);
                        if (n2 > 0) cur[ch].insert (cur[ch].end(), owner.fb[ch].begin() + s2, owner.fb[ch].begin() + s2 + n2);
                    }
                    owner.fifo.finishedRead (n1 + n2); any = true;
                }
                if (any) livePeaks();
                if (fin) { finalize(); owner.finishReq = false; }
                wait (4);
            }
        }
        void livePeaks()
        {
            const size_t full = cur[0].size() / PeakBucket;
            while ((size_t) peakCount < full && peakCount < MaxLivePeaks)
            {
                float m = 0; const size_t b = (size_t) peakCount * PeakBucket;
                for (size_t i = b; i < b + PeakBucket; ++i) m = std::max (m, std::max (std::fabs (cur[1][i]), std::fabs (cur[2][i])));
                owner.livePeaks[(size_t) peakCount] = m; ++peakCount; owner.livePeakN = peakCount;
            }
        }
        void finalize()
        {
            const size_t comp = (size_t) std::max (0, owner.compSamples.load());
            auto t = std::make_shared<TrackData>();
            if (cur[0].size() > comp + (size_t) (0.2 * owner.sr))
            {
                t->dry.assign (cur[0].begin() + (std::ptrdiff_t) comp, cur[0].end());
                t->wl.assign (cur[1].begin() + (std::ptrdiff_t) comp, cur[1].end());
                t->wr.assign (cur[2].begin() + (std::ptrdiff_t) comp, cur[2].end());
                t->makePeaks(); owner.readyTake = t; owner.takeEmpty = false;
            }
            else { owner.readyTake.reset(); owner.takeEmpty = true; }
            owner.takeReady = owner.recTrack;
            for (auto& v : cur) v.clear();
        }
    } writer;

    void handleCommands()
    {
        const int c = cmd.exchange (0);
        if (c == 0) return;
        const int st = state.load();
        if (c == 3)                                            // stop
        {
            if (st == (int) Transport::Rec) finishReq = true;
            if (st != 0) { state = 0; tpos = 0; posOut = 0; freeCount = 0; }
            return;
        }
        if (st != 0) return;                                   // already running
        beatLen = std::max<int64_t> (1, (int64_t) std::llround (60.0 / bpm.load() * sr)); beatSamplesOut = (int) beatLen;
        if (c == 1)
        {
            if (longestTrack() <= 0) return;
            tpos = 0; state = (int) Transport::Play;
        }
        else if (c == 2)
        {
            countLen = countIn.load() ? beatLen * beats.load() : 0; tpos = -countLen;
            recTrack = armed.load(); recLimit = (int64_t) (8 * 60 * sr);
            fifo.reset(); startReq = true; takeReady = -1;
            state = (int) Transport::Rec;
        }
    }

    bool audible (int i)
    {
        bool anySolo = false; for (auto& t : tracks) anySolo = anySolo || t.solo.load();
        return ! tracks[(size_t) i].mute.load() && (! anySolo || tracks[(size_t) i].solo.load());
    }

    void triggerClick (bool accent)
    {
        clickFreq = accent ? 1760.0 : 1175.0; clickAmp = clickVol.load() * (accent ? 0.9f : 0.63f);
        clickTotal = clickRemain = (int) (0.075 * sr); clickPhase = 0; beatFlash = accent ? 2 : 1;
    }

    void chunk (const float* const* in, int nIn, float* const* out, int nOut, int off, int c)
    {
        const int st = state.load(); const bool playing = st != 0;
        const int armedTrack = armed.load();
        const int ich = inChannel.load();
        if (playing)
        {
            for (int i = 0; i < NumTracks; ++i)
            {
                useTd[i] = false; td[i].reset();
                if (st == (int) Transport::Rec && i == armedTrack) continue;
                td[i] = tracks[(size_t) i].tryGet(); useTd[i] = td[i] != nullptr;
            }
        }
        float pkIn = 0;
        for (int i = 0; i < c; ++i)
        {
            float x = 0;
            if (nIn > 0)
            {
                if (ich < 0) { for (int k = 0; k < nIn; ++k) if (in[k]) x += in[k][off + i]; x /= (float) nIn; }
                else { const int k = ich < nIn ? ich : 0; if (in[k]) x = in[k][off + i]; }
            }
            dryTap[i] = x;
            float re = 0;
            if (st == (int) Transport::Play)
            {
                const int64_t tp = tpos + i;
                for (int t = 0; t < NumTracks; ++t)
                    if (useTd[t] && tracks[(size_t) t].reamp.load() && tp >= 0 && tp < td[t]->length() && audible (t)) re += td[t]->dry[(size_t) tp] * tracks[(size_t) t].vol.load();
            }
            gainSmooth += (gainTarget.load() - gainSmooth) * 0.002f;
            const float v = (x + re) * gainSmooth;
            mono[i] = v; const float a = std::fabs (v); if (a > pkIn) pkIn = a;
            tunerBuf[(tunerPos.load (std::memory_order_relaxed) + (uint32_t) i) & (TunerN - 1)] = x * gainSmooth;
        }
        tunerPos.store (tunerPos.load (std::memory_order_relaxed) + (uint32_t) c, std::memory_order_release);
        engine.process (mono, wetL, wetR, c);

        float pkOut = 0;
        for (int i = 0; i < c; ++i)
        {
            float l = wetL[i], r = wetR[i];
            const float recL = l, recR = r;
            const int64_t tp = tpos + i;
            if (playing)
            {
                if (tp >= 0)
                    for (int t = 0; t < NumTracks; ++t)
                        if (useTd[t] && ! (st == (int) Transport::Play && tracks[(size_t) t].reamp.load()) && tp < td[t]->length() && audible (t))
                        { const float g = tracks[(size_t) t].vol.load(); l += td[t]->wl[(size_t) tp] * g; r += td[t]->wr[(size_t) tp] * g; }
                // metronome
                const bool inCount = tp < 0;
                if ((metOn.load() || inCount) && (((tp % beatLen) + beatLen) % beatLen) == 0)
                { const int64_t bi = tp / beatLen, nb = beats.load(); triggerClick ((((bi % nb) + nb) % nb) == 0); }
                // recording
                if (st == (int) Transport::Rec && tp >= 0)
                {
                    int s1, n1, s2, n2; fifo.prepareToWrite (1, s1, n1, s2, n2);
                    if (n1 > 0) { fb[0][(size_t) s1] = dryTap[i]; fb[1][(size_t) s1] = recL; fb[2][(size_t) s1] = recR; fifo.finishedWrite (1); }
                    else ++overruns;
                }
            }
            else if (metOn.load())
            {
                if (freeCount % beatLen == 0)
                {
                    const int64_t nl = std::max<int64_t> (1, (int64_t) std::llround (60.0 / bpm.load() * sr));
                    if (nl != beatLen) { beatLen = nl; beatSamplesOut = (int) nl; freeCount = 0; }
                    triggerClick ((freeCount / beatLen) % beats.load() == 0);
                }
                ++freeCount;
            }
            else freeCount = 0;
            if (clickRemain > 0)
            {
                const int k = clickTotal - clickRemain; const double tt = k / sr;
                const float env = (float) (std::min (1.0, tt / 0.002) * std::exp (-tt / 0.014));
                const float cl = clickAmp * env * (float) std::sin (clickPhase); clickPhase += TAU * clickFreq / sr; --clickRemain;
                l += cl; r += cl;
            }
            if (diagOn.load() && diagN < diagLen.load())
            { diagRaw[(size_t) diagN] = dryTap[i]; diagWet[(size_t) diagN] = recL; if (++diagN >= diagLen.load()) { diagOn = false; diagDone = true; } }
            if (muteOut.load (std::memory_order_relaxed)) { l = 0; r = 0; }
            const float a = std::max (std::fabs (l), std::fabs (r)); if (a > pkOut) pkOut = a;
            for (int ch = 0; ch < nOut; ++ch)
            {
                if (out[ch] == nullptr) continue;
                out[ch][off + i] = nOut == 1 ? 0.5f * (l + r) : (ch == 0 ? l : (ch == 1 ? r : 0.f));
            }
        }
        if (playing)
        {
            tpos += c; posOut = tpos;
            if (st == (int) Transport::Play && tpos > longestTrack() + (int64_t) (0.35 * sr)) { state = 0; tpos = 0; posOut = 0; }
            else if (st == (int) Transport::Rec && tpos > recLimit) { finishReq = true; state = 0; tpos = 0; posOut = 0; }
        }
        peakIn = std::max (pkIn, peakIn.load() * 0.9f); peakOut = std::max (pkOut, peakOut.load() * 0.9f);
    }
};
} // namespace alt
