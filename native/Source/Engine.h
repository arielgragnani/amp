// Alt Guitar Amp Rack - native audio engine.
// A line-by-line port of the browser engine (rig.js): input filter, mains-hum canceller, noise gate, the "Alt Clean"
// blackface-style amp (4x oversampled), speaker cabinet, spring reverb, output volume and soft limiter.
// Plain C++17, no framework dependencies, so it can be tested on its own.
#pragma once
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
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

// One amp circuit: stage headroom/gain, tone stack values, bright cap, power section. Model 0 is the original calibrated Fender-style voicing.
struct AmpModel
{
    double h1, h2, hp, g1, g2, b1, b2, bp;        // stage headroom, preamp gains (dB), asymmetry
    double R1, R2, R3, R4, C1, C2, C3;            // tone stack: treble pot, bass pot, mid pot, slope resistor, treble cap, bass cap, mid cap
    double rv, bc, sag;                           // volume pot, bright cap, supply sag
    double lc[5];                                 // output trim (dB) at volume 0, 2.5, 5, 7.5, 10
    double hp1, lp1, hp2, lp2, tpLP;              // interstage corners (Hz) and power stage low-pass
    double midPot;                                // 1 = a middle control exists
};
static AmpModel MODELS[4] = {
    // 0: calibrated Fender crunch / Twin / jazz voicings (values as before)
    { 5, 3.5, 3.2, 7, 20, 0.035, 0.05, 0.012,   250e3, 250e3, 10e3, 100e3, 250e-12, 100e-9, 47e-9,   1e6, 47e-12, 0.55,   { -9.9, -9.9, -16.31, -21.61, -25.94 },   28, 16000, 24, 15000, 11000, 1 },
    // 1: Blues Junior: service diagram values (250k/250k/25k, 100k slope, 250 pF, 22 nF, 22 nF), 15 W EL84 pair, fixed bias
    { 5, 3.5, 2.4, 8, 22, 0.035, 0.05, 0.02,    250e3, 250e3, 25e3, 100e3, 250e-12, 22e-9, 22e-9,    1e6, 100e-12, 0.85,  { -12.43, -12.43, -18.44, -22.79, -25.87 },   28, 16000, 24, 15000, 11000, 1 },
    // 2: Lead Stack: JCM800 2203-style (220k/1M/25k, 33k slope, 470 pF, 22 nF, 22 nF), two cascaded gain stages, 100 W EL34 with little sag
    { 3.5, 2.5, 4.5, 20, 24, 0.04, 0.06, 0.01,  220e3, 1e6, 25e3, 33e3, 470e-12, 22e-9, 22e-9,       1e6, 120e-12, 0.25,  { -24.62, -24.62, -28.79, -30.69, -31.60 },   45, 14000, 80, 12000, 11000, 1 },
    // 3: British Classic: AC30 Top Boost style (bass/treble only, 1 M pots), no global feedback, cathode-biased EL84 quartet, Cut control
    { 5, 5, 3.0, 8, 17, 0.035, 0.05, 0.03,     1e6, 1e6, 1, 100e3, 100e-12, 4.7e-9, 22e-9,         500e3, 100e-12, 1.0, { -6.65, -6.65, -13.23, -19.00, -23.86 },   28, 16000, 60, 14000, 11000, 0 } };

