/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (HARTERMIST) */
#include "Render.h"
#include <cmath>

namespace hmui
{
namespace
{
struct V3 {
    float x, y, z;
};
inline V3 norm3(V3 v)
{
    float l = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    return l > 0.0f ? V3{v.x / l, v.y / l, v.z / l} : V3{0, 0, 1};
}
inline float dot3(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline float clamp01(float x) { return x < 0.0f ? 0.0f : x > 1.0f ? 1.0f : x; }
inline float smooth(float a, float b, float x)
{
    float t = clamp01((x - a) / (b - a));
    return t * t * (3.0f - 2.0f * t);
}
const V3 L = norm3({-0.55f, -0.75f, 0.95f});     /* the key light, from the upper left */
const V3 H = norm3({L.x, L.y, L.z + 1.0f});      /* (halfway to the eye, straight above) */

inline uint32_t hash(int x, int y, uint32_t s)
{
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + s * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}
inline float h01(int x, int y, uint32_t s) { return (float)(hash(x, y, s) & 0xffffff) / 16777216.0f; }
/* value noise, smooth */
float vnoise(float x, float y, uint32_t s)
{
    int xi = (int)std::floor(x), yi = (int)std::floor(y);
    float fx = x - (float)xi, fy = y - (float)yi;
    fx = fx * fx * (3.0f - 2.0f * fx);
    fy = fy * fy * (3.0f - 2.0f * fy);
    float a = h01(xi, yi, s), b = h01(xi + 1, yi, s), c = h01(xi, yi + 1, s), d = h01(xi + 1, yi + 1, s);
    return a + (b - a) * fx + (c - a) * fy + (a - b - c + d) * fx * fy;
}
float fbm(float x, float y, uint32_t s, int oct = 4)
{
    float v = 0.0f, a = 0.5f, f = 1.0f;
    for (int i = 0; i < oct; i++) {
        v += a * vnoise(x * f, y * f, s + (uint32_t)i * 101u);
        f *= 2.03f;
        a *= 0.5f;
    }
    return v;
}
struct Rng {
    uint32_t s;
    explicit Rng(uint32_t seed) : s(seed ? seed : 1u) {}
    float next()
    {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return (float)(s & 0xffffff) / 16777216.0f;
    }
    float range(float a, float b) { return a + (b - a) * next(); }
};
inline juce::uint8 to8(float v) { return (juce::uint8)juce::jlimit(0, 255, (int)std::lround(v * 255.0f)); }

/* a pixel written: straight colour and alpha (the image premultiplies) */
inline void put(juce::Image::BitmapData &bd, int x, int y, float r, float g, float b, float a)
{
    bd.setPixelColour(x, y, juce::Colour::fromFloatRGBA(clamp01(r), clamp01(g), clamp01(b), clamp01(a)));
}

/* the soft shadow a round part drops (ρ in its radius, offset down right) */
inline float dropShadow(float dx, float dy, float ox, float oy, float spread, float strength)
{
    float sx = dx - ox, sy = dy - oy, sr = std::sqrt(sx * sx + sy * sy);
    float t = clamp01(1.0f - (sr - 0.85f) / spread);
    return strength * t * t;
}

/* a fingerprint's grease at (u, v) in its own frame (-1..1): its ridges in whorls, smeared, faded to its edge */
float printAt(float u, float v, uint32_t seed, float ridges)
{
    float r = std::sqrt(u * u + v * v * 0.75f);
    if (r >= 1.0f)
        return 0.0f;
    /* the ridges: loops round a core low in the print, bent (the skin is not a circle), pressed flat at the tip */
    float wu = u + 0.35f * (fbm(u * 1.3f + 7.0f, v * 1.3f, seed, 3) - 0.5f);
    float wv = v + 0.35f * (fbm(u * 1.3f, v * 1.3f + 4.0f, seed + 5u, 3) - 0.5f);
    float cu = wu * 1.25f, cv = (wv + 0.35f) * 0.85f;
    float loop = std::sqrt(cu * cu + cv * cv) + 0.22f * cv * std::fabs(cu) - 0.12f * cu;
    float ridge = 0.5f + 0.5f * std::sin(loop * ridges * 6.2832f);
    /* partly pressed, partly smeared away: the ridges show only where the finger lay flat */
    float press = smooth(0.35f, 0.75f, fbm(u * 2.2f + 1.0f, v * 2.2f, seed + 9u, 3));
    float edge = 1.0f - smooth(0.4f, 1.0f, r);
    float smear = smooth(0.2f, 0.75f, fbm(u * 0.9f + 3.0f, v * 3.5f, seed + 13u, 3));   /* (wiped along) */
    float haze = 0.45f * smear;
    return edge * (haze + 0.55f * press * ridge) * (0.5f + 0.5f * smear);
}
/* how worn a place is (0..1), from where hands go: worked out on a coarse grid, read smoothly */
struct WearGrid {
    int gw = 0, gh = 0;
    float cell = 4.0f;
    std::vector<float> v;
    WearGrid(float w, float h, const std::vector<juce::Point<float>> &hot, float rx, float ry, bool squared)
    {
        gw = (int)(w / cell) + 2;
        gh = (int)(h / cell) + 2;
        v.assign((size_t)(gw * gh), 0.0f);
        for (auto &p : hot) {
            int x0 = juce::jmax(0, (int)((p.x - rx) / cell)), x1 = juce::jmin(gw - 1, (int)((p.x + rx) / cell) + 1);
            int y0 = juce::jmax(0, (int)((p.y - ry) / cell)), y1 = juce::jmin(gh - 1, (int)((p.y + ry) / cell) + 1);
            for (int y = y0; y <= y1; y++)
                for (int x = x0; x <= x1; x++) {
                    float dx = ((float)x * cell - p.x) / rx, dy = ((float)y * cell - p.y) / ry, q = dx * dx + dy * dy;
                    if (q < 1.0f) {
                        float w_ = squared ? (1.0f - q) * (1.0f - q) : 1.0f - q;
                        float &t = v[(size_t)(y * gw + x)];
                        t = juce::jmax(t, w_);
                    }
                }
        }
    }
    float at(float x, float y) const
    {
        float fx = x / cell, fy = y / cell;
        int xi = juce::jlimit(0, gw - 2, (int)fx), yi = juce::jlimit(0, gh - 2, (int)fy);
        float tx = clamp01(fx - (float)xi), ty = clamp01(fy - (float)yi);
        const float *r0 = &v[(size_t)(yi * gw + xi)], *r1 = r0 + gw;
        return (r0[0] * (1 - tx) + r0[1] * tx) * (1 - ty) + (r1[0] * (1 - tx) + r1[1] * tx) * ty;
    }
};
} // namespace

float jitter(uint32_t seed, int k) { return h01(k, 77, seed) * 2.0f - 1.0f; }
float shadowMargin(float d) { return d * 0.38f + 4.0f; }

/* ===================================================================================== the knob === */
juce::Image renderKnob(float d, int kind, uint32_t seed, float sc)
{
    const float m = shadowMargin(d), full = d + 2.0f * m;
    const int S = juce::jmax(4, (int)std::ceil(full * sc));
    juce::Image img(juce::Image::ARGB, S, S, true);
    juce::Image::BitmapData bd(img, juce::Image::BitmapData::writeOnly);
    const float c = (float)S * 0.5f, R = d * 0.5f * sc;
    Rng rnd(seed);
    /* this one's tolerances: its cap a little off its centre, its turned rings, its knurl's start */
    const float ex = rnd.range(-0.014f, 0.014f), ey = rnd.range(-0.014f, 0.014f);
    const float capR = kind == 2 ? 0.74f : 0.645f, groove = capR + 0.032f;
    const int N = kind == 2 ? 72 : kind == 1 ? 56 : 44;
    const float kph = rnd.range(0.0f, 6.28f);
    const uint32_t ringSeed = seed * 31u + 5u;
    const bool printed = rnd.next() < 0.45f;   /* (some caps touched more than others) */
    const float pu = rnd.range(-0.25f, 0.25f), pv = rnd.range(-0.25f, 0.25f), pa = rnd.range(0.0f, 3.14f);
    for (int y = 0; y < S; y++)
        for (int x = 0; x < S; x++) {
            /* four samples a pixel: smooth edges */
            float accR = 0, accG = 0, accB = 0, shSum = 0;
            int body = 0;
            for (int sy = 0; sy < 2; sy++)
                for (int sx = 0; sx < 2; sx++) {
                    float px = ((float)x + 0.25f + 0.5f * (float)sx - c) / R, py = ((float)y + 0.25f + 0.5f * (float)sy - c) / R;
                    float rho = std::sqrt(px * px + py * py);
                    if (rho > 1.0f) {
                        /* the panel under it: its shadow, darkest where it stands (the contact) */
                        float sh = dropShadow(px, py, 0.10f, 0.16f, 0.55f, 0.72f) + 0.35f * (1.0f - smooth(1.0f, 1.07f, rho));
                        shSum += clamp01(sh);
                        continue;
                    }
                    body++;
                    float th = std::atan2(py, px);
                    float ct = std::cos(th), st = std::sin(th);
                    float cx = px - ex, cy = py - ey, cr = std::sqrt(cx * cx + cy * cy), cth = std::atan2(cy, cx);
                    float r, g, b;
                    if (cr < capR) {
                        /* the cap: brushed aluminium, turned: its edge chamfered and polished */
                        float cct = std::cos(cth), cst = std::sin(cth);
                        V3 n;
                        bool chamfer = cr > capR - 0.045f;
                        if (chamfer) {
                            float t = (cr - (capR - 0.045f)) / 0.045f;
                            float a = 0.35f + 0.75f * t;
                            n = norm3({std::sin(a) * cct, std::sin(a) * cst, std::cos(a)});
                        } else
                            n = norm3({0.07f * cr / capR * cct, 0.07f * cr / capR * cst, 1.0f});
                        float rings = vnoise(cr * R * 1.9f, 0.5f, ringSeed) * 0.6f + vnoise(cr * R * 0.35f, 2.5f, ringSeed + 3u) * 0.4f;
                        V3 T{-cst, cct, 0.0f};
                        float th_ = dot3(T, H);
                        float aniso = std::sqrt(juce::jmax(0.0f, 1.0f - th_ * th_));
                        float spec = 0.95f * std::pow(aniso, 90.0f) + 0.32f * std::pow(aniso, 9.0f);
                        spec *= 0.8f + 0.45f * rings;
                        float diff = juce::jmax(0.0f, dot3(n, L));
                        float base = 0.20f + 0.30f * diff + 0.05f * (rings - 0.5f);
                        if (chamfer)
                            spec = 1.25f * std::pow(juce::jmax(0.0f, dot3(n, H)), 40.0f) + 0.15f;
                        /* the centre where it was turned: a small dimple */
                        if (cr < 0.035f)
                            base *= 0.75f + 0.25f * cr / 0.035f;
                        float v = base + spec;
                        r = v * 0.97f;
                        g = v * 0.985f;
                        b = v * 1.0f;
                        if (printed) {
                            /* a fingerprint: the grease takes the polish (a hazy, flatter patch, its ridges) */
                            float ru = (cx - pu) / (capR * 0.75f), rv = (cy - pv) / (capR * 0.95f);
                            float u = ru * std::cos(pa) - rv * std::sin(pa), w = ru * std::sin(pa) + rv * std::cos(pa);
                            float f = printAt(u, w, seed + 11u, 9.0f);
                            r = r + (0.42f - r) * f * 0.55f;
                            g = g + (0.42f - g) * f * 0.55f;
                            b = b + (0.44f - b) * f * 0.55f;
                        }
                    } else if (cr < groove) {
                        /* the dark gap under the cap's edge */
                        float t = (cr - capR) / (groove - capR);
                        float v = 0.012f + 0.02f * t;
                        r = g = b = v;
                    } else {
                        /* the skirt: black satin, knurled; its side falling away to its edge */
                        float t = (rho - groove) / (1.0f - groove);
                        float a = 0.18f + 1.05f * t * t * t + (rho > 0.95f ? (rho - 0.95f) * 8.0f : 0.0f);
                        float ph = th * (float)N + kph;
                        float knurl = smooth(0.05f, 0.25f, t);
                        float tilt = 0.62f * std::sin(ph) * knurl;
                        V3 n = norm3({std::sin(a) * ct - tilt * st, std::sin(a) * st + tilt * ct, std::cos(a)});
                        float diff = juce::jmax(0.0f, dot3(n, L));
                        float spec = 0.38f * std::pow(juce::jmax(0.0f, dot3(n, H)), 28.0f);
                        float ao = 1.0f - 0.35f * knurl * (0.5f - 0.5f * std::cos(ph));
                        float grain = 0.85f + 0.3f * h01(x, y, seed);
                        float v = (0.028f + 0.07f * diff) * ao + spec * grain * ao;
                        v += 0.06f * std::pow(1.0f - n.z, 3.0f);   /* (a little sky in its far side) */
                        r = v;
                        g = v * 1.01f;
                        b = v * 1.04f;
                    }
                    accR += r;
                    accG += g;
                    accB += b;
                }
            float shadowA = shSum / 4.0f;
            float cov = (float)body / 4.0f;
            if (body > 0) {
                float r = accR / (float)body, g = accG / (float)body, b = accB / (float)body;
                float a = cov + shadowA * (1.0f - cov);
                float k = a > 0.0f ? cov / a : 0.0f;
                put(bd, x, y, r * k, g * k, b * k, a);
            } else if (shadowA > 0.002f)
                put(bd, x, y, 0.0f, 0.0f, 0.0f, shadowA);
        }
    /* dust on it: a speck or two, lit from above, their tiny shadows */
    {
        juce::Graphics g(img);
        int n = (int)(rnd.next() * 3.2f);
        for (int i = 0; i < n; i++) {
            float a = rnd.range(0.0f, 6.28f), rr = rnd.range(0.1f, 0.92f) * R;
            float px = c + std::cos(a) * rr, py = c + std::sin(a) * rr, s = rnd.range(0.35f, 0.9f) * sc;
            g.setColour(juce::Colours::black.withAlpha(0.35f));
            g.fillEllipse(px - s + s * 0.5f, py - s + s * 0.7f, 2 * s, 2 * s);
            g.setColour(juce::Colour::fromFloatRGBA(0.78f, 0.77f, 0.74f, rnd.range(0.5f, 0.9f)));
            g.fillEllipse(px - s, py - s, 2 * s, 2 * s);
        }
        /* a fine scratch across the cap, now and then */
        if (rnd.next() < 0.35f) {
            float a = rnd.range(0.0f, 6.28f), l = rnd.range(0.15f, 0.4f) * R, o = rnd.range(-0.3f, 0.3f) * R;
            float x0 = c + o * std::cos(a + 1.57f) - std::cos(a) * l, y0 = c + o * std::sin(a + 1.57f) - std::sin(a) * l;
            g.setColour(juce::Colours::white.withAlpha(0.28f));
            g.drawLine(x0, y0, x0 + 2 * l * std::cos(a), y0 + 2 * l * std::sin(a), 0.45f * sc);
        }
    }
    return img;
}

/* ================================================================================ the slider's cap === */
juce::Image renderFaderCap(float d, uint32_t seed, float sc)
{
    const float m = shadowMargin(d) * 1.3f, full = d + 2.0f * m;
    const int S = juce::jmax(4, (int)std::ceil(full * sc));
    juce::Image img(juce::Image::ARGB, S, S, true);
    juce::Image::BitmapData bd(img, juce::Image::BitmapData::writeOnly);
    const float c = (float)S * 0.5f, R = d * 0.5f * sc;
    Rng rnd(seed);
    const float tiltx = rnd.range(-0.06f, 0.06f), tilty = rnd.range(-0.06f, 0.06f);   /* (not quite level) */
    for (int y = 0; y < S; y++)
        for (int x = 0; x < S; x++) {
            float r = 0, gg = 0, b = 0, sh = 0;
            int body = 0;
            for (int sy = 0; sy < 2; sy++)
                for (int sx = 0; sx < 2; sx++) {
                    float px = ((float)x + 0.25f + 0.5f * (float)sx - c) / R, py = ((float)y + 0.25f + 0.5f * (float)sy - c) / R;
                    float rho = std::sqrt(px * px + py * py);
                    if (rho > 1.0f) {
                        sh += clamp01(dropShadow(px, py, 0.18f, 0.36f, 0.8f, 0.8f) + 0.4f * (1.0f - smooth(1.0f, 1.1f, rho)));
                        continue;
                    }
                    body++;
                    float th = std::atan2(py, px), ct = std::cos(th), st = std::sin(th);
                    V3 n;
                    if (rho > 0.8f) {                /* the rounded rim */
                        float a = 0.15f + 1.25f * (rho - 0.8f) / 0.2f;
                        n = norm3({std::sin(a) * ct + tiltx, std::sin(a) * st + tilty, std::cos(a)});
                    } else                           /* the top, dished a little */
                        n = norm3({-0.16f * rho / 0.8f * ct + tiltx, -0.16f * rho / 0.8f * st + tilty, 1.0f});
                    float diff = juce::jmax(0.0f, dot3(n, L));
                    float grain = 0.75f + 0.5f * h01(x * 2 + sx, y * 2 + sy, seed);
                    float spec = 0.30f * std::pow(juce::jmax(0.0f, dot3(n, H)), 22.0f) * grain;
                    float v = 0.03f + 0.075f * diff + spec + 0.05f * std::pow(1.0f - n.z, 3.0f);
                    r += v;
                    gg += v * 1.01f;
                    b += v * 1.04f;
                }
            float cov = (float)body / 4.0f, sa = sh / 4.0f;
            if (body > 0) {
                float a = cov + sa * (1.0f - cov), k = cov / a;
                put(bd, x, y, r / (float)body * k, gg / (float)body * k, b / (float)body * k, a);
            } else if (sa > 0.002f)
                put(bd, x, y, 0, 0, 0, sa);
        }
    juce::Graphics g(img);
    if (rnd.next() < 0.5f) {                     /* dust in its dish */
        float a = rnd.range(0.0f, 6.28f), rr = rnd.range(0.0f, 0.7f) * R, s = rnd.range(0.3f, 0.7f) * sc;
        g.setColour(juce::Colour::fromFloatRGBA(0.75f, 0.74f, 0.72f, 0.75f));
        g.fillEllipse(c + std::cos(a) * rr - s, c + std::sin(a) * rr - s, 2 * s, 2 * s);
    }
    return img;
}

/* ================================================================================ the button === */
juce::Image renderButton(float d, uint32_t seed, float sc, bool lit)
{
    const float m = shadowMargin(d) + (lit ? d * 0.6f : 0.0f), full = d + 2.0f * m;
    const int S = juce::jmax(4, (int)std::ceil(full * sc));
    juce::Image img(juce::Image::ARGB, S, S, true);
    juce::Image::BitmapData bd(img, juce::Image::BitmapData::writeOnly);
    const float c = (float)S * 0.5f, R = d * 0.5f * sc;
    Rng rnd(seed);
    /* each its own white (a touch warmer or cooler, older) */
    const float age = rnd.range(0.0f, 1.0f), wr = 0.965f + 0.01f * age, wg = 0.965f + 0.004f * age, wb = 0.975f - 0.03f * age;
    for (int y = 0; y < S; y++)
        for (int x = 0; x < S; x++) {
            float r = 0, g = 0, b = 0, sh = 0, glow = 0;
            int body = 0;
            for (int sy = 0; sy < 2; sy++)
                for (int sx = 0; sx < 2; sx++) {
                    float px = ((float)x + 0.25f + 0.5f * (float)sx - c) / R, py = ((float)y + 0.25f + 0.5f * (float)sy - c) / R;
                    float rho = std::sqrt(px * px + py * py);
                    if (rho > 1.0f) {
                        /* its bezel's dark ring, its shadow; lit, the red light around it */
                        float ring = rho < 1.12f ? 0.85f : 0.0f;
                        sh += clamp01(juce::jmax(ring, dropShadow(px, py, 0.08f, 0.14f, 0.45f, 0.6f)));
                        if (lit)
                            glow += 0.55f * std::exp(-(rho - 1.0f) * 2.6f);
                        continue;
                    }
                    body++;
                    V3 n = norm3({px * 0.85f, py * 0.85f, std::sqrt(juce::jmax(0.0f, 1.0f - 0.72f * rho * rho))});
                    float wrap = (dot3(n, L) + 0.6f) / 1.6f;
                    float spec = 0.55f * std::pow(juce::jmax(0.0f, dot3(n, H)), 70.0f) + 0.1f * std::pow(juce::jmax(0.0f, dot3(n, H)), 8.0f);
                    float sss = 0.12f * (1.0f - rho * rho);   /* (light through it) */
                    if (!lit) {
                        float v = 0.42f + 0.48f * wrap + sss;
                        r += v * wr + spec;
                        g += v * wg + spec;
                        b += v * wb + spec;
                    } else {
                        float e = 0.75f + 0.35f * (1.0f - rho * rho);
                        r += juce::jmin(1.0f, 0.55f + 0.5f * e) * (0.75f + 0.25f * wrap) + spec;
                        g += (0.08f + 0.14f * e) * (0.75f + 0.25f * wrap) + spec * 0.9f;
                        b += (0.12f + 0.16f * e) * (0.75f + 0.25f * wrap) + spec * 0.9f;
                    }
                }
            float cov = (float)body / 4.0f, sa = sh / 4.0f, ga = glow / 4.0f;
            if (body > 0) {
                float a = cov + sa * (1.0f - cov), k = cov / a;
                put(bd, x, y, r / (float)body * k, g / (float)body * k, b / (float)body * k, a);
            } else if (lit && ga > 0.003f && sa < 0.5f) {
                put(bd, x, y, 1.0f, 0.12f, 0.2f, ga * (1.0f - sa));
            } else if (sa > 0.002f)
                put(bd, x, y, 0, 0, 0, sa);
        }
    return img;
}

/* ================================================================================ the panel === */
juce::Image renderPanelFace(float w, float h, float sc, const std::vector<juce::Point<float>> &hot)
{
    const int W = juce::jmax(4, (int)std::ceil(w * sc)), Hh = juce::jmax(4, (int)std::ceil(h * sc));
    juce::Image img(juce::Image::RGB, W, Hh, false);
    juce::Image::BitmapData bd(img, juce::Image::BitmapData::writeOnly);
    /* the brushed grain: each row its own streaks, long along x */
    const WearGrid wg(w, h, hot, 70.0f, 55.0f, true);
    std::vector<float> row((size_t)W), tmp((size_t)W);
    const int blur = juce::jmax(2, (int)(14.0f * sc));
    const float edge = 3.0f * sc, rad = 10.0f * sc;
    for (int y = 0; y < Hh; y++) {
        for (int x = 0; x < W; x++)
            tmp[(size_t)x] = h01(x, y, 991u) - 0.5f;
        /* (a running mean: the streaks) */
        float acc = 0.0f;
        for (int x = -blur; x < W + blur; x++) {
            int xi = juce::jlimit(0, W - 1, x);
            acc += tmp[(size_t)xi];
            int xo = x - 2 * blur;
            if (xo >= -blur)
                acc -= tmp[(size_t)juce::jlimit(0, W - 1, xo)];
            int at = x - blur;
            if (at >= 0 && at < W)
                row[(size_t)at] = acc / (float)(2 * blur);
        }
        float uy = (float)y / sc;
        for (int x = 0; x < W; x++) {
            float ux = (float)x / sc;
            /* outside the rounded face: the dark gap of the case */
            float ex = juce::jmax(0.0f, juce::jmax(edge + rad - (float)x, (float)x - ((float)W - edge - rad)));
            float ey = juce::jmax(0.0f, juce::jmax(edge + rad - (float)y, (float)y - ((float)Hh - edge - rad)));
            float dist = std::sqrt(ex * ex + ey * ey) - rad;
            if (dist > 0.5f) {
                bd.setPixelColour(x, y, juce::Colour(0xff040405));
                continue;
            }
            /* the light: brightest at the upper left, falling off; a faint sheen across */
            float lx = ux / w, ly = uy / h;
            float light = 1.0f - 0.32f * std::sqrt(lx * lx * 0.6f + ly * ly);
            float sheen = 0.010f * std::exp(-std::pow((lx * 0.8f + ly - 0.55f) * 3.0f, 2.0f));
            float grain = row[(size_t)x] * 4.5f + (h01(x, y, 17u) - 0.5f) * 0.6f;   /* streaks, and the sand under them */
            float mottle = (fbm(ux * 0.012f, uy * 0.012f, 404u, 3) - 0.5f) * 0.012f;   /* (anodising is never quite even) */
            float v = (0.062f + mottle) * light + sheen + grain * 0.0062f;
            /* where hands go: the grain polished flatter, a little lighter */
            float wear = wg.at(ux, uy);
            float wn = fbm(ux * 0.05f, uy * 0.05f, 55u, 3);
            v += wear * (0.010f + 0.012f * wn) - wear * grain * 0.002f;
            /* the edge: the anodising rubbed through to the bright metal at the corners and along the rim */
            float rim = smooth(-4.0f * sc, 0.0f, dist);
            float rub = rim * smooth(0.45f, 0.75f, fbm(ux * 0.08f, uy * 0.08f, 812u, 4));
            float r = v, g = v * 1.005f, b = v * 1.035f;
            r += rub * 0.30f;
            g += rub * 0.30f;
            b += rub * 0.31f;
            /* the edge's bevel */
            r += rim * 0.05f * (1.0f - ly);
            g += rim * 0.05f * (1.0f - ly);
            b += rim * 0.055f * (1.0f - ly);
            float aa = clamp01(0.5f - dist);
            bd.setPixelColour(x, y, juce::Colour::fromFloatRGBA(clamp01(r) * aa + 0.016f * (1.0f - aa), clamp01(g) * aa + 0.016f * (1.0f - aa),
                                                                clamp01(b) * aa + 0.018f * (1.0f - aa), 1.0f));
        }
    }
    return img;
}

void wearPrint(juce::Image &layer, float sc, const std::vector<juce::Point<float>> &hot, uint32_t seed)
{
    juce::Image::BitmapData bd(layer, juce::Image::BitmapData::readWrite);
    const WearGrid wg((float)layer.getWidth() / sc, (float)layer.getHeight() / sc, hot, 48.0f, 40.0f, false);
    for (int y = 0; y < layer.getHeight(); y++)
        for (int x = 0; x < layer.getWidth(); x++) {
            auto c = bd.getPixelColour(x, y);
            if (c.getAlpha() == 0)
                continue;
            float ux = (float)x / sc, uy = (float)y / sc;
            float wear = wg.at(ux, uy);
            /* the print: a little thin everywhere (the screen's mesh), worn through in spots, more where touched */
            float n = fbm(ux * 0.35f, uy * 0.35f, seed, 4);
            float fine = h01(x, y, seed + 9u);
            float n2 = fbm(ux * 1.4f, uy * 1.4f, seed + 3u, 3);
            float keep = 1.0f - 0.05f * fine - smooth(0.74f - 0.08f * wear, 0.80f - 0.05f * wear, n) * smooth(0.55f, 0.7f, n2) * (0.35f + 0.5f * wear);
            if (fine > 0.9993f)
                keep *= 0.35f;                     /* (pinholes) */
            keep = clamp01(keep);
            bd.setPixelColour(x, y, c.withAlpha(c.getFloatAlpha() * keep));
        }
}

void addFingerprints(juce::Image &img, float sc, const std::vector<juce::Point<float>> &where, uint32_t seed)
{
    juce::Image::BitmapData bd(img, juce::Image::BitmapData::readWrite);
    Rng rnd(seed);
    for (auto &p : where) {
        /* a print: about 15 x 20 mm (the panel is 375 mm across: 4.75 units a mm); its ridges half a mm apart */
        float a = rnd.range(-0.9f, 0.9f), sw = rnd.range(55.0f, 80.0f), sh = sw * rnd.range(1.15f, 1.4f);
        float strength = rnd.range(0.05f, 0.095f);
        float ridges = sw / 2.4f / 2.0f;
        uint32_t ps = (uint32_t)(rnd.next() * 100000.0f);
        int x0 = juce::jmax(0, (int)((p.x - sh) * sc)), x1 = juce::jmin(img.getWidth() - 1, (int)((p.x + sh) * sc));
        int y0 = juce::jmax(0, (int)((p.y - sh) * sc)), y1 = juce::jmin(img.getHeight() - 1, (int)((p.y + sh) * sc));
        for (int y = y0; y <= y1; y++)
            for (int x = x0; x <= x1; x++) {
                float ux = (float)x / sc - p.x, uy = (float)y / sc - p.y;
                float u = (ux * std::cos(a) + uy * std::sin(a)) / (sw * 0.5f), v = (-ux * std::sin(a) + uy * std::cos(a)) / (sh * 0.5f);
                float f = printAt(u, v, ps, ridges);
                if (f <= 0.0f)
                    continue;
                auto c = bd.getPixelColour(x, y);
                /* grease on black: a pale haze, a hint of the oil's colours */
                float k = f * strength;
                float hue = 0.5f + 0.5f * std::sin((u * 3.0f + v * 2.0f) * 3.1f);
                float r = c.getFloatRed() + k * (0.85f + 0.1f * hue), g = c.getFloatGreen() + k * 0.88f, b = c.getFloatBlue() + k * (0.95f - 0.08f * hue);
                bd.setPixelColour(x, y, juce::Colour::fromFloatRGBA(clamp01(r), clamp01(g), clamp01(b), 1.0f));
            }
    }
}

void drawScratches(juce::Graphics &g, juce::Rectangle<float> area, int n, uint32_t seed)
{
    Rng rnd(seed);
    for (int i = 0; i < n; i++) {
        float x = rnd.range(area.getX(), area.getRight()), y = rnd.range(area.getY(), area.getBottom());
        float a = rnd.range(-0.5f, 0.5f) + (rnd.next() < 0.3f ? 1.57f : 0.0f);
        float l = rnd.range(8.0f, 70.0f);
        juce::Path p;
        p.startNewSubPath(x, y);
        float bend = rnd.range(-0.05f, 0.05f);
        for (int k = 1; k <= 6; k++) {
            float t = (float)k / 6.0f;
            float aa = a + bend * t * 6.0f;
            p.lineTo(x + std::cos(aa) * l * t, y + std::sin(aa) * l * t);
        }
        g.setColour(juce::Colours::white.withAlpha(rnd.range(0.025f, 0.09f)));
        g.strokePath(p, juce::PathStrokeType(rnd.range(0.35f, 0.8f)));
        /* (its dark side: a scratch is a groove) */
        g.setColour(juce::Colours::black.withAlpha(0.15f));
        g.strokePath(p, juce::PathStrokeType(0.3f), juce::AffineTransform::translation(0.4f, 0.4f));
    }
}

void drawDust(juce::Graphics &g, juce::Rectangle<float> area, int n, uint32_t seed, float sc)
{
    Rng rnd(seed);
    const float px = 1.0f / sc;
    for (int i = 0; i < n; i++) {
        float x = rnd.range(area.getX(), area.getRight()), y = rnd.range(area.getY(), area.getBottom());
        float s = juce::jmax(px * 0.6f, rnd.range(0.25f, 1.1f) * (rnd.next() < 0.1f ? 1.8f : 1.0f));
        float a = rnd.range(0.25f, 0.8f);
        g.setColour(juce::Colours::black.withAlpha(0.3f * a));
        g.fillEllipse(x - s + s * 0.4f, y - s + s * 0.6f, 2 * s, 2 * s);
        float tone = rnd.range(0.6f, 0.85f);
        g.setColour(juce::Colour::fromFloatRGBA(tone, tone * 0.98f, tone * 0.95f, a));
        g.fillEllipse(x - s, y - s, 2 * s, 2 * s);
    }
    /* a few fibres */
    for (int i = 0; i < n / 60 + 1; i++) {
        float x = rnd.range(area.getX(), area.getRight()), y = rnd.range(area.getY(), area.getBottom());
        juce::Path p;
        p.startNewSubPath(x, y);
        float a = rnd.range(0.0f, 6.28f), l = rnd.range(4.0f, 14.0f);
        p.cubicTo(x + std::cos(a) * l * 0.4f + rnd.range(-3.0f, 3.0f), y + std::sin(a) * l * 0.4f + rnd.range(-3.0f, 3.0f),
                  x + std::cos(a) * l * 0.7f + rnd.range(-3.0f, 3.0f), y + std::sin(a) * l * 0.7f + rnd.range(-3.0f, 3.0f), x + std::cos(a) * l,
                  y + std::sin(a) * l);
        g.setColour(juce::Colour::fromFloatRGBA(0.7f, 0.7f, 0.68f, rnd.range(0.2f, 0.4f)));
        g.strokePath(p, juce::PathStrokeType(rnd.range(0.25f, 0.45f)));
    }
}

juce::Image renderSmudge(float w, float h, float sc, uint32_t seed)
{
    const int W = juce::jmax(2, (int)std::ceil(w * sc)), Hh = juce::jmax(2, (int)std::ceil(h * sc));
    juce::Image img(juce::Image::ARGB, W, Hh, true);
    juce::Image::BitmapData bd(img, juce::Image::BitmapData::writeOnly);
    Rng rnd(seed);
    const float cx = rnd.range(0.25f, 0.75f) * w, cy = rnd.range(0.35f, 0.75f) * h, a = rnd.range(-0.6f, 0.6f);
    for (int y = 0; y < Hh; y++)
        for (int x = 0; x < W; x++) {
            float ux = (float)x / sc - cx, uy = (float)y / sc - cy;
            float u = (ux * std::cos(a) + uy * std::sin(a)) / 34.0f, v = (-ux * std::sin(a) + uy * std::cos(a)) / 44.0f;
            float f = printAt(u, v, seed, 14.0f);
            /* and a wipe across the glass */
            float wipe = 0.35f * smooth(0.55f, 0.8f, fbm((float)x / sc * 0.04f, (float)y / sc * 0.3f, seed + 4u, 3));
            float al = juce::jmax(f * 0.16f, wipe * 0.05f);
            if (al > 0.002f)
                put(bd, x, y, 0.85f, 0.88f, 0.95f, al);
        }
    return img;
}

void drawSlotReal(juce::Graphics &g, float x, float top, float bottom)
{
    using namespace juce;
    auto r = Rectangle<float>(x - 6.5f, top - 20.0f, 13.0f, bottom - top + 40.0f);
    /* the cut in the panel: its upper edge in shadow, its lower edge catching the light */
    g.setColour(Colours::white.withAlpha(0.10f));
    g.drawRoundedRectangle(r.translated(0.0f, 0.8f), 3.5f, 1.0f);
    g.setColour(Colour(0xff010101));
    g.fillRoundedRectangle(r, 3.5f);
    /* inside, deep down: the slider's metal, barely lit */
    auto in = r.reduced(4.0f, 8.0f);
    g.setGradientFill(ColourGradient(Colour(0xff0a0a0b), in.getX(), in.getY(), Colour(0xff1d1d20), in.getRight(), in.getY(), false));
    g.fillRect(in);
    g.setColour(Colours::black.withAlpha(0.8f));
    g.drawLine(r.getX() + 1.0f, r.getY() + 3.0f, r.getX() + 1.0f, r.getBottom() - 3.0f, 1.4f);
}
} // namespace hmui
