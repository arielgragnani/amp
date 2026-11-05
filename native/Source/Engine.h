// Alt Guitar Amp Rack - native audio engine.
// A line-by-line port of the browser engine (rig.js): input filter, mains-hum canceller, noise gate, the "Alt Clean"
// blackface-style amp (4x oversampled), speaker cabinet, spring reverb, output volume and soft limiter.
// Plain C++17, no framework dependencies, so it can be tested on its own.
#pragma once
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <vector>

namespace alt
{
constexpr double TAU = 6.283185307179586476925286766559;
inline double clampd (double v, double a, double b) { return v < a ? a : (v > b ? b : v); }
inline double lerpd (double a, double b, double t) { return a + (b - a) * t; }
inline double dbl (double x) { return std::pow (10.0, x / 20.0); }
inline int pow2i (int n) { int p = 1; while (p < n) p <<= 1; return p; }
inline double ft (double x) { if (x >= 3) return 1; if (x <= -3) return -1; const double q = x * x; return x * (27 + q) / (27 + 9 * q); }
inline void flushv (double& v) { if (v < 1e-25 && v > -1e-25) v = 0; }
template <size_t N> inline void flushArr (double (&a)[N]) { for (size_t i = 0; i < N; ++i) flushv (a[i]); }
inline void flushVec (std::vector<double>& a) { for (auto& v : a) flushv (v); }

enum class BqType { LP, HP, Peak, LowShelf, HighShelf };

// RBJ biquad, transposed direct form II
struct Bq
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void set (BqType type, double f, double q, double g, double sr)
    {
        f = clampd (f, 10, sr * 0.45);
        const double w = TAU * f / sr, c = std::cos (w), s = std::sin (w), al = s / (2 * q), A = std::pow (10.0, g / 40);
        double B0, B1, B2, A0, A1, A2;
        if (type == BqType::LP) { B0 = (1 - c) / 2; B1 = 1 - c; B2 = B0; A0 = 1 + al; A1 = -2 * c; A2 = 1 - al; }
        else if (type == BqType::HP) { B0 = (1 + c) / 2; B1 = -(1 + c); B2 = B0; A0 = 1 + al; A1 = -2 * c; A2 = 1 - al; }
        else if (type == BqType::Peak) { B0 = 1 + al * A; B1 = -2 * c; B2 = 1 - al * A; A0 = 1 + al / A; A1 = -2 * c; A2 = 1 - al / A; }
        else if (type == BqType::LowShelf)
        {
            const double sq = 2 * std::sqrt (A) * al;
            B0 = A * ((A + 1) - (A - 1) * c + sq); B1 = 2 * A * ((A - 1) - (A + 1) * c); B2 = A * ((A + 1) - (A - 1) * c - sq);
            A0 = (A + 1) + (A - 1) * c + sq; A1 = -2 * ((A - 1) + (A + 1) * c); A2 = (A + 1) + (A - 1) * c - sq;
        }
        else
        {
            const double sq = 2 * std::sqrt (A) * al;
            B0 = A * ((A + 1) + (A - 1) * c + sq); B1 = -2 * A * ((A - 1) + (A + 1) * c); B2 = A * ((A + 1) + (A - 1) * c - sq);
            A0 = (A + 1) - (A - 1) * c + sq; A1 = 2 * ((A - 1) - (A + 1) * c); A2 = (A + 1) - (A - 1) * c - sq;
        }
        b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
    }
    void flush() { flushv (z1); flushv (z2); }
    inline double p (double x) { const double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
};

// Halfband FIR (Blackman) shared by the 2x up/down stages
struct HB
{
    static constexpr int M = 8;
    double h[2 * M], g[2 * M], cc, c2;
    HB()
    {
        const int L = 4 * M + 1, c = 2 * M; double hh[2 * M], sum = 0.5;
        for (int i = 0; i < 2 * M; ++i)
        {
            const int k = 2 * i + 1; const double t = (k - c) / 2.0;
            const double w = 0.42 - 0.5 * std::cos (TAU * k / (L - 1)) + 0.08 * std::cos (2 * TAU * k / (L - 1));
            hh[i] = 0.5 * std::sin (M_PI * t) / (M_PI * t) * w; sum += hh[i];
        }
        cc = 0.5 / sum;
        for (int i = 0; i < 2 * M; ++i) { h[i] = hh[i] / sum; g[i] = 2 * hh[i] / sum; }
        c2 = 2 * cc;
    }
};
struct Up2
{
    const HB* hb; double buf[32] = {}; int pos = 0;
    void run (double x, double* out, int i)
    {
        const int M = HB::M; const int p = (pos + 1) & 31; pos = p; buf[p] = x;
        out[i] = buf[(p - M) & 31] * hb->c2;
        double s = 0; for (int k = 0; k < 2 * M; ++k) s += hb->g[k] * buf[(p - k) & 31];
        out[i + 1] = s;
    }
};
struct Down2
{
    const HB* hb; double bE[32] = {}, bO[32] = {}; int pE = 0, pO = 0;
    double run (double e, double o)
    {
        const int M = HB::M; const int pe = (pE + 1) & 31; pE = pe; bE[pe] = e;
        double s = hb->cc * bE[(pe - M) & 31]; const int po = pO;
        for (int k = 0; k < 2 * M; ++k) s += hb->h[k] * bO[(po - k) & 31];
        pO = (po + 1) & 31; bO[pO] = o; return s;
    }
};
struct Os4
{
    Up2 u1, u2; Down2 d1, d2;
    void init (const HB* hb) { u1.hb = u2.hb = d1.hb = d2.hb = hb; }
    void flush() { flushArr (u1.buf); flushArr (u2.buf); flushArr (d1.bE); flushArr (d1.bO); flushArr (d2.bE); flushArr (d2.bO); }
    void up (const double* in, int n, double* out)
    {
        double t[2];
        for (int m = 0; m < n; ++m) { u1.run (in[m], t, 0); u2.run (t[0], out, 4 * m); u2.run (t[1], out, 4 * m + 2); }
    }
    void down (const double* w, int n, double* out)
    {
        for (int m = 0; m < n; ++m)
        {
            const double y0 = d2.run (w[4 * m], w[4 * m + 1]), y1 = d2.run (w[4 * m + 2], w[4 * m + 3]);
            out[m] = d1.run (y0, y1);
        }
    }
};

// ---------- Alt Clean voicing ----------
namespace FEN
{
constexpr double h1 = 5, h2 = 3.5, hp = 3.2;      // headroom of each stage
constexpr double g1 = 7, g2 = 20;                 // preamp stage gains (dB)
constexpr double b1 = 0.035, b2 = 0.05, bp = 0.012; // asymmetry
constexpr double R1 = 250e3, R2 = 250e3, R3 = 10e3, R4 = 100e3, C1 = 250e-12, C2 = 100e-9, C3 = 47e-9; // blackface tone stack
constexpr double tsMk = 12;
constexpr double rv = 1e6, bc = 47e-12;
constexpr double sag = 0.55;
constexpr double lc[5] = { -9.9, -9.9, -16.31, -21.61, -25.94 };
}

// Fender tone stack (Yeh & Smith): analog coefficients, then bilinear transform at rate fs -> o[0..7] = b0..b3, a0..a3; o[8..13] analog
inline void tsDesign (double t, double m, double lin, double fs, double* o)
{
    using namespace FEN;
    const double l = std::exp ((lin - 1) * 3.4);
    const double b1_ = t * C1 * R1 + m * C3 * R3 + l * (C1 * R2 + C2 * R2) + (C1 * R3 + C2 * R3);
    const double b2_ = t * (C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4) - m * m * (C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3) + m * (C1 * C3 * R1 * R3 + C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3) + l * (C1 * C2 * R1 * R2 + C1 * C2 * R2 * R4 + C1 * C3 * R2 * R4) + l * m * (C1 * C3 * R2 * R3 + C2 * C3 * R2 * R3) + (C1 * C2 * R1 * R3 + C1 * C2 * R3 * R4 + C1 * C3 * R3 * R4);
    const double b3_ = l * m * (C1 * C2 * C3 * R1 * R2 * R3 + C1 * C2 * C3 * R2 * R3 * R4) - m * m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4) + m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4) + t * C1 * C2 * C3 * R1 * R3 * R4 - t * m * C1 * C2 * C3 * R1 * R3 * R4 + t * l * C1 * C2 * C3 * R1 * R2 * R4;
    const double a1_ = (C1 * R1 + C1 * R3 + C2 * R3 + C2 * R4 + C3 * R4) + m * C3 * R3 + l * (C1 * R2 + C2 * R2);
    const double a2_ = m * (C1 * C3 * R1 * R3 - C2 * C3 * R3 * R4 + C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3) + l * m * (C1 * C3 * R2 * R3 + C2 * C3 * R2 * R3) - m * m * (C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3) + l * (C1 * C2 * R2 * R4 + C1 * C2 * R1 * R2 + C1 * C3 * R2 * R4 + C2 * C3 * R2 * R4) + (C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4 + C1 * C2 * R3 * R4 + C1 * C2 * R1 * R3 + C1 * C3 * R3 * R4 + C2 * C3 * R3 * R4);
    const double a3_ = l * m * (C1 * C2 * C3 * R1 * R2 * R3 + C1 * C2 * C3 * R2 * R3 * R4) - m * m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4) + m * (C1 * C2 * C3 * R3 * R3 * R4 + C1 * C2 * C3 * R1 * R3 * R3 - C1 * C2 * C3 * R1 * R3 * R4) + l * C1 * C2 * C3 * R1 * R2 * R4 + C1 * C2 * C3 * R1 * R3 * R4;
    const double c = 2 * fs, c2 = c * c, c3 = c2 * c;
    const double B0 = -b1_ * c - b2_ * c2 - b3_ * c3, B1 = -b1_ * c + b2_ * c2 + 3 * b3_ * c3, B2 = b1_ * c + b2_ * c2 - 3 * b3_ * c3, B3 = b1_ * c - b2_ * c2 + b3_ * c3;
    const double A0 = -1 - a1_ * c - a2_ * c2 - a3_ * c3, A1 = -3 - a1_ * c + a2_ * c2 + 3 * a3_ * c3, A2 = -3 + a1_ * c + a2_ * c2 - 3 * a3_ * c3, A3 = -1 + a1_ * c - a2_ * c2 + a3_ * c3;
    const double r = 1 / A0;
    o[0] = B0 * r; o[1] = B1 * r; o[2] = B2 * r; o[3] = B3 * r; o[4] = 1; o[5] = A1 * r; o[6] = A2 * r; o[7] = A3 * r;
    o[8] = b1_; o[9] = b2_; o[10] = b3_; o[11] = a1_; o[12] = a2_; o[13] = a3_;
}
inline double tsPow (const double* o)
{
    static const double TSF[11] = { 110, 160, 220, 330, 440, 660, 880, 1300, 1800, 2600, 3500 };
    double sum = 0;
    for (int j = 0; j < 11; ++j)
    {
        const double w = TAU * TSF[j], w2 = w * w, w3 = w2 * w;
        const double nr = -w2 * o[9], ni = w * o[8] - w3 * o[10], dr = 1 - w2 * o[12], di = w * o[11] - w3 * o[13];
        sum += (nr * nr + ni * ni) / (dr * dr + di * di);
    }
    return sum / 11;
}

