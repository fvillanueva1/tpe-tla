#include "Music.h"

static bool validKey(MusicKey key) {
    return musicPitchClass(key.tonic) >= 0 && key.mode >= MUSIC_MODE_DORIAN && key.mode <= MUSIC_MODE_PHRYGIAN;
}

MusicStatus musicKeyScale(MusicKey key, MusicScale *scale) {
    MusicNote tonic;
    int pitches[7];
    if (!scale || !validKey(key) || musicNoteFromSpelling(key.tonic, 4, &tonic) != MUSIC_OK ||
        !musicScale(tonic.pitch, key.mode, pitches)) return MUSIC_INVALID_ARGUMENT;
    /* Degree i is the tonic moved up i letters and as many semitones as the mode says. */
    MusicScale result;
    for (int i = 0; i < 7; ++i) {
        MusicInterval interval = {pitches[i] - pitches[0], i};
        if (musicTransposeNote(tonic, interval, false, &result.notes[i]) != MUSIC_OK) return MUSIC_INVALID_ARGUMENT;
    }
    *scale = result;
    return MUSIC_OK;
}

MusicStatus musicScaleNote(const MusicScale *scale, int index, MusicNote *note) {
    if (!scale || !note) return MUSIC_INVALID_ARGUMENT;
    if (index < 0 || index > 6) return MUSIC_OUT_OF_RANGE;
    *note = scale->notes[index];
    return MUSIC_OK;
}

/* The key whose tonic is the original's moved up by the interval, with the same mode. */
static MusicStatus relatedKey(MusicKey key, MusicInterval interval, MusicKey *result) {
    MusicNote tonic, moved;
    if (!result || !validKey(key) || musicNoteFromSpelling(key.tonic, 4, &tonic) != MUSIC_OK ||
        musicTransposeNote(tonic, interval, false, &moved) != MUSIC_OK) return MUSIC_INVALID_ARGUMENT;
    *result = key;
    result->tonic = moved.spelling;
    return MUSIC_OK;
}

MusicStatus musicDominantKey(MusicKey key, MusicKey *result) {
    return relatedKey(key, (MusicInterval){7, 4}, result);
}

MusicStatus musicSubdominantKey(MusicKey key, MusicKey *result) {
    return relatedKey(key, (MusicInterval){5, 3}, result);
}

MusicStatus musicRelativeMinorKey(MusicKey key, MusicKey *result) {
    MusicScale scale;
    int minor[7], mode[7];
    if (!result || musicKeyScale(key, &scale) != MUSIC_OK || !musicScale(60, MUSIC_MODE_MINOR, minor) ||
        !musicScale(60, key.mode, mode)) return MUSIC_INVALID_ARGUMENT;
    /* Find the degree from which the mode's steps are those of the minor mode. */
    for (int degree = 0; degree < 7; ++degree) {
        bool same = true;
        for (int i = 0; i < 7 && same; ++i)
            same = (mode[(degree + i) % 7] - mode[degree] + 12) % 12 == minor[i] - minor[0];
        if (same) {
            *result = key;
            result->tonic = scale.notes[degree].spelling;
            result->mode = MUSIC_MODE_MINOR;
            return MUSIC_OK;
        }
    }
    return MUSIC_INVALID_ARGUMENT;
}
