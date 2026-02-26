/*
 * Formant Synthesizer - C Implementation
 * Copyright (c) 2026 hotdogdevourer
 *
 * Licensed under the MIT License.
 * See the LICENSE file in the project root for full license information.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <stdint.h>
#include <time.h>

#define FS 16000
#define MAX_PHONES 1024
#define MAX_TOKENS 2048
#define WINDOW_SIZE 256
#define HOP_SIZE 64
#define CROSSFADE_MS 20
#define PI 3.14159265358979323846

typedef struct {
    float f[5];
    float voicing;
    float duration;
    float gain;
} Phoneme;

typedef struct {
    char name[8];
    float f[5];
    float voicing;
    float duration;
    float gain;
} PhonemeEntry;

typedef struct {
    char phone[8];
    float f[5];
    float voicing;
    float gain;
    float pitch;
    int samps;
    int is_plosive;
    int plosive_type;
} Target;

typedef struct {
    char *text;
    char *output;
    char *input_file;
    float f0;
    float vol_db;
    int verbose;
} Config;

typedef struct {
    float z1, z2;
} BiquadState;

static const PhonemeEntry PHONEMES[] = {
    {"IY", {270, 2290, 3010, 3500, 4500}, 1.0, 100, 2.5},
    {"IH", {390, 1990, 2550, 3500, 4500}, 1.0,  80, 2.5},
    {"EH", {530, 1840, 2480, 3500, 4500}, 1.0,  80, 2.5},
    {"AE", {660, 1720, 2410, 3500, 4500}, 1.0, 120, 2.5},
    {"AA", {730, 1090, 2440, 3500, 4500}, 1.0, 120, 2.5},
    {"AO", {570,  840, 2410, 3500, 4500}, 1.0, 120, 2.5},
    {"UW", {300,  870, 2240, 3500, 4500}, 1.0, 100, 2.5},
    {"UH", {420, 1150, 2400, 3500, 4500}, 1.0,  80, 2.5},
    {"AH", {640, 1190, 2390, 3500, 4500}, 1.0,  80, 2.5},
    {"AX", {500, 1500, 2500, 3500, 4500}, 1.0,  60, 2.0},
    {"ER", {490, 1350, 1690, 3500, 4500}, 1.0, 120, 2.5},
    {"L",  {320, 1250, 2650, 3500, 4500}, 1.0, 110, 2.3},
    {"R",  {400, 1100, 1550, 3500, 4500}, 1.0, 110, 2.3},
    {"W",  {300,  700, 2200, 3500, 4500}, 1.0,  90, 2.2},
    {"Y",  {300, 2250, 3050, 3500, 4500}, 1.0,  90, 2.2},
    {"M",  {250, 1000, 2000, 3000, 4000}, 1.0,  90, 2.1},
    {"N",  {250, 1550, 2100, 3000, 4000}, 1.0,  90, 2.1},
    {"NG", {280, 1700, 2400, 3500, 4500}, 1.0,  90, 2.1},
    {"S",  {3200, 4500, 6000, 7200, 8200}, 0.0, 100, 0.035},
    {"Z",  {400, 5200, 6500, 7500, 8500}, 0.4, 100, 0.035},
    {"SH", {2300, 3400, 4600, 5800, 7000}, 0.0, 100, 4.3},
    {"ZH", {2200, 3200, 4200, 5200, 6200}, 0.4, 100, 4.1},
    {"F",  {1400, 2600, 3800, 5000, 6500}, 0.0, 100, 5.1},
    {"V",  { 400, 2600, 3800, 5000, 6500}, 0.4, 100, 3.2},
    {"TH", {1500, 2400, 3800, 5200, 6800}, 0.0,  90, 4.9},
    {"DH", { 350, 2000, 3000, 4200, 5500}, 0.4,  90, 3.6},
    {"HH", {1100, 1700, 2700, 3800, 5200}, 0.0, 110, 2.2},
    {"CH", {2300, 3400, 4600, 5800, 7000}, 0.0,  70, 6.5},
    {"JH", { 350, 2300, 3400, 4600, 5800}, 0.3,  70, 4.6},
    {"T",  {2000, 3000, 4000, 5000, 6000}, 0.0,  25, 7.5},
    {"D",  { 300, 3000, 4000, 5000, 6000}, 0.3,  40, 4.2},
    {"P",  {1000, 2000, 3000, 4000, 5000}, 0.0,  25, 7.5},
    {"B",  { 300, 2000, 3000, 4000, 5000}, 0.3,  40, 4.2},
    {"K",  {1500, 2500, 3500, 4500, 5500}, 0.0,  25, 7.5},
    {"G",  { 300, 2500, 3500, 4500, 5500}, 0.3,  40, 4.2},
    {"ASPT", {1200, 1850, 2750, 3850, 5200}, 0.0, 80, 2.8},
    {"ASPP", { 950, 1550, 2550, 3650, 4950}, 0.0, 80, 2.6},
    {"ASPK", {1350, 2050, 2950, 4050, 5450}, 0.0, 80, 2.7},
    {"_", {500, 1500, 2500, 3500, 4500}, 0.0, 50, 0.0}
};
#define NUM_PHONEMES (sizeof(PHONEMES)/sizeof(PHONEMES[0]))

typedef struct {
    char name[8];
    char components[2][8];
    float dur_scales[2];
} DiphthongEntry;

static const DiphthongEntry DIPHTHONGS[] = {
    {"AY", {"AA", "IY"}, {1.00, 0.75}},
    {"EY", {"EH", "IY"}, {1.00, 0.80}},
    {"OY", {"AO", "IY"}, {1.00, 0.80}},
    {"AW", {"AA", "UW"}, {1.00, 0.75}},
    {"OW", {"AO", "UW"}, {1.00, 0.80}}
};
#define NUM_DIPHTHONGS (sizeof(DIPHTHONGS)/sizeof(DIPHTHONGS[0]))

static const PhonemeEntry* find_phoneme(const char *name) {
    for (size_t i = 0; i < NUM_PHONEMES; i++) {
        if (strcmp(PHONEMES[i].name, name) == 0) {
            return &PHONEMES[i];
        }
    }
    return &PHONEMES[NUM_PHONEMES - 1];
}

static const DiphthongEntry* find_diphthong(const char *name) {
    for (size_t i = 0; i < NUM_DIPHTHONGS; i++) {
        if (strcmp(DIPHTHONGS[i].name, name) == 0) {
            return &DIPHTHONGS[i];
        }
    }
    return NULL;
}

static int tokenize_arpabet(const char *input, char tokens[][16], int max_tokens) {
    int count = 0;
    const char *p = input;
    
    while (*p && count < max_tokens) {
        while (*p && !isupper(*p) && *p != '_') p++;
        if (!*p) break;
        
        int i = 0;
        while (*p && (isupper(*p) || *p == '_') && i < 15) {
            tokens[count][i++] = *p++;
        }
        if (*p && isdigit(*p) && i < 15) {
            tokens[count][i++] = *p++;
        }
        tokens[count][i] = '\0';
        count++;
    }
    return count;
}

static void make_resonator(float fc, float bw, float fs, float *b, float *a) {
    float r = expf(-PI * bw / fs);
    float omega = 2.0f * PI * fc / fs;
    
    b[0] = 1.0f - r * r;
    b[1] = 0.0f;
    b[2] = 0.0f;
    
    a[0] = 1.0f;
    a[1] = -2.0f * r * cosf(omega);
    a[2] = r * r;
}

static float biquad_df2(float x, float *b, float *a, BiquadState *s) {
    float w = x - a[1] * s->z1 - a[2] * s->z2;
    float y = b[0] * w + b[1] * s->z1 + b[2] * s->z2;
    s->z2 = s->z1;
    s->z1 = w;
    return y;
}

static float soft_clip(float x, float threshold) {
    float ax = fabsf(x);
    if (ax <= threshold) return x;
    return copysignf(threshold * (1.0f + logf(1.0f + ax / threshold)), x);
}

static float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

static void generate_plosive_burst(float *buffer, int length, int plosive_type, float fs) {
    int burst_len = (plosive_type == 1) ? (int)(0.008f * fs) :
                    (plosive_type == 2) ? (int)(0.006f * fs) :
                                          (int)(0.010f * fs);
    if (burst_len > length) burst_len = length;
    
    const float center_freq = (plosive_type == 1) ? 1800.0f : 
                              (plosive_type == 2) ? 4200.0f : 2800.0f;
    (void)center_freq;
    
    float decay = expf(-3.0f / burst_len);
    float amp = 1.0f;
    
    static float hp_xz = 0.0f, hp_yz = 0.0f;
    
    for (int i = 0; i < burst_len; i++) {
        float noise = ((float)rand() / RAND_MAX * 2.0f - 1.0f);
        
        float hp_out = noise - hp_xz + 0.85f * hp_yz;
        hp_xz = noise;
        hp_yz = hp_out;
        
        buffer[i] = hp_out * amp * 3.5f;
        amp *= decay;
    }
    
    for (int i = burst_len; i < length; i++) {
        buffer[i] = 0.0f;
    }
}

static float* synthesize(const char *arpabet, float f0, float vol_db, int *out_len) {
    Target targets[MAX_TOKENS];
    int num_targets = 0;
    
    char tokens[MAX_TOKENS][16];
    int num_tokens = tokenize_arpabet(arpabet, tokens, MAX_TOKENS);
    
    for (int t = 0; t < num_tokens && num_targets < MAX_TOKENS; t++) {
        char phone[16];
        int stress = 0;
        int i = 0;
        while (tokens[t][i] && (isupper(tokens[t][i]) || tokens[t][i] == '_')) {
            phone[i] = tokens[t][i];
            i++;
        }
        phone[i] = '\0';
        if (tokens[t][i] && isdigit(tokens[t][i])) {
            stress = tokens[t][i] - '0';
        }
        
        const DiphthongEntry *dip = find_diphthong(phone);
        if (dip) {
            for (int k = 0; k < 2 && num_targets < MAX_TOKENS; k++) {
                const PhonemeEntry *p = find_phoneme(dip->components[k]);
                int cstress = (k == 0) ? stress : 0;
                float dscale = dip->dur_scales[k];
                
                Target *tg = &targets[num_targets++];
                strncpy(tg->phone, dip->components[k], 7);
                tg->phone[7] = '\0';
                memcpy(tg->f, p->f, 5 * sizeof(float));
                tg->voicing = p->voicing;
                tg->gain = p->gain;
                tg->pitch = f0 * (cstress ? 1.2f : 1.0f);
                tg->samps = (int)((p->duration * dscale * (cstress ? 1.3f : 1.0f) / 1000.0f) * FS);
                tg->is_plosive = 0;
                tg->plosive_type = 0;
            }
        } else {
            const PhonemeEntry *p = find_phoneme(phone);
            Target *tg = &targets[num_targets++];
            snprintf(tg->phone, sizeof(tg->phone), "%s", phone);
            memcpy(tg->f, p->f, 5 * sizeof(float));
            tg->voicing = p->voicing;
            tg->gain = p->gain;
            tg->pitch = f0 * (stress ? 1.2f : 1.0f);
            tg->samps = (int)((p->duration * (stress ? 1.3f : 1.0f) / 1000.0f) * FS);
            
            if (strcmp(phone, "P") == 0) { tg->is_plosive = 1; tg->plosive_type = 1; }
            else if (strcmp(phone, "T") == 0) { tg->is_plosive = 1; tg->plosive_type = 2; }
            else if (strcmp(phone, "K") == 0) { tg->is_plosive = 1; tg->plosive_type = 3; }
            else { tg->is_plosive = 0; tg->plosive_type = 0; }
        }
    }
    
    Target expanded[MAX_TOKENS * 2];
    int num_expanded = 0;
    const char *voiceless_stops[] = {"T", "P", "K", NULL};
    
    for (int i = 0; i < num_targets && num_expanded < MAX_TOKENS; i++) {
        expanded[num_expanded++] = targets[i];
        
        for (int j = 0; voiceless_stops[j]; j++) {
            if (strcmp(targets[i].phone, voiceless_stops[j]) == 0) {
                const char *asp_name = NULL;
                if (strcmp(targets[i].phone, "T") == 0) asp_name = "ASPT";
                else if (strcmp(targets[i].phone, "P") == 0) asp_name = "ASPP";
                else if (strcmp(targets[i].phone, "K") == 0) asp_name = "ASPK";
                
                if (asp_name && num_expanded < MAX_TOKENS) {
                    const PhonemeEntry *asp = find_phoneme(asp_name);
                    Target *tg = &expanded[num_expanded++];
                    strncpy(tg->phone, asp_name, 7);
                    tg->phone[7] = '\0';
                    memcpy(tg->f, asp->f, 5 * sizeof(float));
                    tg->voicing = asp->voicing;
                    tg->gain = asp->gain;
                    tg->pitch = f0 * 0.55f;
                    tg->samps = (int)(asp->duration / 1000.0f * FS);
                    tg->is_plosive = 0;
                    tg->plosive_type = 0;
                }
                break;
            }
        }
    }
    
    memcpy(targets, expanded, num_expanded * sizeof(Target));
    num_targets = num_expanded;
    
    if (num_targets == 0) {
        *out_len = 0;
        return NULL;
    }
    
    int crossfade_samps = (int)(CROSSFADE_MS * 0.001f * FS);
    int total_samps = 0;
    for (int i = 0; i < num_targets; i++) {
        total_samps += targets[i].samps;
    }
    total_samps -= crossfade_samps * (num_targets - 1);
    total_samps = (total_samps > FS / 2) ? total_samps : FS / 2;
    
    float *env_f[5], *env_v, *env_p, *env_g;
    env_f[0] = calloc(total_samps, sizeof(float));
    env_f[1] = calloc(total_samps, sizeof(float));
    env_f[2] = calloc(total_samps, sizeof(float));
    env_f[3] = calloc(total_samps, sizeof(float));
    env_f[4] = calloc(total_samps, sizeof(float));
    env_v = calloc(total_samps, sizeof(float));
    env_p = calloc(total_samps, sizeof(float));
    env_g = calloc(total_samps, sizeof(float));
    
    if (!env_f[0] || !env_v || !env_p || !env_g) {
        for (int i = 0; i < 5; i++) free(env_f[i]);
        free(env_v); free(env_p); free(env_g);
        *out_len = 0;
        return NULL;
    }
    
    int idx = 0;
    for (int i = 0; i < num_targets; i++) {
        Target *t = &targets[i];
        Target *prev = (i > 0) ? &targets[i-1] : t;
        
        int length = t->samps;
        int seg_length = (i < num_targets - 1) ? (length - crossfade_samps) : length;
        seg_length = (seg_length > 1) ? seg_length : 1;
        
        int attack = (int)(seg_length * 0.15f);
        int release = (int)(seg_length * 0.15f);
        if (attack > seg_length / 3) attack = seg_length / 3;
        if (release > seg_length / 3) release = seg_length / 3;
        int sustain = seg_length - attack - release;
        if (sustain < 1) sustain = 1;
        
        float *env = malloc(seg_length * sizeof(float));
        if (!env) continue;
        
        for (int j = 0; j < attack; j++)
            env[j] = (float)j / attack;
        for (int j = 0; j < sustain; j++)
            env[attack + j] = 1.0f;
        for (int j = 0; j < release; j++)
            env[attack + sustain + j] = 1.0f - (float)j / release;
        
        int end_idx = (idx + seg_length < total_samps) ? idx + seg_length : total_samps;
        int actual_len = end_idx - idx;
        
        for (int j = 0; j < actual_len && j < seg_length; j++) {
            float e = env[j];
            for (int k = 0; k < 5; k++) {
                env_f[k][idx + j] = lerp(prev->f[k], t->f[k], e);
            }
            env_v[idx + j] = lerp(prev->voicing, t->voicing, e);
            env_p[idx + j] = lerp(prev->pitch, t->pitch, e);
            env_g[idx + j] = lerp(prev->gain, t->gain, e);
        }
        
        free(env);
        idx += seg_length;
    }
    
    float *source = calloc(total_samps, sizeof(float));
    float *phase = calloc(total_samps, sizeof(float));
    float *burst_buf = calloc(total_samps, sizeof(float));
    if (!source || !phase || !burst_buf) {
        for (int i = 0; i < 5; i++) free(env_f[i]);
        free(env_v); free(env_p); free(env_g);
        free(source); free(phase); free(burst_buf);
        *out_len = 0;
        return NULL;
    }
    
    phase[0] = 0;
    for (int i = 1; i < total_samps; i++) {
        phase[i] = phase[i-1] + env_p[i] * 2.0f * PI / FS;
    }
    
    for (int i = 0; i < total_samps; i++) {
        float saw = 2.0f * fmodf(phase[i] / (2.0f * PI), 1.0f) - 1.0f;
        float buzz = saw * 2.0f;
        float hiss = ((float)rand() / RAND_MAX * 2.0f - 1.0f) * 1.25f;
        source[i] = (buzz * env_v[i] + hiss * (1.0f - env_v[i])) * env_g[i];
    }
    
    int src_idx = 0;
    for (int t = 0; t < num_targets; t++) {
        Target *tg = &targets[t];
        if (tg->is_plosive && tg->plosive_type > 0) {
            int burst_start = src_idx;
            int burst_max = (t < num_targets - 1) ? 
                           (tg->samps - crossfade_samps) : tg->samps;
            if (burst_start + burst_max <= total_samps) {
                generate_plosive_burst(burst_buf, burst_max, tg->plosive_type, FS);
                
                int blend_len = (int)(0.003f * FS);
                for (int i = 0; i < burst_max && burst_start + i < total_samps; i++) {
                    float blend = (i < blend_len) ? (float)i / blend_len : 1.0f;
                    source[burst_start + i] = source[burst_start + i] * (1.0f - blend * 0.7f) 
                                            + burst_buf[i] * blend * 0.7f;
                }
            }
        }
        src_idx += (t < num_targets - 1) ? (tg->samps - crossfade_samps) : tg->samps;
    }
    free(burst_buf);
    free(phase);
    
    float *output = calloc(total_samps, sizeof(float));
    BiquadState formant_state[5] = {{0}};
    float bws[5] = {100, 140, 200, 280, 380};
    
    for (int pos = 0; pos + WINDOW_SIZE <= total_samps; pos += HOP_SIZE) {
        float chunk[WINDOW_SIZE];
        memcpy(chunk, &source[pos], WINDOW_SIZE * sizeof(float));
        
        for (int f = 0; f < 5; f++) {
            float b[3], a[3];
            float fc = env_f[f][pos + WINDOW_SIZE/2];
            if (fc <= 0) fc = 50;
            make_resonator(fc, bws[f], FS, b, a);
            
            for (int n = 0; n < WINDOW_SIZE; n++) {
                chunk[n] = biquad_df2(chunk[n], b, a, &formant_state[f]);
            }
        }
        
        for (int n = 0; n < WINDOW_SIZE; n++) {
            float window = 0.5f * (1.0f - cosf(2.0f * PI * n / (WINDOW_SIZE - 1)));
            output[pos + n] += chunk[n] * window;
        }
    }
    
    float preemph_z = 0;
    for (int i = 0; i < total_samps; i++) {
        float x = output[i];
        output[i] = x - 0.97f * preemph_z;
        preemph_z = x;
    }
    
    float gain = 2.5f * powf(10.0f, vol_db / 20.0f);
    float peak = 0;
    for (int i = 0; i < total_samps; i++) {
        output[i] *= gain;
        output[i] = soft_clip(output[i], 0.9f);
        float absv = fabsf(output[i]);
        if (absv > peak) peak = absv;
    }
    
    if (peak > 0.001f) {
        float norm = 0.95f / peak;
        for (int i = 0; i < total_samps; i++) {
            output[i] *= norm;
        }
    }
    
    for (int i = 0; i < 5; i++) free(env_f[i]);
    free(env_v); free(env_p); free(env_g);
    free(source);
    
    *out_len = total_samps;
    return output;
}

static int write_wav(const char *filename, float *audio, int len) {
    FILE *f = fopen(filename, "wb");
    if (!f) {
        fprintf(stderr, "Error: Cannot open %s for writing\n", filename);
        return -1;
    }
    
    uint32_t sample_rate = FS;
    uint16_t channels = 1;
    uint16_t bits_per_sample = 16;
    uint32_t byte_rate = sample_rate * channels * bits_per_sample / 8;
    uint16_t block_align = channels * bits_per_sample / 8;
    uint32_t data_size = len * channels * bits_per_sample / 8;
    uint32_t chunk_size = 36 + data_size;
    
    fwrite("RIFF", 1, 4, f);
    fwrite(&chunk_size, 4, 1, f);
    fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f);
    uint32_t fmt_size = 16;
    fwrite(&fmt_size, 4, 1, f);
    uint16_t audio_format = 1;
    fwrite(&audio_format, 2, 1, f);
    fwrite(&channels, 2, 1, f);
    fwrite(&sample_rate, 4, 1, f);
    fwrite(&byte_rate, 4, 1, f);
    fwrite(&block_align, 2, 1, f);
    fwrite(&bits_per_sample, 2, 1, f);
    fwrite("data", 1, 4, f);
    fwrite(&data_size, 4, 1, f);
    
    for (int i = 0; i < len; i++) {
        int16_t sample = (int16_t)(audio[i] * 32767.0f);
        fwrite(&sample, 2, 1, f);
    }
    
    fclose(f);
    return 0;
}

static char* read_text_file(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "Error: Cannot open %s\n", filename);
        return NULL;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *buf = malloc(size + 1);
    if (!buf) { fclose(f); return NULL; }
    
    size_t read = fread(buf, 1, size, f);
    buf[read] = '\0';
    fclose(f);
    return buf;
}

static void print_usage(const char *prog) {
    printf("Formant Synthesizer\n\n");
    printf("Usage: %s [options] \"phoneme text\"\n", prog);
    printf("   or: %s [options] -i input.txt\n\n", prog);
    printf("Options:\n");
    printf("  -o FILE    Output WAV file (default: output.wav)\n");
    printf("  -i FILE    Read phoneme text from file\n");
    printf("  -f FREQ    Pitch in Hz (default: 100, range: 50-500)\n");
    printf("  -v DB      Volume in dB (default: 6.0, range: -120 to +120)\n");
    printf("  -V         Verbose output\n");
    printf("  -h         Show this help\n\n");
}

static int parse_args(int argc, char **argv, Config *cfg) {
    cfg->text=NULL; cfg->output="output.wav"; cfg->input_file=NULL;
    cfg->f0=100.0; cfg->vol_db=6.0; cfg->verbose=0;
    
    for(int i=1; i<argc; i++) {
        if(argv[i][0]=='-') {
            switch(argv[i][1]) {
                case 'o': if(i+1>=argc) return -1; cfg->output=argv[++i]; break;
                case 'i': if(i+1>=argc) return -1; cfg->input_file=argv[++i]; break;
                case 'f':
                    if(i+1>=argc) return -1;
                    cfg->f0 = atof(argv[++i]);
                    if (cfg->f0 < 50 || cfg->f0 > 500) return -1;
                    break;
                case 'v':
                    if(i+1>=argc) return -1;
                    cfg->vol_db = atof(argv[++i]);
                    if (cfg->vol_db < -120 || cfg->vol_db > 120) return -1;
                    break;
                case 'V': cfg->verbose=1; break;
                case 'h': print_usage(argv[0]); return 1;
                default: fprintf(stderr,"Unknown option: %s\n",argv[i]); return -1;
            }
        } else {
            if(cfg->text) return -1;
            cfg->text=argv[i];
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    Config cfg;
    int parse_result = parse_args(argc, argv, &cfg);
    if (parse_result != 0) {
        if (parse_result == 1) return 0;
        print_usage(argv[0]);
        return 1;
    }
    
    char *text_to_free = NULL;
    if (cfg.input_file) {
        cfg.text = read_text_file(cfg.input_file);
        if (!cfg.text) return 1;
        text_to_free = cfg.text;
    }
    if (!cfg.text) { print_usage(argv[0]); return 1; }
    
    if (cfg.verbose) {
        printf("Synthesizing: %s\n", cfg.text);
        printf("F0: %.1f Hz, Vol: %.1f dB, Sample Rate: %d Hz\n", cfg.f0, cfg.vol_db, FS);
    }
    
    srand(42);
    
    int audio_len;
    float *audio = synthesize(cfg.text, cfg.f0, cfg.vol_db, &audio_len);
    if (!audio) { 
        if (text_to_free) free(text_to_free); 
        fprintf(stderr, "Synthesis failed\n");
        return 1; 
    }
    
    if (cfg.verbose) {
        printf("Generated %d samples (%.2fs)\n", audio_len, (float)audio_len/FS);
    }
    
    if (write_wav(cfg.output, audio, audio_len) != 0) {
        free(audio); 
        if (text_to_free) free(text_to_free); 
        return 1;
    }
    
    printf("Saved: %s\n", cfg.output);
    
    float peak = 0;
    for (int i = 0; i < audio_len; i++) {
        float absv = fabsf(audio[i]);
        if (absv > peak) peak = absv;   
    }
    if (cfg.verbose) printf("Peak Amplitude: %.4f\n", peak);
    
    free(audio); 
    if (text_to_free) free(text_to_free);
    return 0;
}