// Spring tank: delay line + chain of first-order allpass filters (dispersion) + damping, in a feedback loop
struct Spring
{
    int D = 1, K = 64, mask = 0, w = 0; double a = -0.7, g = 0.9, lp = 0, dk = 0.3; std::vector<double> buf, z;
    void init (double sr, double sec, int K_, double a_, double g_, double fc)
    {
        D = (int) std::lround (sec * sr); buf.assign ((size_t) pow2i (D + 8), 0.0); mask = (int) buf.size() - 1; w = 0;
        K = K_; a = a_; z.assign ((size_t) K, 0.0); g = g_; lp = 0; dk = 1 - std::exp (-TAU * fc / sr);
    }
    inline double p (double x)
    {
        double v = buf[(size_t) ((w - D) & mask)];
        for (int k = 0; k < K; ++k) { const double y = a * v + z[(size_t) k]; z[(size_t) k] = v - a * y; v = y; }
        lp += (v - lp) * dk;
        buf[(size_t) w] = x + g * lp; w = (w + 1) & mask;
        return lp;
    }
    void flush() { flushVec (z); flushv (lp); }
};

inline double softLimit (double x)
{
    const double a = x < 0 ? -x : x;
    if (a < 0.75) return x;
    const double y = 0.75 + 0.25 * std::tanh ((a - 0.75) / 0.25);
    return x < 0 ? -y : y;
}

