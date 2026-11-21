/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 Ziao */
#include "synthesis.h"
#include "fastmath.h"
#include "oscillator.h"

#ifndef PLAT_HOST
__attribute__((section(".data"), aligned(16)))
#endif
const float slap_sine_table[SLAP_SINE_SIZE + 1] = {
#include "slap_sine_table.inc"
};

const char *const slap_engine_names[SLAP_ENGINES] = {
    "OP FM", "VIRTUAL ANALOG", "WAVETABLE", "PHASE DIST",
    "PHYSICAL PLUCK", "BOWED STRING", "KARPLUS STRONG", "ADDITIVE",
    "PHASE MOD", "RING MOD", "WAVEFOLD", "SYNC", "SUPERSAW", "ANALOG BASS",
    "ACID", "ELECTRIC PIANO", "ACOUSTIC PIANO", "ORGAN", "FORMANT CHOIR",
    "BRASS", "WOODWIND", "FLUTE", "BELLS", "GRAIN CLOUD", "SPECTRAL"
};

#if !defined(PLAT_HOST)
#define SLAP_SYNTH_SRAM __attribute__((section(".c1_text.slap_synthesis")))
#define SLAP_SYNTH_INLINE __attribute__((always_inline))
#else
#define SLAP_SYNTH_SRAM
#define SLAP_SYNTH_INLINE
#endif

static inline SLAP_SYNTH_INLINE float wrap(float phase)
{
    return phase - (float)(int)phase + (phase < 0.0f && phase != (float)(int)phase ? 1.0f : 0.0f);
}

static inline SLAP_SYNTH_INLINE float wrap_advance(float phase)
{
    if (phase < 1.0f) return phase;
    // The higher partials can advance by more than a full cycle at high notes.
    return phase - (float)(int)phase;
}

static inline SLAP_SYNTH_INLINE float synth_abs(float value)
{
    fm_bits_t bits = {.f = value};
    bits.u &= 0x7fffffffu;
    return bits.f;
}

static SLAP_SYNTH_SRAM float sine(float phase) { return slap_oscillator(phase); }
static SLAP_SYNTH_SRAM float tri(float phase) { return 1.0f - 4.0f * synth_abs(wrap(phase) - 0.5f); }
static SLAP_SYNTH_SRAM float limit(float v) { return v / (1.0f + synth_abs(v)); }

static inline SLAP_SYNTH_INLINE float noise_sample(SlapSynth *s)
{
    uint32_t x = s->rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s->rng = x;
    return (float)(x & 65535u) * (1.0f / 32768.0f) - 1.0f;
}

static SLAP_SYNTH_SRAM float blep(float phase, float delta)
{
    if (phase < delta) {
        float t = phase / delta;
        return 2.0f * t - t*t - 1.0f;
    }
    if (phase > 1.0f - delta) {
        float t = (phase - 1.0f) / delta;
        return t*t + 2.0f*t + 1.0f;
    }
    return 0.0f;
}

static SLAP_SYNTH_SRAM float saw(float phase, float delta)
{
    return 2.0f * phase - 1.0f - blep(phase, delta);
}

static SLAP_SYNTH_SRAM float pulse(float phase, float delta, float width)
{
    return (phase < width ? 1.0f : -1.0f) + blep(phase, delta) - blep(wrap(phase - width), delta);
}

float slap_pitch_increment(unsigned pitch)
{
    if (pitch > 127u) pitch = 127u;
    return (440.0f / 44100.0f) * fm_exp2f(((float)pitch - 69.0f) / 12.0f);
}

void slap_synth_start(SlapSynth *s, int16_t *string, unsigned engine, unsigned pitch,
                      unsigned color, unsigned motion, uint32_t seed, int modern)
{
    s->phase = s->second = s->third = s->slow = s->pole = s->feedback = 0.0f;
    s->strike = 1.0f;
    s->inc = slap_pitch_increment(pitch);
    s->color = color * (1.0f / 127.0f);
    s->motion = motion * (1.0f / 127.0f);
    s->rng = seed | 1u;
    s->engine = (uint8_t)engine;
    s->modern = (uint8_t)modern;
    s->string = string;
    switch (engine) {
    case 2: case 5: case 18: case 21: case 23: case 24:
        s->slow_inc = (0.19f + s->motion * 2.2f) * (1.0f / 44100.0f);
        break;
    default:
        s->slow_inc = 0.0f;
        break;
    }
    if (engine == 4 || engine == 6) {
        unsigned period = (unsigned)(1.0f / s->inc + 0.25f);
        s->length = period > SLAP_STRING_CAPACITY ? SLAP_STRING_CAPACITY :
                    period < 2 ? 2 : (uint16_t)period;
        s->index = 0;
        s->pickup = modern ? (unsigned)(s->length * (0.11f + 0.14f * s->color)) : s->length / 5u;
        float excitation = 0.0f;
        for (unsigned j = 0; j < s->length; ++j) {
            excitation += (noise_sample(s) - excitation) * (0.16f + s->color * 0.7f);
            s->string[j] = (int16_t)(excitation * 26000.0f);
        }
    }
}

