#include "Music.h"
#include <stdint.h>
#include <stdlib.h>

static int classCount(unsigned short mask) {
    int count = 0;
    for (; mask; mask &= (unsigned short)(mask - 1)) ++count;
    return count;
}

/* Clears the first of the intervals above the root that the chord has. */
static void omit(unsigned short *mask, int root, const int *intervals, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        unsigned short bit = (unsigned short)(1u << ((root + intervals[i]) % 12));
        if (*mask & bit) { *mask &= (unsigned short)~bit; return; }
    }
}

/* Pitch classes that "voice" distributes: at most four, dropping the fifth and then the seventh. */
static MusicStatus voicedMask(const MusicChord *chord, unsigned short *mask) {
    static const int fifths[] = {7, 6, 8}, sevenths[] = {10, 11, 9};
    if (!chord->count || chord->count > MUSIC_MAX_CHORD_NOTES || chord->root >= chord->count) return MUSIC_INVALID_ARGUMENT;
    int root = chord->notes[chord->root].pitch % 12;
    *mask = musicChordMask(chord);
    if (classCount(*mask) > VOICE_COUNT) omit(mask, root, fifths, 3);
    if (classCount(*mask) > VOICE_COUNT) omit(mask, root, sevenths, 3);
    return classCount(*mask) > VOICE_COUNT ? MUSIC_TOO_MANY_NOTES : MUSIC_OK;
}

MusicStatus musicVoiceProgression(const MusicProgression *progression, MusicVoicing *result) {
    if (!result) return MUSIC_INVALID_ARGUMENT;
    result->chords = NULL; result->durations = NULL; result->count = 0;
    if (!progression || (!progression->steps && progression->count)) return MUSIC_INVALID_ARGUMENT;
    size_t count = progression->count;
    if (!count) return MUSIC_OK;
    if (count > SIZE_MAX / sizeof(unsigned short)) return MUSIC_NO_MEMORY;
    unsigned short *masks = malloc(count * sizeof(*masks));
    if (!masks) return MUSIC_NO_MEMORY;
    MusicStatus status = MUSIC_OK;
    for (size_t i = 0; i < count && status == MUSIC_OK; ++i) status = voicedMask(&progression->steps[i].chord, &masks[i]);
    if (status == MUSIC_OK && !musicVoicingInit(result, count)) status = MUSIC_NO_MEMORY;
    if (status == MUSIC_OK) {
        MusicVoiceResult realization = musicRealizeSATB(masks, count, result->chords);
        if (realization == MUSIC_VOICE_NO_MEMORY) status = MUSIC_NO_MEMORY;
        else if (realization != MUSIC_VOICE_SUCCESS) status = MUSIC_NO_REALIZATION;
    }
    free(masks);
    if (status != MUSIC_OK) { musicVoicingDestroy(result); return status; }
    for (size_t i = 0; i < count; ++i) result->durations[i] = progression->steps[i].duration;
    return MUSIC_OK;
}

MusicStatus musicExplicitVoicing(const MusicChord *chords, size_t count, MusicVoicing *result) {
    if (!result) return MUSIC_INVALID_ARGUMENT;
    result->chords = NULL; result->durations = NULL; result->count = 0;
    if (!chords && count) return MUSIC_INVALID_ARGUMENT;
    for (size_t i = 0; i < count; ++i) {
        if (chords[i].count != VOICE_COUNT) return MUSIC_NOT_FOUR_VOICES;
        for (int v = 0; v < VOICE_COUNT; ++v)
            if (chords[i].notes[v].pitch < MUSIC_LOWEST_PITCH || chords[i].notes[v].pitch > MUSIC_HIGHEST_PITCH)
                return MUSIC_INVALID_ARGUMENT;
    }
    if (!musicVoicingInit(result, count)) return MUSIC_NO_MEMORY;
    for (size_t i = 0; i < count; ++i) {
        for (int v = 0; v < VOICE_COUNT; ++v) result->chords[i][v] = chords[i].notes[v].pitch;
        result->durations[i] = MUSIC_DEFAULT_DURATION;
    }
    return MUSIC_OK;
}

MusicValidationResult musicCheckVoicingValue(const MusicVoicing *voicing, unsigned int rules, size_t *failedChord) {
    if (!voicing || (!voicing->chords && voicing->count)) return MUSIC_INVALID_INPUT;
    return musicCheckVoicing((const int (*)[VOICE_COUNT])voicing->chords, voicing->count, rules, failedChord);
}
