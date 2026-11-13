#include "Music.h"
#include <stdint.h>

static const MusicInterval octave = {12, 7};

static bool validChord(const MusicChord *chord) {
    return chord && chord->count && chord->count <= MUSIC_MAX_CHORD_NOTES && chord->root < chord->count;
}

static bool validProgression(const MusicProgression *progression) {
    return progression && (progression->steps || !progression->count);
}

MusicStatus musicTransposeKey(MusicKey key, MusicInterval interval, bool down, MusicKey *result) {
    MusicNote tonic, moved;
    if (!result || musicKeyTonic(key, &tonic) != MUSIC_OK ||
        musicTransposeNote(tonic, interval, down, &moved) != MUSIC_OK) return MUSIC_INVALID_ARGUMENT;
    *result = key;
    result->tonic = moved.spelling;
    return MUSIC_OK;
}

MusicStatus musicTransposeChord(const MusicChord *chord, MusicInterval interval, bool down, MusicChord *result) {
    if (!validChord(chord) || !result) return MUSIC_INVALID_ARGUMENT;
    MusicChord moved = *chord;
    for (size_t i = 0; i < chord->count; ++i) {
        MusicStatus status = musicTransposeNote(chord->notes[i], interval, down, &moved.notes[i]);
        if (status != MUSIC_OK) return status;
    }
    if (chord->hasKey && musicTransposeKey(chord->key, interval, down, &moved.key) != MUSIC_OK) return MUSIC_INVALID_ARGUMENT;
    *result = moved;
    return MUSIC_OK;
}

MusicStatus musicTransposeProgression(const MusicProgression *progression, MusicInterval interval, bool down,
                                      MusicProgression *result) {
    if (!result) return MUSIC_INVALID_ARGUMENT;
    result->steps = NULL; result->count = 0;
    if (!validProgression(progression)) return MUSIC_INVALID_ARGUMENT;
    if (!musicProgressionCopy(progression, result)) return MUSIC_NO_MEMORY;
    for (size_t i = 0; i < result->count; ++i) {
        MusicStatus status = musicTransposeChord(&progression->steps[i].chord, interval, down, &result->steps[i].chord);
        if (status != MUSIC_OK) { musicProgressionDestroy(result); return status; }
    }
    return MUSIC_OK;
}

/* Indices of the chord's notes from the lowest pitch up (stable for equal pitches). */
static void ascending(const MusicChord *chord, size_t order[MUSIC_MAX_CHORD_NOTES]) {
    for (size_t i = 0; i < chord->count; ++i) {
        size_t j = i;
        for (; j > 0 && chord->notes[order[j - 1]].pitch > chord->notes[i].pitch; --j) order[j] = order[j - 1];
        order[j] = i;
    }
}

MusicStatus musicInvertChord(const MusicChord *chord, int times, MusicChord *result) {
    if (!validChord(chord) || !result) return MUSIC_INVALID_ARGUMENT;
    if (times < 0 || (size_t)times > chord->count) return MUSIC_OUT_OF_RANGE;
    size_t order[MUSIC_MAX_CHORD_NOTES];
    ascending(chord, order);
    /* "notes" and "origin" stay sorted from the lowest pitch; origin tracks where the root goes. */
    MusicNote notes[MUSIC_MAX_CHORD_NOTES];
    size_t origin[MUSIC_MAX_CHORD_NOTES], count = chord->count;
    for (size_t i = 0; i < count; ++i) { notes[i] = chord->notes[order[i]]; origin[i] = order[i]; }
    for (int t = 0; t < times; ++t) {
        MusicNote lowest = notes[0];
        size_t source = origin[0];
        do {
            MusicStatus status = musicTransposeNote(lowest, octave, false, &lowest);
            if (status != MUSIC_OK) return status;
        } while (lowest.pitch <= notes[count - 1].pitch);
        for (size_t i = 1; i < count; ++i) { notes[i - 1] = notes[i]; origin[i - 1] = origin[i]; }
        notes[count - 1] = lowest;
        origin[count - 1] = source;
    }
    MusicChord inverted = *chord;
    for (size_t i = 0; i < count; ++i) {
        inverted.notes[i] = notes[i];
        if (origin[i] == chord->root) inverted.root = i;
    }
    inverted.size = 0;
    inverted.degree = 0;
    *result = inverted;
    return MUSIC_OK;
}