// Fender tone stack (Yeh & Smith): analog coefficients, then bilinear transform at rate fs -> o[0..7] = b0..b3, a0..a3; o[8..13] analog
inline void tsDesign (double t, double m, double lin, double fs, double* o, const AmpModel& M = MODELS[0])
{
    const double R1 = M.R1, R2 = M.R2, R3 = M.R3, R4 = M.R4, C1 = M.C1, C2 = M.C2, C3 = M.C3;
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

// Spring tank: modulated delay line + chain of first-order allpass filters (dispersion) + damping, in a feedback loop.
// The slow delay wobble stops the loop from ringing at fixed pitches like a plain comb filter would.
struct Spring
{
    int D = 1, K = 64, mask = 0, w = 0; double a = -0.7, g = 0.9, lp = 0, dk = 0.3, ph = 0, phInc = 0, depth = 0; std::vector<double> buf, z;
    void init (double sr, double sec, int K_, double a_, double g_, double fc, double modHz = 0, double modMs = 0, double phase0 = 0)
    {
        D = (int) std::lround (sec * sr); buf.assign ((size_t) pow2i (D + 64), 0.0); mask = (int) buf.size() - 1; w = 0;
        K = K_; a = a_; z.assign ((size_t) K, 0.0); g = g_; lp = 0; dk = 1 - std::exp (-TAU * fc / sr);
        ph = phase0; phInc = TAU * modHz / sr; depth = modMs * 0.001 * sr;
    }
    inline double p (double x)
    {
        ph += phInc; if (ph > TAU) ph -= TAU;
        const double d = (double) D + depth * std::sin (ph), rp = (double) w - d, fl = std::floor (rp), fr = rp - fl; const int i = (int) fl;
        double v = buf[(size_t) (i & mask)] * (1 - fr) + buf[(size_t) ((i + 1) & mask)] * fr;
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

// ---------- pedals ----------
// Blues-crunch drive: highpass, asymmetric soft clipping at 4x oversampling, tone lowpass
struct DrivePedal
{
    Os4 os; Bq hp, lp; double s4 = 192000, u[512] = {};
    void init (const HB* hb, double sr)
    {
        os = Os4(); os.init (hb); s4 = sr * 4;
        hp = Bq(); hp.set (BqType::HP, 110, 0.6, 0, s4); lp = Bq(); lp.set (BqType::LP, 3000, 0.707, 0, s4);
    }
    void flush() { os.flush(); hp.flush(); lp.flush(); }
    void setTone (double hz) { lp.set (BqType::LP, hz, 0.707, 0, s4); }
    // drive 0..10 -> pre-clip gain in dB
    static double gainDb (double drive) { return 8.0 + clampd (drive, 0, 10) * 2.8; }
    // level normalisation so every drive setting has about the same loudness at Level 5 (table measured with calib)
    static double norm (double drive)
    {
        static const double T[5] = { 0.58, 0.29, 0.17, 0.125, 0.108 };   // about 2 dB hotter than equal loudness, so the pedal pushes the amp
        const double p = clampd (drive, 0, 10) / 2.5; const int i = std::min (3, (int) p);
        return lerpd (T[i], T[i + 1], p - i);
    }
    void process (const double* in, int n, double* out, double drive, double toneHz, double outGain)
    {
        setTone (toneHz);
        os.up (in, n, u);
        const double g = dbl (gainDb (drive)), bias = 0.12, tb = ft (bias);
        for (int i = 0; i < 4 * n; ++i) { const double v = hp.p (u[i]); u[i] = lp.p (ft (v * g + bias) - tb) * outGain; }
        os.down (u, n, out);
    }
};

// Delay with four voicings: 0 mono digital, 1 stereo (second tap on the right), 2 ping-pong, 3 tape (wow and flutter, darker repeats)
struct DelayPedal
{
    std::vector<double> buf, bufR; int mask = 0, w = 0; double lpv = 0, lpv2 = 0, lpk = 0.3, sr = 48000, wph = 0, fph = 0;
    void init (double sampleRate)
    {
        sr = sampleRate; buf.assign ((size_t) pow2i ((int) (1.4 * sr) + 8), 0.0); bufR = buf; mask = (int) buf.size() - 1; w = 0; lpv = lpv2 = 0; wph = fph = 0;
        setTone (5, 1);
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0); std::fill (bufR.begin(), bufR.end(), 0.0); lpv = lpv2 = 0; }
    void setTone (double tone, int type)      // feedback low-pass: about 1.2 kHz .. 8 kHz, darker for tape
    {
        double f = 1200.0 * std::pow (6.7, clampd (tone, 0, 10) / 10); if (type == 3) f *= 0.6; if (type == 0) f *= 1.5;
        lpk = 1 - std::exp (-TAU * std::min (f, 0.45 * sr) / sr);
    }
    inline double rd (const std::vector<double>& b, double d) const
    {
        const double p = (double) w - d; const double fl = std::floor (p); const double fr = p - fl; const int i = (int) fl;
        return b[(size_t) (i & mask)] * (1 - fr) + b[(size_t) ((i + 1) & mask)] * fr;
    }
    inline void process (double in, double dSamples, double fb, int type, double& wl, double& wr)
    {
        if (type == 2)          // ping-pong: left repeat feeds the right line, right repeat feeds back to the left
        {
            wl = rd (buf, dSamples); wr = rd (bufR, dSamples);
            lpv += (wr - lpv) * lpk;
            buf[(size_t) w] = ft ((in + fb * lpv) * 0.9) * 1.1; bufR[(size_t) w] = wl;
        }
        else if (type == 3)     // tape: slow wobble plus a little flutter on the read head, tape-style saturation in the loop
        {
            wph += TAU * 0.55 / sr; fph += TAU * 6.3 / sr; if (wph > TAU) wph -= TAU; if (fph > TAU) fph -= TAU;
            const double d = dSamples + sr * 0.0004 * (std::sin (wph) + 0.25 * std::sin (fph));
            wl = rd (buf, d); wr = rd (buf, d * 0.5 + sr * 0.0003 * std::sin (wph * 1.3));
            lpv += (wl - lpv) * lpk; lpv2 += (lpv - lpv2) * lpk;
            buf[(size_t) w] = ft ((in + fb * lpv2) * 1.1) * 0.95;
        }
        else
        {
            wl = rd (buf, dSamples); wr = type == 1 ? rd (buf, dSamples * 0.62) : wl;
            lpv += (wl - lpv) * lpk;
            buf[(size_t) w] = ft ((in + fb * lpv) * 0.9) * 1.1;
        }
        w = (w + 1) & mask;
    }
};

// Hall reverb: pre-delay, four input diffusers, then an 8-line feedback delay network (Householder mix) with slowly modulated
// delay lines and damping in every line, so the tail is smooth and dark rather than metallic.
struct ReverbPedal
{
    static constexpr int N = 8, NA = 4;
    std::vector<double> d[N], ap[NA], pre; int len[N] = {}, wp[N] = {}, apLen[NA] = {}, apW[NA] = {}, preLen = 1, preW = 0, mask[N] = {};
    double apG = 0, lp[N] = {}, g[N] = {}, ph[N] = {}, inc[N] = {}, outScale = 5.5, depth = 0, dk = 0.3, sr = 48000, hpz = 0, hpk = 0.01;
    double toneLo = 1800, toneSpan = 4;
    void init (double sampleRate, double lenScale = 1.0, double preMs = 12.0, double apBoost = 0.0, double tLo = 1800, double tSpan = 4.0)
    {
        toneLo = tLo; toneSpan = tSpan; apG = apBoost;
        sr = sampleRate; static const int L[N] = { 1429, 1693, 1993, 2371, 2753, 3217, 3613, 4129 }, A[NA] = { 142, 107, 379, 277 };
        static const double R[N] = { 0.17, 0.23, 0.29, 0.31, 0.37, 0.41, 0.43, 0.47 };
        for (int i = 0; i < N; ++i)
        {
            len[i] = std::max (16, (int) std::lround (L[i] * lenScale * sr / 48000)); const int sz = pow2i (len[i] + 64); d[i].assign ((size_t) sz, 0.0); mask[i] = sz - 1; wp[i] = 0; lp[i] = 0;
            ph[i] = i * 0.9; inc[i] = TAU * R[i] / sr;
        }
        depth = 5.0 * std::sqrt (lenScale) * sr / 48000;
        for (int i = 0; i < NA; ++i) { apLen[i] = std::max (4, (int) std::lround (A[i] * lenScale * sr / 48000)); ap[i].assign ((size_t) apLen[i], 0.0); apW[i] = 0; }
        preLen = std::max (1, (int) std::lround (preMs * 0.001 * sr)); pre.assign ((size_t) preLen, 0.0); preW = 0; hpz = 0; hpk = 1 - std::exp (-TAU * 140 / sr);
        setDecay (1.8); setTone (5);
    }
    void clear() { for (auto& v : d) std::fill (v.begin(), v.end(), 0.0); for (auto& v : ap) std::fill (v.begin(), v.end(), 0.0); std::fill (pre.begin(), pre.end(), 0.0); for (auto& x : lp) x = 0; hpz = 0; }
    void setDecay (double rt60) { for (int i = 0; i < N; ++i) g[i] = std::pow (10.0, -3.0 * len[i] / (std::max (0.1, rt60) * sr)); outScale = 5.5 * std::pow (2.0 / std::max (0.3, rt60), 0.4); }
    void setTone (double tone) { dk = 1 - std::exp (-TAU * (toneLo * std::pow (toneSpan, clampd (tone, 0, 10) / 10)) / sr); }
    inline void process (double x, double& oL, double& oR)
    {
        const double pd = pre[(size_t) preW]; pre[(size_t) preW] = x; preW = (preW + 1) % preLen; x = pd;
        hpz += (x - hpz) * hpk; x -= hpz;                                            // keep rumble out of the tank
        static const double AG[NA] = { 0.72, 0.72, 0.62, 0.62 };
        for (int i = 0; i < NA; ++i) { const double ag = std::min (0.82, AG[i] + apG), z = ap[i][(size_t) apW[i]], v = x - ag * z; ap[i][(size_t) apW[i]] = v; x = z + ag * v; apW[i] = (apW[i] + 1) % apLen[i]; }
        double y[N], sum = 0;
        for (int i = 0; i < N; ++i)
        {
            ph[i] += inc[i]; if (ph[i] > TAU) ph[i] -= TAU;
            const double rp = (double) wp[i] - ((double) len[i] + depth * std::sin (ph[i])), fl = std::floor (rp), fr = rp - fl; const int k = (int) fl;
            const double r = d[i][(size_t) (k & mask[i])] * (1 - fr) + d[i][(size_t) ((k + 1) & mask[i])] * fr;
            lp[i] += (r - lp[i]) * dk; y[i] = lp[i] * g[i]; sum += y[i];
        }
        const double h = sum * (2.0 / N);
        for (int i = 0; i < N; ++i) { d[i][(size_t) wp[i]] = x * 0.35 + (y[i] - h); wp[i] = (wp[i] + 1) & mask[i]; }
        oL = outScale * 0.35 * (y[0] - y[2] + y[4] - y[6] + 0.5 * (y[1] + y[3])); oR = outScale * 0.35 * (y[1] - y[3] + y[5] - y[7] + 0.5 * (y[0] + y[2]));
    }
};

// Compressor / sustainer: fast level detector, soft-knee gain computer in dB, separate attack and release on the gain itself,
// and a modest automatic make-up so that tails come up without the pedal turning into a booster.
struct CompPedal
{
    double env = 0, gdb = 0, sr = 48000, ka = 0, kr = 0, kd = 0, kdr = 0;
    void init (double sampleRate) { sr = sampleRate; env = 0; gdb = 0; kd = 1 - std::exp (-1 / (0.0004 * sr)); kdr = 1 - std::exp (-1 / (0.025 * sr)); setAttack (4); setRelease (5); }
    void flush() { flushv (env); flushv (gdb); }
    void setAttack (double knob) { ka = 1 - std::exp (-1 / ((0.002 * std::pow (30.0, clampd (knob, 0, 10) / 10)) * sr)); }      // 2 ms .. 60 ms
    void setRelease (double knob) { kr = 1 - std::exp (-1 / ((0.05 * std::pow (24.0, clampd (knob, 0, 10) / 10)) * sr)); }    // 50 ms .. 1.2 s
    inline double process (double x, double thr, double ratio, double makeup)
    {
        const double a = x < 0 ? -x : x;
        env += (a - env) * (a > env ? kd : kdr);
        const double lv = 20 * std::log10 (env + 1e-7), over = lv - thr, knee = 8.0, s = 1 - 1 / ratio;
        double gr = 0;
        if (over > knee * 0.5) gr = over * s;
        else if (over > -knee * 0.5) { const double q = over + knee * 0.5; gr = s * q * q / (2 * knee); }
        const double target = makeup - gr;
        gdb += (target - gdb) * (target < gdb ? ka : kr);
        return x * std::exp (gdb * 0.11512925464970229);
    }
};

// Stereo chorus: two short modulated delays, opposite LFO phase left/right
struct ChorusPedal
{
    std::vector<double> buf; int mask = 0, w = 0; double ph = 0, sr = 48000;
    void init (double sampleRate) { sr = sampleRate; buf.assign ((size_t) pow2i ((int) (0.05 * sr) + 8), 0.0); mask = (int) buf.size() - 1; w = 0; ph = 0; }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0); }
    inline double rd (double d) const
    {
        const double p = (double) w - d; const double fl = std::floor (p); const double fr = p - fl; const int i = (int) fl;
        return buf[(size_t) (i & mask)] * (1 - fr) + buf[(size_t) ((i + 1) & mask)] * fr;
    }
    inline void process (double in, double rateHz, double depthMs, double& wl, double& wr)
    {
        buf[(size_t) w] = in; ph += rateHz / sr; if (ph >= 1) ph -= 1;
        const double s = std::sin (TAU * ph), c = 0.0115 * sr, dm = depthMs * 0.001 * sr;
        wl = rd (c + dm * s); wr = rd (c - dm * s); w = (w + 1) & mask;
    }
};