// ---------- parameters ----------
struct Params
{
    // amp (knobs 0..10)
    double vol = 4, treb = 5, mid = 5, bass = 4, rev = 2.5, trs = 5, tri = 0;
    bool bright = true;
    // cabinet: 0 = 2x12 open back, 1 = 1x12 open back
    int cab = 0; double mic = 5; bool cabOn = true;
    // noise control
    bool humOn = true; int humHz = 60; double humAmt = 10;
    bool gateOn = true; double gateThr = -58;
    // output (knob 0..10)
    double outVol = 6;
};

struct CabVoicing { double hp, rf, rg, pf, pk, df, dg, lp; };
static const CabVoicing CABS[2] = { { 70, 100, 2.0, 3000, 3.5, 6800, -2.0, 8200 }, { 80, 115, 2.5, 2600, 3.0, 5800, -2.5, 7400 } };

class Engine
{
public:
    // outputs the interface can poll
    std::atomic<int> gateOpenFlag { 1 };
    std::atomic<float> humLevelDb { -140.f }, humHzOut { 60.f };
    std::atomic<int> learnDone { 0 };            // increments each time a learn finishes
    std::atomic<int> learnOk { 0 }, learnMains { 0 };
    std::atomic<float> learnThr { -58.f }, learnLevel { -90.f };
    std::atomic<int> learning { 0 };