static SLAP_SYNTH_SRAM float string_sample(SlapSynth *s, int pickup)
{
    unsigned i = s->index, n = i + 1u, length = s->length;
    if (n == length) n = 0;
    float a = s->string[i], b = s->string[n];
    float damp = 0.08f + (1.0f - s->color) * 0.36f;
    s->string[i] = (int16_t)((a + (b - a) * damp) * (0.980f + 0.019f * s->motion));
    s->index = (uint16_t)n;
    if (pickup) {
        unsigned tap = i + s->pickup;
        if (tap >= length) tap -= length;
        a -= s->string[tap] * 0.70f;
        s->pole += (a - s->pole) * 0.08f;
        a = a * 0.75f + s->pole * 0.25f;
    }
    return a * (1.0f / 32768.0f);
}

static SLAP_SYNTH_SRAM float additive(float p, float brightness)
{
    return (sine(p) + brightness * 0.53f * sine(2*p) +
            brightness * brightness * (0.28f * sine(3*p) + 0.16f * sine(5*p))) * 0.54f;
}

static const int8_t wavetable[3][4][32] = {
#include "wavetable.inc"
};

static inline SLAP_SYNTH_INLINE float table_frame(const int8_t *frame, unsigned index, float fraction)
{
    float a = frame[index], b = frame[(index + 1u) & 31u];
    return (a + (b - a) * fraction) * 0.009f;
}