// ---------- parameters ----------
struct Params
{
    // amp (knobs 0..10)
    double vol = 4, treb = 5, mid = 5, bass = 4, rev = 2.5, trs = 5, tri = 0;
    bool bright = true;
    int model = 0;      // 0 Fender-style voicings, 1 Blues Junior, 2 Lead Stack, 3 British Classic
    bool sat = false;   // extra valve stage: more saturation
    // pedal order on the board: ids 0 compressor, 1 drive, 2 chorus, 3 delay, 4 reverb (left to right)
    int order[5] = { 0, 1, 2, 3, 4 };
    // cabinet: 0 = 2x12 open back, 1 = 1x12 open back, 2 = 4x12 closed back
    // mic: position 0 (edge of the cone, dark) .. 10 (centre, bright); micDist 0 (close, bassy) .. 10 (far); micType 0..4
    int cab = 0; double mic = 5; bool cabOn = true; int micType = 0; double micDist = 3;
    // noise control
    bool humOn = true; int humHz = 60; double humAmt = 10;
    bool gateOn = true; double gateThr = -58;
    // output (knob 0..10)
    double outVol = 6;
    // amp input gain (5 = unchanged)
    double gain = 5;
    // pedals
    bool cmpOn = false; double cmpSustain = 5, cmpAttack = 4, cmpLevel = 5, cmpRelease = 5;
    bool drvOn = false; double drvDrive = 5, drvTone = 5, drvLevel = 5;
    bool dlyOn = false; double dlyTime = 4, dlyFb = 3, dlyMix = 3, dlyTone = 5; int dlyType = 1;   // 0 mono, 1 stereo, 2 ping-pong, 3 tape
    bool rvbOn = false; double rvbSize = 4, rvbTone = 5, rvbMix = 3; int rvbType = 0;   // 0 hall, 1 plate, 2 spring, 3 room
    bool choOn = false; double choRate = 4, choDepth = 5, choMix = 5;
};