    void prepare (double sampleRate)
    {
        sr = sampleRate; s4 = sr * 4;
        hb = HB(); osD = Os4(); osD.init (&hb); osA = Os4(); osA.init (&hb);
        inHp.set (BqType::HP, 68, 0.5412, 0, sr); inHp2.set (BqType::HP, 68, 1.3066, 0, sr); detHp.set (BqType::HP, 120, 0.707, 0, sr);
        std::fill (std::begin (hc), std::end (hc), 1.0); std::fill (std::begin (hs), std::end (hs), 0.0);
        std::fill (std::begin (ha), std::end (ha), 0.0); std::fill (std::begin (hw), std::end (hw), 0.0);
        std::fill (std::begin (hthPrev), std::end (hthPrev), 0.0);
        hPe = 0; hAge = 0; fllN = 0; hLvl = 0; hSent = -999; hCount = 0; humNomSet = 0;
        setHumFreq (60);
        gEnv = 0; gOpen = false; gHold = 0; gG = 1; gSentv = -1; learn = 0; lMax = 0; lLen = 0;
        gRel = std::exp (-1 / (0.03 * sr)); gHoldN = (int) std::lround (0.06 * sr);
        aOpen = 1 - std::exp (-1 / (0.00025 * sr)); aClose = 1 - std::exp (-1 / (0.07 * sr));
        std::fill (std::begin (mc), std::end (mc), 1.0);
        sagEnv = 0; sagKa = 1 - std::exp (-1 / (0.02 * sr * 4)); sagKr = 1 - std::exp (-1 / (0.16 * sr * 4));
        trPh = 0;
        spA.init (sr, 0.0343, 64, -0.74, 0.92, 3300); spB.init (sr, 0.0419, 64, -0.70, 0.92, 3000);
        spAct = 0; spLo = 0; spHk = 1 - std::exp (-TAU * 190 / sr); spTail = 0;
        { double o[14]; tsDesign (0.6, 0.4, 0.4, s4, o); tsRef = tsPow (o); }
        std::fill (std::begin (tsZ), std::end (tsZ), 0.0); brZ = 0;
        aHp[0] = aHp[1] = aLp[0] = aLp[1] = tP = tPh = Bq();
        cHp = cRes = cPk = cDip = cL1 = cL2 = Bq();
        fade = fadeOld = 1; haveP = false; cabIdx = -1;
        std::fill (cur, cur + NP, 0.0); std::fill (old, old + NP, 0.0); std::fill (tgt, tgt + NP, 0.0);
    }

    void setParams (const Params& P)
    {
        derive (P);
        if (! haveP) { std::copy (tgt, tgt + NP, cur); std::copy (tgt, tgt + NP, old); coefs(); fade = fadeOld = 1; cabIdx = P.cab; haveP = true; }
        else if (P.cab != cabIdx) { cabIdx = P.cab; fade = 0; fadeOld = 0; }
    }

    // Start a 2-second noise-floor measurement (keep the strings muted). Result appears in learn* atomics.
    void startLearn() { lLen = (int) std::lround (2.0 * sr); learn = lLen; lMax = 0; learning = 1; }

    // mono in -> stereo out. Any block size; processed internally in chunks of up to 128 samples
    void process (const float* in, float* outL, float* outR, int n)
    {
        int pos = 0;
        while (pos < n) { const int c = std::min (128, n - pos); processBlock (in + pos, outL + pos, outR + pos, c); pos += c; }
    }