SLAP_SYNTH_SRAM float slap_synth_sample(SlapSynth *s)
{
    const float p = s->phase, d = s->inc, c = s->color, m = s->motion;
    const float x = s->second, y = s->third, t = s->slow;
    float out = 0.0f;
    float next_x = x, next_y = y;
    const float next_p = p + d;
    if (s->slow_inc && !(s->engine == 0u && s->modern >= 2u)) {
        float next = t + s->slow_inc;
        s->slow = wrap_advance(next);
    }

    switch (s->engine) {
    case 0: {
        if (s->modern >= 2u) {
            /* The first three phases use the common oscillator state. This
             * engine owns the remaining phase fields and feedback history. */
            unsigned algorithm = m < 0.34f ? 0u : m < 0.68f ? 1u : 2u;
            float attack = s->strike;
            float six = sine(s->pole + s->feedback * (0.022f + 0.11f * c));
            float five = sine(s->slow_inc + six * (0.10f + 0.23f * m) * attack);
            float four = sine(t + five * (algorithm == 0u ? 0.35f : 0.11f));
            float three = sine(y + (algorithm == 1u ? six : four) * c * attack * 0.24f);
            float two = sine(x + three * (0.12f + 0.31f * c));
            float one = sine(p + two * (0.035f + 0.25f * c) + (algorithm == 2u ? five * 0.09f : 0.0f));
            out = algorithm == 0u ? one * 0.78f :
                one * 0.65f + (algorithm == 1u ? four : three) * (0.08f + 0.14f * attack);
            s->feedback = six;
            s->pole = wrap_advance(s->pole + d * (algorithm == 2u ? 5.01f : 3.002f));
            s->slow_inc = wrap_advance(s->slow_inc + d * (algorithm == 1u ? 2.004f : 1.503f));
            s->slow = wrap_advance(t + d * (algorithm == 0u ? 4.0f : 2.0f));
            next_x = x + d * (algorithm == 2u ? 2.001f : 1.0f);
            next_y = y + d * (algorithm == 0u ? 2.0f : 3.02f);
            s->strike *= 0.99962f + 0.00032f * m;
            break;
        }
        float op4 = sine(y + 0.15f * c * s->feedback);
        float op3 = sine(2.0f * p + op4 * m * 0.22f);
        float op2 = sine(x + op3 * c * s->strike * 0.34f);
        if (s->modern) {
            // Two carriers. The bright pair falls away faster than the body.
            float transient = s->strike * s->strike;
            float upper_mod = sine(2.0f * y + 0.24f * m * op4);
            float upper = sine(2.0f * p + (0.025f + 0.25f * c * transient) * upper_mod);
            float body = sine(p + op2 * (0.025f + 0.27f * c) + 0.07f * m * op4);
            out = (0.70f + 0.05f * (1.0f - c)) * body + (0.04f + 0.13f * c * transient) * upper;
        } else {
            out = 0.8f * sine(p + op2 * (0.02f + 0.34f*c) + m * 0.055f * op4);
        }
        s->feedback = op4;
        if (s->modern) {
            // Motion selects the operator ratios and controls modulation decay.
            next_x = x + d * (m < 0.22f ? 1.0f : m < 0.48f ? 2.0f : m < 0.69f ? 3.5f : 5.0f);
            next_y = y + d * (m < 0.40f ? 1.0f : m < 0.69f ? 2.01f : 7.02f);
            s->strike *= 0.99962f + 0.00034f * m;
        } else {
            next_x = x + d * (m > 0.60f ? 3.0f : 2.0f);
            next_y = y + d * (m > 0.29f ? 3.01f : 1.0f);
            s->strike *= 0.99991f;
        }
        break;
    }
    case 1:
        s->pole += (0.65f * saw(p, d) + 0.35f * pulse(x, d, 0.22f + 0.33f*m) - s->pole) *
                   (0.025f + 0.36f*c);
        out = s->pole;
        next_x = x + d * 1.005f;
        break;
    case 2: {
        if (s->modern >= 2u) {
            float scan = fm_clampf(c * 2.74f + m * (0.12f + 0.25f * sine(t)), 0.0f, 2.999f);
            unsigned frame = (unsigned)scan;
            unsigned band = d < 0.059f ? 0u : d < 0.137f ? 1u : 2u;
            float phase = p * 32.0f;
            unsigned index = (unsigned)phase & 31u;
            float fraction = phase - (unsigned)phase;
            float a = table_frame(wavetable[band][frame], index, fraction);
            float b = table_frame(wavetable[band][frame + 1u], index, fraction);
            out = a + (b - a) * (scan - frame);
            break;
        }
        float morph = c * 0.76f + m * (0.12f + 0.12f*sine(t));
        float a = sine(p), b = 0.62f * a + 0.28f * sine(3*p) + 0.10f * sine(5*p);
        if (morph < 0.5f) {
            out = a + (b-a)*morph*2.0f;
        } else {
            float z = 0.40f * a + 0.38f * sine(2*p) + 0.22f * sine(7*p);
            out = b + (z-b)*(2.0f*morph-1.0f);
        }
        break;
    }
    case 3: {
        float knee = 0.11f + 0.38f * (1.0f - c);
        float warped = p < knee ? 0.5f*p/knee : 0.5f + 0.5f*(p-knee)/(1.0f-knee);
        out = 0.74f * sine(warped) + 0.26f * m * sine(2.0f*warped);
        break;
    }
    case 4:
        out = 1.6f * string_sample(s, 1);
        break;
    case 5: { /* Nonlinear bow force feeding a damped body. */
        float side = saw(p, d);
        float vibrato = sine(t) * 0.006f * m;
        float slip = limit((side * 0.9f - s->feedback) * (1.4f + 4.0f*c));
        s->feedback += (slip - s->feedback) * (0.15f + 0.22f*c);
        s->pole += (side - s->pole) * (0.08f + 0.3f*c);
        out = 0.41f * s->pole + 0.36f * sine(x + vibrato) + 0.26f * s->feedback;
        next_x = x + d * 0.997f;
        break;
    }
    case 6:
        out = 1.9f * string_sample(s, 0);
        break;
    case 7:
        out = additive(p, c) + 0.12f*m*sine(p*4);
        break;
    case 8:
        next_x = x + d * (m > 0.58f ? 1.4983f : 0.5f);
        out = 0.83f * sine(p + (0.08f + c*0.29f) * sine(x));
        break;
    case 9:
        next_x = x + d * (m > 0.48f ? 2.4142f : 0.503f);
        out = sine(p) * ((1.0f-c)*0.45f + c*sine(x));
        break;
    case 10:
        out = 0.76f * tri(0.25f + sine(p) * (0.27f + 0.85f*c) + 0.19f*m*sine(2*p));
        break;
    case 11:
        next_x = next_p >= 1.0f ? (next_p - 1.0f)*(1.5f+3.0f*c) : x+d*(1.5f+3.0f*c);
        out = 0.72f*sine(x) + 0.28f*(1.0f-m)*sine(p);
        s->pole += (out - s->pole) * 0.36f;
        out = s->pole;
        break;
    case 12: {
        float spread = 0.002f + c*0.022f;
        next_x = x + d*(1.0f+spread);
        next_y = y + d*(1.0f-spread*1.31f);
        if (s->modern) {
            float sides = saw(x,d) + saw(y,d);
            float body = 0.24f*saw(p,d) + (0.31f - 0.15f*c)*sides + (0.37f - 0.10f*c)*tri(p);
            s->pole += (body - s->pole) * (0.14f + 0.47f*c);
            out = s->pole;
        } else {
            out = 0.23f*saw(p,d) + 0.24f*saw(x,d) + 0.24f*saw(y,d) + 0.23f*tri(p);
        }
        break;
    }
    case 13:
        if (s->modern) {
            float sub = sine(x);
            float main = sine(p + (0.012f + 0.15f * m * s->strike) * sub);
            float edge = pulse(p, d, 0.21f + 0.27f * m);
            s->pole += (edge - s->pole) * (0.033f + 0.32f * c * s->strike);
            out = (0.56f - 0.16f * c) * main + (0.17f + 0.25f * m) * sub +
                  (0.05f + 0.36f * c) * s->pole;
            next_x = x + d * 0.5f;
            s->strike *= 0.9994f + 0.00053f * m;
            break;
        }
        next_x = x + d*0.5f;
        out = 0.51f*sine(p) + 0.22f*sine(x) + 0.22f*c*pulse(p,d,0.35f+0.12f*m);
        break;
    case 14: {
        float edge = blep(p, d);
        float saw_input = 2.0f * p - 1.0f - edge;
        float pulse_input = (p < 0.50f ? 1.0f : -1.0f) + edge - blep(wrap(p - 0.50f), d);
        float input = 0.72f * saw_input + 0.25f * c * pulse_input;
        float cutoff = 0.016f + 0.30f*c*s->strike;
        s->pole += cutoff*(input - s->pole - s->feedback*(0.55f+1.5f*m));
        s->feedback += cutoff*(s->pole - s->feedback);
        s->strike *= 0.99982f;
        out = limit(2.1f*s->feedback);
        break;
    }
    case 15:
        if (s->modern) {
            float tine = sine(x);
            float body = sine(p + tine * (0.03f + c * (0.04f + 0.29f * s->strike)));
            float reed = sine(2.0f * p + 0.11f * tine);
            next_x = x + d * (m < 0.25f ? 1.0f : m < 0.55f ? 2.0f : m < 0.77f ? 7.003f : 3.002f);
            out = 0.67f * body + (0.04f + 0.17f * c) * reed +
                  (0.03f + 0.16f * c) * s->strike * tine;
            s->strike *= 0.99945f + 0.00041f * m;
            break;
        }
        next_x = x + d*7.002f;
        out = 0.65f*sine(p+0.23f*c*s->strike*sine(2*p)) + 0.20f*s->strike*sine(x);
        s->strike *= 0.99972f + 0.00017f*m;
        break;
    case 16:
        if (s->modern) {
            next_x = x + d * (2.003f + 0.021f * c);
            next_y = y + d * ((m < 0.44f ? 4.0f : 3.0f) + 0.012f + 0.045f * c);
            float hammer = s->strike * s->strike;
            s->pole += (noise_sample(s) - s->pole) * (0.12f + 0.30f*c);
            out = 0.59f * sine(p) * (0.32f + 0.19f * m + 0.49f * s->strike) +
                  (0.055f + 0.07f * m + 0.21f * c * s->strike) * sine(x) +
                  (0.015f + 0.24f * c * hammer) * sine(y) +
                  (0.05f + 0.14f * c) * hammer * s->pole;
        } else {
            next_x = x + d*2.007f;
            next_y = y + d*3.028f;
            out = (0.55f*sine(p)+0.24f*c*sine(x)+0.16f*c*sine(y) +
                   0.12f*s->strike*noise_sample(s)) * (0.42f+0.58f*s->strike);
        }
        s->strike *= 0.99969f + 0.00023f*m;
        break;
    case 17:
        next_x = x + d*0.5f;
        out = 0.43f*sine(p)+0.23f*sine(x)+0.12f*sine(3*p) +
              0.20f*c*sine(4*p)+0.15f*m*sine(6*p);
        break;
    case 18: {
        if (s->modern) {
            float movement = sine(t) * (0.04f + 0.19f * m);
            float vowel = c + movement;
            out = 0.39f * sine(p) + (0.10f + 0.31f * vowel) * sine(2.0f * p) +
                  (0.32f - 0.24f * vowel) * sine(3.0f * p) +
                  (0.04f + 0.20f * c) * sine((c > 0.54f ? 7.0f : 4.0f) * p) +
                  0.16f * sine(x + 0.10f * movement);
            next_x = x + d * (0.997f + 0.018f * m);
            break;
        }
        float vowel = c + 0.13f*sine(t);
        out = 0.42f*sine(p) + (0.18f+0.13f*vowel)*sine(3*p) +
              (0.20f-0.14f*vowel)*sine(5*p) + 0.17f*sine(x);
        next_x = x + d*(1.001f+0.003f*m);
        break;
    }
    case 19:
        s->pole += (saw(p,d) - s->pole) * (0.055f + 0.5f*c*(1.0f-s->strike));
        s->strike *= 0.99972f;
        out = 0.67f*s->pole + 0.28f*sine(p);
        break;
    case 20:
        if (s->modern) {
            // Narrow doubles favor oboe-like upper partials. The broad single
            // reed keeps the odd harmonics of a more cylindrical bore.
            float width = m < 0.27f ? 0.49f : m < 0.36f ? 0.24f : m > 0.71f ? 0.36f : 0.43f - 0.20f * c;
            float bore = pulse(p, d, width);
            float breath = noise_sample(s);
            s->pole += (bore - s->pole) * (0.045f + 0.31f*c);
            s->feedback += (s->pole - s->feedback) * (0.035f + 0.12f*m);
            float air = (0.014f + 0.07f * s->strike) * m * breath;
            out = (0.60f - 0.17f * c) * s->pole +
                  (0.12f + 0.34f * c) * (s->pole - s->feedback) +
                  (0.18f + 0.21f * m) * sine(p) + air;
            s->strike *= 0.9987f;
        } else {
            s->pole += (pulse(p,d,0.48f-c*0.09f)-s->pole)*(0.05f+0.30f*c);
            out = 0.65f*s->pole + 0.23f*sine(p) + 0.06f*m*noise_sample(s);
        }
        break;
    case 21:
        s->pole += (noise_sample(s)-s->pole)*(0.015f+c*0.13f);
        if (s->modern) {
            float vibrato = 0.014f*m*(1.0f-s->strike)*sine(t);
            out = 0.76f*sine(p+vibrato) + (0.035f+0.10f*c)*sine(2*p) +
                  (0.14f+0.35f*s->strike+0.12f*c)*s->pole;
            s->strike *= 0.99955f;
        } else {
            out = 0.75f*sine(p+0.007f*m*sine(t))+0.12f*c*sine(2*p)+0.37f*s->pole;
        }
        break;
    case 22:
        next_x = x + d*2.756f;
        next_y = y + d*5.403f;
        out = 0.58f*sine(p)+(0.30f*c*sine(x)+0.15f*sine(y))*s->strike;
        s->strike *= 0.99977f + 0.00019f*m;
        break;
    case 23: {
        float grains = 0.55f * (1.0f-tri(t+0.34f)) * sine(x) +
                       0.35f * (1.0f-tri(2*t)) * sine(y);
        next_x = x + d*(0.997f + 0.037f*c);
        next_y = y + d*(1.48f + 0.27f*c);
        out = 0.28f*sine(p) + 0.46f*grains;
        break;
    }
    case 24:
        next_x = x + d*(3.07f+1.03f*c);
        next_y = y + d*(6.47f+1.71f*m);
        float overtone = sine(x);
        out = 0.38f*sine(p)+(0.18f+0.13f*sine(t))*overtone+0.18f*sine(y+0.0064f*overtone);
        break;
    default: out = sine(p); break;
    }

    // Voice oscillator advances are nonnegative. Shaping still uses signed wrap.
    s->phase = wrap_advance(next_p);
    s->second = wrap_advance(next_x);
    s->third = wrap_advance(next_y);
    return out;
}
