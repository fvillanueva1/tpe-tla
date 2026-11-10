#include "MusicValidation.h"
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct { int voice[VOICE_COUNT]; long long cost; size_t previous; } Candidate;
typedef struct { Candidate *items; size_t count, capacity; } Layer;

static bool append(Layer *layer, const int voices[VOICE_COUNT]) {
    if (layer->count == layer->capacity) {
        size_t capacity = layer->capacity ? layer->capacity * 2 : 64;
        if (capacity < layer->capacity || capacity > SIZE_MAX / sizeof(Candidate)) return false;
        Candidate *items = realloc(layer->items, capacity * sizeof(Candidate));
        if (!items) return false;
        layer->items = items; layer->capacity = capacity;
    }
    Candidate *candidate = &layer->items[layer->count++];
    for (int i = 0; i < VOICE_COUNT; ++i) candidate->voice[i] = voices[i];
    candidate->cost = LLONG_MAX; candidate->previous = 0;
    return true;
}

static MusicVoiceResult candidates(unsigned short chord, Layer *layer) {
    if (!chord || (chord & 0xf000)) return MUSIC_VOICE_INVALID_CHORD;
    static const int low[VOICE_COUNT] = {60, 53, 48, 40};
    static const int high[VOICE_COUNT] = {81, 74, 69, 64};
    int options[VOICE_COUNT][25], count[VOICE_COUNT] = {0};
    for (int v = 0; v < VOICE_COUNT; ++v)
        for (int pitch = low[v]; pitch <= high[v]; ++pitch)
            if (chord & (1u << (pitch % 12))) options[v][count[v]++] = pitch;
    int notes[VOICE_COUNT];
    for (int s = 0; s < count[0]; ++s) for (int a = 0; a < count[1]; ++a)
    for (int t = 0; t < count[2]; ++t) for (int b = 0; b < count[3]; ++b) {
        notes[0] = options[0][s]; notes[1] = options[1][a]; notes[2] = options[2][t]; notes[3] = options[3][b];
        if (!musicWithoutVoiceCrossing(notes)) continue;
        unsigned short mask = 0;
        for (int v = 0; v < VOICE_COUNT; ++v) mask |= 1u << (notes[v] % 12);
        if (mask == chord && !append(layer, notes)) return MUSIC_VOICE_NO_MEMORY;
    }
    return layer->count ? MUSIC_VOICE_SUCCESS : MUSIC_VOICE_INVALID_CHORD;
}

MusicVoiceResult musicRealizeSATB(const unsigned short *chords, size_t count, int (*voices)[VOICE_COUNT]) {
    if (!count) return MUSIC_VOICE_SUCCESS;
    if (!chords || !voices || count > SIZE_MAX / sizeof(Layer)) return MUSIC_VOICE_INVALID_CHORD;
    Layer *layers = calloc(count, sizeof(Layer));
    if (!layers) return MUSIC_VOICE_NO_MEMORY;
    MusicVoiceResult result = MUSIC_VOICE_SUCCESS;
    for (size_t i = 0; i < count && result == MUSIC_VOICE_SUCCESS; ++i) {
        result = candidates(chords[i], &layers[i]);
        if (result != MUSIC_VOICE_SUCCESS) break;
        for (size_t current = 0; current < layers[i].count; ++current) {
            Candidate *c = &layers[i].items[current];
            if (!i) { c->cost = 0; continue; }
            for (size_t previous = 0; previous < layers[i - 1].count; ++previous) {
                Candidate *p = &layers[i - 1].items[previous];
                int movement = 0;
                for (int v = 0; v < VOICE_COUNT; ++v) {
                    int delta = c->voice[v] - p->voice[v];
                    movement += delta < 0 ? -delta : delta;
                }
                if (p->cost <= LLONG_MAX - movement && p->cost + movement < c->cost) {
                    c->cost = p->cost + movement; c->previous = previous;
                }
            }
        }
    }
    if (result == MUSIC_VOICE_SUCCESS) {
        size_t best = 0;
        for (size_t i = 1; i < layers[count - 1].count; ++i)
            if (layers[count - 1].items[i].cost < layers[count - 1].items[best].cost) best = i;
        for (size_t i = count; i-- > 0;) {
            Candidate *candidate = &layers[i].items[best];
            for (int v = 0; v < VOICE_COUNT; ++v) voices[i][v] = candidate->voice[v];
            best = candidate->previous;
        }
    }
    for (size_t i = 0; i < count; ++i) free(layers[i].items);
    free(layers);
    return result;
}