    double sampleRate() const { return sr; }

private:
    enum { GateThr, GateOn, AT, AM, AL, AV, ABr, AG1, AG2, APg, ATrim, ATrR, ATrD, SpMix, ASag, CabOn, CabHP, CabRF, CabRG, CabPF, CabPG, CabDF, CabDG, CabLP, Vol, HumAmt, NP };
    double sr = 48000, s4 = 192000;
    HB hb; Os4 osD, osA;
    double tgt[NP], cur[NP], old[NP]; bool haveP = false; int cabIdx = -1; double fade = 1, fadeOld = 1;
    double humNom = 60, humF0 = 60; int humNomSet = 0;
    // input filter + hum canceller
    Bq inHp, inHp2, detHp;
    static constexpr int HK = 12;
    double hc[HK], hs[HK], ha[HK], hw[HK], hrc[HK], hrs[HK], hthPrev[HK];
    double hPe = 0, hLvl = 0, hSent = -999; const double hRefInv = 1 / 1.6e-5; long long hAge = 0; const double humTau = 0.3; int fllN = 0, hCount = 0;
    // gate + learn
    double gEnv = 0, gG = 1, gRel = 0, aOpen = 0, aClose = 0, lMax = 0; bool gOpen = false; int gHold = 0, gHoldN = 0, gSentv = -1, learn = 0, lLen = 0;
    double mc[8], ms[8] = {}, mrc[8] = {}, mrs[8] = {}, mxr[8] = {}, mxi[8] = {};
    // amp
    Bq aHp[2], aLp[2], tP, tPh; double tsC[14] = {}, tsZ[3] = {}, brZ = 0, brC[3] = {}, tsRef = 1, tsCmp = 1;
    double sagEnv = 0, sagKa = 0, sagKr = 0, trPh = 0;
    // spring
    Spring spA, spB; double spAct = 0, spLo = 0, spHk = 0, spTail = 0;
    // cab
    Bq cHp, cRes, cPk, cDip, cL1, cL2;
    // work buffers
    double b1[128], u[128 * 4], tg[128];

    void setHumFreq (double f0)
    {
        humF0 = f0;
        for (int k = 0; k < HK; ++k) { const double w = TAU * (k + 1) * f0 / sr; hrc[k] = std::cos (w); hrs[k] = std::sin (w); }
    }
    void startMains()
    {
        static const double f[8] = { 50, 100, 150, 200, 60, 120, 180, 240 };
        for (int j = 0; j < 8; ++j) { mc[j] = 1; ms[j] = 0; mrc[j] = std::cos (TAU * f[j] / sr); mrs[j] = std::sin (TAU * f[j] / sr); mxr[j] = 0; mxi[j] = 0; }
    }
    void derive (const Params& P)
    {
        double* t = tgt;
        t[GateThr] = P.gateThr; t[GateOn] = P.gateOn ? 1 : 0;
        const double vol = clampd (P.vol, 0, 10);
        t[AT] = clampd (P.treb, 0, 10) / 10; t[AM] = clampd (P.mid, 0, 10) / 10; t[AL] = clampd (P.bass, 0, 10) / 10;
        t[AV] = vol <= 0 ? 0 : std::pow (10.0, ((vol - 10) * 3.0) / 20) * std::pow (std::min (1.0, vol / 1.5), 2.0);
        t[ABr] = P.bright ? 1 : 0;
        t[AG1] = dbl (FEN::g1); t[AG2] = dbl (FEN::g2); t[APg] = 1; t[ASag] = FEN::sag;
        t[ATrR] = 0.8 * std::pow (15.0, clampd (P.trs, 0, 10) / 10); t[ATrD] = P.tri <= 0 ? 0 : 0.92 * std::pow (clampd (P.tri, 0, 10) / 10, 1.1);
        t[SpMix] = P.rev <= 0 ? 0 : std::pow (clampd (P.rev, 0, 10) / 10, 1.5);
        { const double pos = vol / 2.5; const int i0 = std::min (3, (int) std::floor (pos)); const double fr = pos - i0; t[ATrim] = dbl (lerpd (FEN::lc[i0], FEN::lc[i0 + 1], fr)); }
        const CabVoicing& c = CABS[P.cab == 1 ? 1 : 0]; const double mic = clampd (P.mic, 0, 10) / 10;
        t[CabOn] = P.cabOn ? 1 : 0; t[CabHP] = c.hp; t[CabRF] = c.rf; t[CabRG] = c.rg;
        t[CabPF] = c.pf; t[CabPG] = c.pk + (mic - 0.5) * 4; t[CabDF] = c.df; t[CabDG] = c.dg; t[CabLP] = c.lp * (0.78 + 0.44 * mic);
        t[Vol] = std::pow (clampd (P.outVol, 0, 10) / 10, 1.5) / std::pow (0.6, 1.5);
        t[HumAmt] = P.humOn ? clampd (P.humAmt, 0, 10) / 10 : 0;
        humNom = P.humHz == 50 ? 50 : 60;
    }
    void coefs()
    {
        const double* c = cur;
        aHp[0].set (BqType::HP, 28, 0.707, 0, s4); aLp[0].set (BqType::LP, 16000, 0.707, 0, s4);
        aHp[1].set (BqType::HP, 24, 0.707, 0, s4); aLp[1].set (BqType::LP, 15000, 0.707, 0, s4);
        tsDesign (c[AT], c[AM], c[AL], s4, tsC);
        tsCmp = std::pow (tsRef / tsPow (tsC), 0.25);
        { const double v = c[AV], tau = (1 - v) * FEN::rv * FEN::bc * c[ABr], cc = 2 * s4, d0 = 1 + v * cc * tau;
          brC[0] = v * (1 + cc * tau) / d0; brC[1] = v * (1 - cc * tau) / d0; brC[2] = (1 - v * cc * tau) / d0; }
        tP.set (BqType::LP, 11000, 0.707, 0, s4); tPh.set (BqType::HP, 38, 0.707, 0, s4);
        cHp.set (BqType::HP, c[CabHP], 0.707, 0, sr); cRes.set (BqType::Peak, c[CabRF], 1, c[CabRG], sr);
        cPk.set (BqType::Peak, c[CabPF], 0.9, c[CabPG], sr); cDip.set (BqType::Peak, c[CabDF], 1.4, c[CabDG], sr);
        cL1.set (BqType::LP, c[CabLP], 0.5412, 0, sr); cL2.set (BqType::LP, c[CabLP], 1.3066, 0, sr);
    }
    void flushAll()
    {
        inHp.flush(); inHp2.flush(); detHp.flush();
        for (int i = 0; i < 2; ++i) { aHp[i].flush(); aLp[i].flush(); }
        tP.flush(); tPh.flush(); flushArr (tsZ); flushv (brZ); spA.flush(); spB.flush();
        flushv (sagEnv); flushv (spLo);
        cHp.flush(); cRes.flush(); cPk.flush(); cDip.flush(); cL1.flush(); cL2.flush();
        osA.flush(); osD.flush(); flushv (gEnv);
        flushArr (ha); flushArr (hw);
        for (int k = 0; k < HK; ++k) { const double m = 1 / std::hypot (hc[k], hs[k]); hc[k] *= m; hs[k] *= m; }
    }