struct CabVoicing { double hp, rf, rg, pf, pk, df, dg, lp; };
static const CabVoicing CABS[3] = { { 70, 100, 2.0, 3000, 3.5, 6800, -2.0, 8200 }, { 80, 115, 2.5, 2600, 3.0, 5800, -2.5, 7400 }, { 75, 95, 3.0, 2300, 4.5, 5000, -3.5, 6800 } };
struct MicVoicing { double f1, g1, q1, f2, g2, q2; };    // two peaking filters that give each microphone its character
static const MicVoicing MICS[5] = { { 5500, 0.3, 1.1, 140, 0.0, 0.9 },    // dynamic, 57-style: nearly flat, so the original tone is kept
                                    { 4500, -3.5, 0.7, 200, 2.5, 0.8 },   // ribbon, 121-style: dark and warm
                                    { 9000, 2.5, 0.6, 3000, 1.0, 0.8 },   // large condenser: open top end
                                    { 650, -2.0, 0.9, 4000, 3.0, 1.0 },   // dynamic, 421-style: scooped mids
                                    { 120, 2.5, 0.8, 8000, 1.5, 0.7 } };  // tube condenser: big and smooth

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
        spA.init (sr, 0.0343, 32, -0.60, 0.88, 3300, 0.8, 0.6, 0.0); spB.init (sr, 0.0419, 32, -0.56, 0.88, 3036, 1.1, 0.6, 2.0);
        spAct = 0; spLo = 0; spHk = 1 - std::exp (-TAU * 190 / sr); spTail = 0;
        for (int m = 0; m < 4; ++m) { double o[14]; tsDesign (0.6, 0.4, 0.4, s4, o, MODELS[m]); tsRefs[m] = tsPow (o); }
        for (int m = 0; m < 4; ++m) tsMkM[m] = FEN::tsMk * std::sqrt (tsRefs[0] / tsRefs[m]);
        tsRef = tsRefs[0]; am = &MODELS[0]; modelIdx = 0;
        std::fill (std::begin (tsZ), std::end (tsZ), 0.0); brZ = 0;
        aHp[0] = aHp[1] = aLp[0] = aLp[1] = tP = tPh = Bq();
        cHp = cRes = cPk = cDip = cL1 = cL2 = cM1 = cM2 = cProx = Bq();
        pedC.init (sr); pedD.init (&hb, sr); pedL.init (sr); pedR.init (sr); pedP.init (sr, 0.55, 1.5, 0.08, 3000, 4.3); pedRm.init (sr, 0.2, 3.0, 0.05, 2200, 3.6); pedCh.init (sr); dlyLive = rvbLive = choLive = false;
        fade = fadeOld = 1; haveP = false; cabIdx = -1;
        std::fill (cur, cur + NP, 0.0); std::fill (old, old + NP, 0.0); std::fill (tgt, tgt + NP, 0.0);
    }

    void setParams (const Params& P)
    {
        const int mi = P.model < 0 ? 0 : (P.model > 3 ? 3 : P.model);
        const bool modelChanged = haveP && mi != modelIdx;
        modelIdx = mi; am = &MODELS[mi]; tsRef = tsRefs[mi];
        derive (P);
        if (modelChanged) { fade = 0; fadeOld = 0; }
        if (! haveP) { std::copy (tgt, tgt + NP, cur); std::copy (tgt, tgt + NP, old); coefs(); fade = fadeOld = 1; cabIdx = P.cab * 16 + P.micType; haveP = true; }
        else if (P.cab * 16 + P.micType != cabIdx) { cabIdx = P.cab * 16 + P.micType; fade = 0; fadeOld = 0; }
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
    enum { GateThr, GateOn, AT, AM, AL, AV, ABr, AG1, AG2, APg, ATrim, ATrR, ATrD, SpMix, ASag, CabOn, CabHP, CabRF, CabRG, CabPF, CabPG, CabDF, CabDG, CabLP, Vol, HumAmt, AGn, CoOn, CoLv, PdOn, PdLv, DlOn, DlTime, DlFb, DlMix, RvOn, RvMix, ChOn, ChRate, ChDepth, ChMix, CabM1F, CabM1G, CabM2F, CabM2G, CabProx, TpF, NP };
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
    Bq aHp[2], aLp[2], tP, tPh; double tsC[14] = {}, tsZ[3] = {}, brZ = 0, brC[3] = {}, tsRef = 1, tsCmp = 1, tsRefs[4] = { 1, 1, 1, 1 }, tsMkM[4] = { 12, 12, 12, 12 };
    const AmpModel* am = &MODELS[0]; int modelIdx = 0;
    double sagEnv = 0, sagKa = 0, sagKr = 0, trPh = 0;
    // spring
    Spring spA, spB; double spAct = 0, spLo = 0, spHk = 0, spTail = 0;
    // cab
    Bq cHp, cRes, cPk, cDip, cL1, cL2, cM1, cM2, cProx; double mq1 = 1, mq2 = 1;
    // work buffers
    double b1[128], u[128 * 4], tg[128];
    // pedals
    int preOrd[2] = { 0, 1 }, postOrd[3] = { 2, 3, 4 }; ChorusPedal pedCh; bool choLive = false;
    CompPedal pedC; double coThr = -40, coRatio = 6, coMake = 10, coAtt = 4, coRel = 5;
    DrivePedal pedD; DelayPedal pedL; ReverbPedal pedR, pedP, pedRm; int dlType = 1, rvType = 0, rvTypeLive = -1; double spG = 0.88, rvTypeGain[4] = { 1.0, 0.58, 1.0, 0.52 }; double pdDrive = 5, pdTone = 3000, rvDecay = 1.5, rvTone = 5; bool dlyLive = false, rvbLive = false;

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
        // Gain is the only drive control: it sweeps the preamp level pot (positions 0..10 over knob 0..8) and, above 8, pushes extra level into the first stage.
        // Volume is a pure output level after everything else.
        const double gk = clampd (P.gain, 0, 10), vol = std::min (gk, 8.0) * 1.25;
        t[AT] = clampd (P.treb, 0, 10) / 10; t[AM] = clampd (P.mid, 0, 10) / 10; t[AL] = clampd (P.bass, 0, 10) / 10;
        t[AV] = vol <= 0 ? 0 : std::pow (10.0, ((vol - 10) * 3.0) / 20) * std::pow (std::min (1.0, vol / 1.5), 2.0);
        t[ABr] = P.bright ? 1 : 0;
        t[TpF] = (am->midPot > 0.5) ? am->tpLP : am->tpLP * std::pow (0.16, clampd (P.mid, 0, 10) / 10);   // British Classic: the Middle knob becomes the Cut control
        if (am->midPot < 0.5) t[AM] = 0.5;
        t[AG1] = dbl (am->g1); t[AG2] = dbl (am->g2) * (P.sat ? 1.7 : 1.0); t[APg] = P.sat ? 1.5 : 1.0; t[ASag] = am->sag;
        t[ATrR] = 0.8 * std::pow (15.0, clampd (P.trs, 0, 10) / 10); t[ATrD] = P.tri <= 0 ? 0 : 0.92 * std::pow (clampd (P.tri, 0, 10) / 10, 1.1);
        t[SpMix] = P.rev <= 0 ? 0 : std::pow (clampd (P.rev, 0, 10) / 10, 1.5);
        { const double pos = vol / 2.5; const int i0 = std::min (3, (int) std::floor (pos)); const double fr = pos - i0; t[ATrim] = dbl (lerpd (am->lc[i0], am->lc[i0 + 1], fr)) * (P.sat ? 0.5 : 1.0); }
        const CabVoicing& c = CABS[P.cab < 0 ? 0 : (P.cab > 2 ? 2 : P.cab)]; const MicVoicing& mv = MICS[P.micType < 0 ? 0 : (P.micType > 4 ? 4 : P.micType)]; const double md = clampd (P.micDist, 0, 10) / 10; const double mic = clampd (P.mic, 0, 10) / 10;
        t[CabOn] = P.cabOn ? 1 : 0; t[CabHP] = c.hp; t[CabRF] = c.rf; t[CabRG] = c.rg;
        t[CabPF] = c.pf; t[CabPG] = c.pk + (mic - 0.5) * 4; t[CabDF] = c.df; t[CabDG] = c.dg; t[CabLP] = c.lp * (0.78 + 0.44 * mic) * (1 - 0.12 * (md - 0.3));
        t[CabM1F] = mv.f1; t[CabM1G] = mv.g1; t[CabM2F] = mv.f2; t[CabM2G] = mv.g2; mq1 = mv.q1; mq2 = mv.q2; t[CabProx] = (0.3 - md) * 6.0;
        t[Vol] = std::pow (clampd (P.outVol, 0, 10) / 10, 1.5) / std::pow (0.6, 1.5);
        t[HumAmt] = P.humOn ? clampd (P.humAmt, 0, 10) / 10 : 0;
        humNom = P.humHz == 50 ? 50 : 60;
        t[AGn] = dbl (gk > 8 ? (gk - 8) * 6.0 : 0.0);
        t[CoOn] = P.cmpOn ? 1 : 0; t[CoLv] = dbl ((clampd (P.cmpLevel, 0, 10) - 5) * 2.4);
        { const double su = clampd (P.cmpSustain, 0, 10); coThr = -12 - su * 2.4; coRatio = 2.5 + su * 0.55; coMake = 0.8 * std::max (0.0, -12 - coThr) * (1 - 1 / coRatio); coAtt = P.cmpAttack; coRel = P.cmpRelease; }
        t[PdOn] = P.drvOn ? 1 : 0; t[PdLv] = dbl ((clampd (P.drvLevel, 0, 10) - 5) * 2.4);
        pdDrive = clampd (P.drvDrive, 0, 10); pdTone = 600.0 * std::pow (10.0, clampd (P.drvTone, 0, 10) / 10 * 1.0);   // 600 Hz .. 6 kHz
        t[DlOn] = P.dlyOn ? 1 : 0; t[DlTime] = 0.06 * std::pow (12.0, clampd (P.dlyTime, 0, 10) / 10);                  // 60 ms .. 720 ms
        t[DlFb] = clampd (P.dlyFb, 0, 10) / 10 * 0.85; t[DlMix] = clampd (P.dlyMix, 0, 10) / 10 * 0.9;
        t[RvOn] = P.rvbOn ? 1 : 0; t[RvMix] = clampd (P.rvbMix, 0, 10) / 10 * 0.8;
        t[ChOn] = P.choOn ? 1 : 0; t[ChRate] = 0.15 * std::pow (40.0, clampd (P.choRate, 0, 10) / 10); t[ChDepth] = 0.4 + clampd (P.choDepth, 0, 10) / 10 * 4.6; t[ChMix] = clampd (P.choMix, 0, 10) / 10;
        { int k = 0, q = 0; for (int i = 0; i < 5; ++i) { const int id = P.order[i]; if (id == 0 || id == 1) { if (k < 2) preOrd[k++] = id; } else if (id >= 2 && id <= 4) { if (q < 3) postOrd[q++] = id; } }
          if (k < 2 || q < 3) { preOrd[0] = 0; preOrd[1] = 1; postOrd[0] = 2; postOrd[1] = 3; postOrd[2] = 4; } }
        rvType = P.rvbType < 0 ? 0 : (P.rvbType > 3 ? 3 : P.rvbType);
        { const double sz = clampd (P.rvbSize, 0, 10) / 10;
          static const double dMin[4] = { 0.4, 0.5, 0, 0.12 }, dRatio[4] = { 15.0, 8.0, 1, 7.5 };   // decay seconds: hall 0.4..6, plate 0.5..4, room 0.12..0.9
          rvDecay = dMin[rvType] * std::pow (dRatio[rvType], sz); rvTone = P.rvbTone; spG = 0.80 + 0.135 * sz; }
        dlType = P.dlyType < 0 ? 0 : (P.dlyType > 3 ? 3 : P.dlyType); pedL.setTone (P.dlyTone, dlType);
    }
    void coefs()
    {
        const double* c = cur;
        aHp[0].set (BqType::HP, am->hp1, 0.707, 0, s4); aLp[0].set (BqType::LP, am->lp1, 0.707, 0, s4);
        aHp[1].set (BqType::HP, am->hp2, 0.707, 0, s4); aLp[1].set (BqType::LP, am->lp2, 0.707, 0, s4);
        tsDesign (c[AT], c[AM], c[AL], s4, tsC, *am);
        tsCmp = std::pow (tsRef / tsPow (tsC), 0.25);
        { const double v = c[AV], tau = (1 - v) * am->rv * am->bc * c[ABr], cc = 2 * s4, d0 = 1 + v * cc * tau;
          brC[0] = v * (1 + cc * tau) / d0; brC[1] = v * (1 - cc * tau) / d0; brC[2] = (1 - v * cc * tau) / d0; }
        tP.set (BqType::LP, c[TpF], 0.707, 0, s4); tPh.set (BqType::HP, 38, 0.707, 0, s4);
        cHp.set (BqType::HP, c[CabHP], 0.707, 0, sr); cRes.set (BqType::Peak, c[CabRF], 1, c[CabRG], sr);
        cPk.set (BqType::Peak, c[CabPF], 0.9, c[CabPG], sr); cDip.set (BqType::Peak, c[CabDF], 1.4, c[CabDG], sr);
        cM1.set (BqType::Peak, c[CabM1F], mq1, c[CabM1G], sr); cM2.set (BqType::Peak, c[CabM2F], mq2, c[CabM2G], sr); cProx.set (BqType::LowShelf, 160, 0.7, c[CabProx], sr);
        cL1.set (BqType::LP, c[CabLP], 0.5412, 0, sr); cL2.set (BqType::LP, c[CabLP], 1.3066, 0, sr);
    }
    void flushAll()
    {
        inHp.flush(); inHp2.flush(); detHp.flush();
        for (int i = 0; i < 2; ++i) { aHp[i].flush(); aLp[i].flush(); }
        tP.flush(); tPh.flush(); flushArr (tsZ); flushv (brZ); spA.flush(); spB.flush();
        flushv (sagEnv); flushv (spLo);
        cHp.flush(); cRes.flush(); cPk.flush(); cDip.flush(); cL1.flush(); cL2.flush(); cM1.flush(); cM2.flush(); cProx.flush();
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

        // B0: compression sustainer
        auto doComp = [&] {
        if (old[CoOn] > 1e-4 || cur[CoOn] > 1e-4)
        {
            pedC.setAttack (coAtt); pedC.setRelease (coRel);
            const double e0 = old[CoOn], e1 = cur[CoOn], l0 = old[CoLv], l1 = cur[CoLv];
            for (int i = 0; i < n; ++i)
            {
                const double e = e0 + (e1 - e0) * (i + 1) * inv, lv = l0 + (l1 - l0) * (i + 1) * inv, dry = b1[i];
                b1[i] = dry * (1 - e) + pedC.process (dry, coThr, coRatio, coMake) * lv * e;
            }
        }
        else pedC.flush();
        };

        // B: drive pedal in front of the amp
        auto doDrive = [&] {
        if (old[PdOn] > 1e-4 || cur[PdOn] > 1e-4)
        {
            double wet[128]; pedD.process (b1, n, wet, pdDrive, pdTone, DrivePedal::norm (pdDrive) * cur[PdLv]);
            const double e0 = old[PdOn], e1 = cur[PdOn];
            for (int i = 0; i < n; ++i) { const double e = e0 + (e1 - e0) * (i + 1) * inv; b1[i] = b1[i] * (1 - e) + wet[i] * e; }
        }
        else pedD.flush();
        };
        for (int pi = 0; pi < 2; ++pi) { if (preOrd[pi] == 0) doComp(); else doDrive(); }

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
            const double g1 = cur[AG1] * cur[AGn] / am->h1, g2 = cur[AG2] / am->h2;
            stage (aHp[0], aLp[0], n4, g1, am->b1, am->h1);
            // tone stack
            { const double b0 = tsC[0], bb1 = tsC[1], bb2 = tsC[2], bb3 = tsC[3], a1 = tsC[5], a2 = tsC[6], a3 = tsC[7], mk = dbl (tsMkM[modelIdx]) * tsCmp;
              double z1 = tsZ[0], z2 = tsZ[1], z3 = tsZ[2];
              for (int i = 0; i < n4; ++i) { const double xx = u[i], y = b0 * xx + z1; z1 = bb1 * xx - a1 * y + z2; z2 = bb2 * xx - a2 * y + z3; z3 = bb3 * xx - a3 * y; u[i] = y * mk; }
              tsZ[0] = z1; tsZ[1] = z2; tsZ[2] = z3; }
            // volume pot with bright cap
            { const double b0 = brC[0], bb1 = brC[1], a1 = brC[2]; double z = brZ;
              for (int i = 0; i < n4; ++i) { const double xx = u[i], y = b0 * xx + z; z = bb1 * xx - a1 * y; u[i] = y; }
              brZ = z; }
            stage (aHp[1], aLp[1], n4, g2, am->b2, am->h2);
            // power stage with supply sag
            { const double H = am->hp, bp = am->bp, tbp = ft (bp), gp = cur[APg], trim = cur[ATrim], sagK = cur[ASag]; double env = sagEnv;
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
                const double v = b1[i], w = cL2.p (cL1.p (cM2.p (cM1.p (cProx.p (cDip.p (cPk.p (cRes.p (cHp.p (v)))))))));
                const double e = cb0 + (cb1 - cb0) * (i + 1) * inv;
                b1[i] = v * (1 - e) + w * 1.6 * e;
            }

        // E: spring reverb, volume, limiter
        const double v0 = old[Vol], v1 = cur[Vol], sp0 = old[SpMix], sp1 = cur[SpMix];
        const bool dlyAct = old[DlOn] > 1e-4 || cur[DlOn] > 1e-4, rvbAct = old[RvOn] > 1e-4 || cur[RvOn] > 1e-4;
        if (dlyAct && ! dlyLive) pedL.clear();
        if (rvbAct && ! rvbLive) pedR.clear();
        const bool choAct = old[ChOn] > 1e-4 || cur[ChOn] > 1e-4; if (choAct && ! choLive) pedCh.clear(); choLive = choAct;
        dlyLive = dlyAct; rvbLive = rvbAct;
        if (rvbAct)
        {
            if (rvType != rvTypeLive) { pedR.clear(); pedP.clear(); pedRm.clear(); spA.flush(); spB.flush(); spLo = 0; rvTypeLive = rvType; }
            ReverbPedal* rp = rvType == 1 ? &pedP : (rvType == 3 ? &pedRm : &pedR);
            if (rvType != 2) { rp->setDecay (rvDecay); rp->setTone (rvTone); }
            else { spA.g = spB.g = spG; const double f = 1800.0 * std::pow (3.0, clampd (rvTone, 0, 10) / 10); spA.dk = spB.dk = 1 - std::exp (-TAU * f / sr); }
        }
        else rvTypeLive = -1;
        for (int i = 0; i < n; ++i)
        {
            const double t = (i + 1) * inv, s = b1[i]; double l = s, r = s;
                        for (int pi = 0; pi < 3; ++pi)
            {
                const int po = postOrd[pi];
            if (po == 2)
            {
            if (choAct)
            {
                const double en = old[ChOn] + (cur[ChOn] - old[ChOn]) * t; double wl, wr;
                pedCh.process ((l + r) * 0.5, lerpd (old[ChRate], cur[ChRate], t), lerpd (old[ChDepth], cur[ChDepth], t), wl, wr);
                const double mx = lerpd (old[ChMix], cur[ChMix], t) * en * 0.5; l = l * (1 - mx) + wl * mx; r = r * (1 - mx) + wr * mx;
            }
            }
            if (po == 3)
            {
            if (dlyAct)
            {
                const double e = old[DlOn] + (cur[DlOn] - old[DlOn]) * t, tm = lerpd (old[DlTime], cur[DlTime], t) * sr;
                double wl, wr; pedL.process ((l + r) * 0.5, tm, lerpd (old[DlFb], cur[DlFb], t), dlType, wl, wr);
                const double mx = lerpd (old[DlMix], cur[DlMix], t) * e; l += wl * mx; r += wr * mx;
            }
            }
            if (po == 4)
            {
            if (rvbAct)
            {
                const double e = old[RvOn] + (cur[RvOn] - old[RvOn]) * t; double wl = 0, wr = 0; const double xin = (l + r) * 0.5;
                if (rvType == 2)
                {
                    spLo += (xin - spLo) * spHk;
                    const double xi = ft ((xin - spLo) * 2) * 0.5 + 1e-18, wa = spA.p (xi), wb = spB.p (xi);
                    wl = (wa + 0.35 * wb) * 1.3; wr = (wb + 0.35 * wa) * 1.3;
                }
                else (rvType == 1 ? pedP : (rvType == 3 ? pedRm : pedR)).process (xin, wl, wr);
                const double mx = lerpd (old[RvMix], cur[RvMix], t) * e * rvTypeGain[rvType]; l += wl * mx; r += wr * mx;
            }
            }
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