MusicStatus musicArpeggiate(const MusicChord *chord, MusicDuration duration, MusicProgression *result) {
    if (!result) return MUSIC_INVALID_ARGUMENT;
    result->steps = NULL; result->count = 0;
    if (!validChord(chord) || duration < MUSIC_DURATION_WHOLE || duration > MUSIC_DURATION_SIXTEENTH) return MUSIC_INVALID_ARGUMENT;
    if (!musicProgressionInit(result, chord->count)) return MUSIC_NO_MEMORY;
    size_t order[MUSIC_MAX_CHORD_NOTES];
    ascending(chord, order);
    for (size_t i = 0; i < chord->count; ++i) {
        MusicChord single;
        if (musicChordFromNotes(&chord->notes[order[i]], 1, &single) != MUSIC_OK) {
            musicProgressionDestroy(result); return MUSIC_INVALID_ARGUMENT;
        }
        single.hasKey = chord->hasKey;
        single.key = chord->key;
        result->steps[i] = (MusicStep){single, duration};
    }
    return MUSIC_OK;
}

/* The interval from the tonic of "from" to the nearest tonic of "to" (at most a tritone away). */
static MusicStatus tonicInterval(MusicKey from, MusicKey to, MusicInterval *interval, bool *down) {
    MusicNote source, target;
    if (musicKeyTonic(from, &source) != MUSIC_OK || musicKeyTonic(to, &target) != MUSIC_OK) return MUSIC_INVALID_ARGUMENT;
    int distance = target.pitch - source.pitch;
    if (distance > 6) target.pitch -= 12;
    else if (distance < -6) target.pitch += 12;
    if (musicIntervalBetween(source, target, interval, down) == MUSIC_OK) return MUSIC_OK;
    /* Spellings that no interval joins (B# to Cb): move by semitones and respell. */
    distance = target.pitch - source.pitch;
    *down = distance < 0;
    *interval = (MusicInterval){distance < 0 ? -distance : distance, 0};
    return MUSIC_OK;
}

MusicStatus musicModulate(const MusicProgression *progression, MusicKey key, MusicProgression *result) {
    if (!result) return MUSIC_INVALID_ARGUMENT;
    result->steps = NULL; result->count = 0;
    MusicNote tonic;
    if (!validProgression(progression) || musicKeyTonic(key, &tonic) != MUSIC_OK ||
        key.mode < MUSIC_MODE_DORIAN || key.mode > MUSIC_MODE_PHRYGIAN) return MUSIC_INVALID_ARGUMENT;
    const MusicKey *source = NULL;
    for (size_t i = 0; i < progression->count && !source; ++i)
        if (progression->steps[i].chord.hasKey) source = &progression->steps[i].chord.key;
    if (!musicProgressionCopy(progression, result)) return MUSIC_NO_MEMORY;
    for (size_t i = 0; i < progression->count; ++i) {
        const MusicChord *chord = &progression->steps[i].chord;
        MusicChord *target = &result->steps[i].chord;
        MusicStatus status;
        if (chord->size) {
            status = musicDegreeChord(key, chord->degree, chord->size, target);
        } else if (!chord->hasKey && !source) {
            status = MUSIC_MISSING_KEY;
        } else {
            MusicInterval interval;
            bool down;
            status = tonicInterval(chord->hasKey ? chord->key : *source, key, &interval, &down);
            if (status == MUSIC_OK) status = musicTransposeChord(chord, interval, down, target);
            if (status == MUSIC_OK) { target->hasKey = true; target->key = key; }
        }
        if (status != MUSIC_OK) { musicProgressionDestroy(result); return status; }
    }
    return MUSIC_OK;
}