    void processBlock (const float* x, float* oL, float* oR, int n)
    {
        const double inv = 1.0 / n;
        // smooth parameters once per block
        const double k = 1 - std::exp (-n / (0.02 * sr)); bool changed = false;
        for (int i = 0; i < NP; ++i)
        {
            old[i] = cur[i]; const double dd = tgt[i] - cur[i];
            if (dd != 0) { cur[i] += std::fabs (dd) < 1e-7 ? dd : dd * k; changed = true; }
        }
        if (changed) coefs();
        flushAll();
        fadeOld = fade; fade = std::min (1.0, fade + n / (0.03 * sr));

        // hum canceller housekeeping
        if ((int) humNom != humNomSet) { humNomSet = (int) humNom; setHumFreq (humNom); std::fill (std::begin (ha), std::end (ha), 0.0); std::fill (std::begin (hw), std::end (hw), 0.0); hAge = 0; }
        fllN += n;
        if (fllN >= 0.1 * sr)
        {
            const double dt = fllN / sr; fllN = 0; double num = 0, den = 0;
            for (int kk = 0; kk < 6; ++kk)
            {
                const double th = std::atan2 (-hw[kk], ha[kk]), pw = ha[kk] * ha[kk] + hw[kk] * hw[kk];
                double dth = th - hthPrev[kk]; hthPrev[kk] = th;
                if (dth > M_PI) dth -= TAU; else if (dth < -M_PI) dth += TAU;
                num += pw * dth / (TAU * dt * (kk + 1)); den += pw;
            }
            if (den > 1e-11 && hPe < 3 / hRefInv) setHumFreq (clampd (humF0 + 0.9 * num / den, humNom - 0.5, humNom + 0.5));
        }
        if (++hCount >= 12)
        {
            hCount = 0; const double dbv = 10 * std::log10 (hLvl + 1e-14);
            if (std::fabs (dbv - hSent) >= 1.5) { hSent = dbv; humLevelDb = (float) dbv; humHzOut = (float) (std::round (humF0 * 100) / 100); }
        }

        // A: input highpass, hum canceller, gate
        const double thr = dbl (cur[GateThr]), gOn0 = old[GateOn], gOn1 = cur[GateOn], closeThr = thr * 0.63;
        const double humMu = 2 / ((hAge < 2 * sr || learn > 0 ? 0.1 : humTau) * sr), hmA0 = old[HumAmt], hmA1 = cur[HumAmt];
        const long long learnSkip = std::llround (0.8 * sr);
        for (int i = 0; i < n; ++i)
        {
            const double t = (i + 1) * inv, xin = x[i];
            double s = inHp2.p (inHp.p (xin + 1e-18));
            {
                double h = 0;
                for (int kk = 0; kk < HK; ++kk) h += ha[kk] * hc[kk] + hw[kk] * hs[kk];
                const double e = s - h;
                hPe += (e * e - hPe) * 0.0004;
                const double boost = hAge < sr ? 4 : (learn > 0 ? 2 : 1);
                const double mu = humMu * boost / (1 + hPe * hRefInv), me = mu * e;
                for (int kk = 0; kk < HK; ++kk)
                {
                    const double c = hc[kk], q = hs[kk];
                    ha[kk] += me * c; hw[kk] += me * q;
                    hc[kk] = c * hrc[kk] - q * hrs[kk]; hs[kk] = q * hrc[kk] + c * hrs[kk];
                }
                hLvl += (h * h - hLvl) * 0.0002;
                s -= (hmA0 + (hmA1 - hmA0) * t) * h;
            }
            ++hAge;
            const double hpd = detHp.p (s), a = hpd < 0 ? -hpd : hpd;
            gEnv = a > gEnv ? a : gEnv * gRel;
            if (learn > 0)
            {
                const long long el = lLen - learn;
                if (el == learnSkip) startMains();
                if (el >= learnSkip)
                {
                    if (gEnv > lMax) lMax = gEnv;
                    for (int j = 0; j < 8; ++j)
                    {
                        const double c = mc[j], q = ms[j];
                        mxr[j] += xin * c; mxi[j] += xin * q;
                        mc[j] = c * mrc[j] - q * mrs[j]; ms[j] = q * mrc[j] + c * mrs[j];
                    }
                }
                if (--learn == 0)
                {
                    const double lv = 20 * std::log10 (lMax + 1e-9); double e50 = 0, e60 = 0;
                    for (int j = 0; j < 4; ++j) { e50 += mxr[j] * mxr[j] + mxi[j] * mxi[j]; e60 += mxr[j + 4] * mxr[j + 4] + mxi[j + 4] * mxi[j + 4]; }
                    const int mains = e50 > 2 * e60 ? 50 : (e60 > 2 * e50 ? 60 : 0);
                    learnLevel = (float) lv; learnMains = mains;
                    if (lv > -26) learnOk = 0; else { learnOk = 1; learnThr = (float) std::max (-80.0, lv + 3); }
                    learning = 0; ++learnDone;
                }
                gOpen = true; gHold = gHoldN;
            }
            else if (! gOpen) { if (gEnv > thr) { gOpen = true; gHold = gHoldN; } }
            else if (gEnv < closeThr) { if (--gHold <= 0) gOpen = false; }
            else gHold = gHoldN;
            gG += ((gOpen ? 1 : 0.00005) - gG) * (gOpen ? aOpen : aClose);
            const double ge = gOn0 + (gOn1 - gOn0) * t;
            s *= 1 - ge + ge * gG;
            b1[i] = s;
        }
        const int gs = gOpen ? 1 : 0;
        if (gs != gSentv) { gSentv = gs; gateOpenFlag = gs; }

        // C: amp at 4x
        {
            const int n4 = 4 * n;
            const double td0 = old[ATrD], td1 = cur[ATrD]; const bool trem = td0 > 1e-4 || td1 > 1e-4;
            if (trem)
            {
                const double inc = cur[ATrR] / sr;
                for (int i = 0; i < n; ++i)
                {
                    trPh += inc; if (trPh >= 1) trPh -= 1;
                    tg[i] = 1 - (td0 + (td1 - td0) * (i + 1) * inv) * 0.5 * (1 + std::sin (TAU * trPh));
                }
            }
            osA.up (b1, n, u);
            const double g1 = cur[AG1] / FEN::h1, g2 = cur[AG2] / FEN::h2;
            stage (aHp[0], aLp[0], n4, g1, FEN::b1, FEN::h1);
            // tone stack
            { const double b0 = tsC[0], bb1 = tsC[1], bb2 = tsC[2], bb3 = tsC[3], a1 = tsC[5], a2 = tsC[6], a3 = tsC[7], mk = dbl (FEN::tsMk) * tsCmp;
              double z1 = tsZ[0], z2 = tsZ[1], z3 = tsZ[2];
              for (int i = 0; i < n4; ++i) { const double xx = u[i], y = b0 * xx + z1; z1 = bb1 * xx - a1 * y + z2; z2 = bb2 * xx - a2 * y + z3; z3 = bb3 * xx - a3 * y; u[i] = y * mk; }
              tsZ[0] = z1; tsZ[1] = z2; tsZ[2] = z3; }
            // volume pot with bright cap
            { const double b0 = brC[0], bb1 = brC[1], a1 = brC[2]; double z = brZ;
              for (int i = 0; i < n4; ++i) { const double xx = u[i], y = b0 * xx + z; z = bb1 * xx - a1 * y; u[i] = y; }
              brZ = z; }
            stage (aHp[1], aLp[1], n4, g2, FEN::b2, FEN::h2);
            // power stage with supply sag
            { const double H = FEN::hp, bp = FEN::bp, tbp = ft (bp), gp = cur[APg], trim = cur[ATrim], sagK = cur[ASag]; double env = sagEnv;
              for (int i = 0; i < n4; ++i)
              {
                  double xx = u[i] * gp; if (trem) xx *= tg[i >> 2];
                  const double Hs = H / (1 + sagK * env), y = Hs * (ft (xx / Hs + bp) - tbp);
                  const double a = (y < 0 ? -y : y) / H; env += (a - env) * (a > env ? sagKa : sagKr);
                  u[i] = y * trim;
              }
              sagEnv = env; }
            for (int i = 0; i < n4; ++i) u[i] = tP.p (tPh.p (u[i]));
            osA.down (u, n, b1);
        }

        // D: cabinet
        const double cb0 = old[CabOn], cb1 = cur[CabOn];
        if (cb0 > 1e-5 || cb1 > 1e-5)
            for (int i = 0; i < n; ++i)
            {
                const double v = b1[i], w = cL2.p (cL1.p (cDip.p (cPk.p (cRes.p (cHp.p (v))))));
                const double e = cb0 + (cb1 - cb0) * (i + 1) * inv;
                b1[i] = v * (1 - e) + w * 1.6 * e;
            }

        // E: spring reverb, volume, limiter
        const double v0 = old[Vol], v1 = cur[Vol], sp0 = old[SpMix], sp1 = cur[SpMix];
        for (int i = 0; i < n; ++i)
        {
            const double t = (i + 1) * inv, s = b1[i]; double l = s, r = s;
            const double spm = sp0 + (sp1 - sp0) * t;
            { const double aa = s < 0 ? -s : s; spAct = aa > spAct ? aa : spAct * 0.9997; }
            if (spm > 1e-4 && (spAct > 1e-6 || spTail > 3e-6))
            {
                spLo += (s - spLo) * spHk;
                const double xi = ft ((s - spLo) * 2) * 0.5 + 1e-18;
                const double wa = spA.p (xi), wb = spB.p (xi);
                l += (wa + 0.35 * wb) * spm * 1.3; r += (wb + 0.35 * wa) * spm * 1.3;
                const double ta = wa < 0 ? -wa : wa; spTail = ta > spTail ? ta : spTail * 0.9995;
            }
            const double vol = (v0 + (v1 - v0) * t) * (fadeOld + (fade - fadeOld) * t);
            oL[i] = (float) softLimit (l * vol); oR[i] = (float) softLimit (r * vol);
        }
    }

    // one preamp tube stage at 4x: coupling highpass, gain into the saturator, Miller lowpass
    void stage (Bq& hp, Bq& lp, int n4, double g, double bias, double at)
    {
        const double h0 = hp.b0, h1 = hp.b1, h2 = hp.b2, ha1 = hp.a1, ha2 = hp.a2; double hz1 = hp.z1, hz2 = hp.z2;
        const double l0 = lp.b0, l1 = lp.b1, l2 = lp.b2, la1 = lp.a1, la2 = lp.a2; double lz1 = lp.z1, lz2 = lp.z2;
        const double tb = ft (bias);
        for (int i = 0; i < n4; ++i)
        {
            const double v = u[i];
            double y = h0 * v + hz1; hz1 = h1 * v - ha1 * y + hz2; hz2 = h2 * v - ha2 * y;
            y *= g; y = ft (y + bias) - tb;
            const double w = l0 * y + lz1; lz1 = l1 * y - la1 * w + lz2; lz2 = l2 * y - la2 * w;
            u[i] = w * at;
        }
        hp.z1 = hz1; hp.z2 = hz2; lp.z1 = lz1; lp.z2 = lz2;
    }
};
} // namespace alt
