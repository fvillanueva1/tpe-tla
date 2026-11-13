#include "Music.h"

static bool validChord(const MusicChord *chord) {
    return chord && chord->count && chord->count <= MUSIC_MAX_CHORD_NOTES && chord->root < chord->count;
}

MusicStatus musicChordRoot(const MusicChord *chord, MusicNote *root) {
    if (!validChord(chord) || !root) return MUSIC_INVALID_ARGUMENT;
    *root = chord->notes[chord->root];
    return MUSIC_OK;
}

MusicQuality musicChordQuality(const MusicChord *chord) {
    return validChord(chord) ? chord->quality : MUSIC_QUALITY_OTHER;
}

MusicStatus musicKeyTonic(MusicKey key, MusicNote *tonic) {
    if (!tonic) return MUSIC_INVALID_ARGUMENT;
    return musicNoteFromSpelling(key.tonic, 4, tonic) == MUSIC_OK ? MUSIC_OK : MUSIC_INVALID_ARGUMENT;
}

MusicMode musicKeyMode(MusicKey key) {
    return key.mode;
}

const char *musicModeName(MusicMode mode) {
    static const char *names[] = {
        [MUSIC_MODE_DORIAN] = "dorian", [MUSIC_MODE_LOCRIAN] = "locrian", [MUSIC_MODE_LYDIAN] = "lydian",
        [MUSIC_MODE_MAJOR] = "major", [MUSIC_MODE_MINOR] = "minor", [MUSIC_MODE_MIXOLYDIAN] = "mixolydian",
        [MUSIC_MODE_PHRYGIAN] = "phrygian"
    };
    return mode >= MUSIC_MODE_DORIAN && mode <= MUSIC_MODE_PHRYGIAN ? names[mode] : NULL;
}

const char *musicQualityName(MusicQuality quality) {
    static const char *names[] = {
        [MUSIC_QUALITY_MAJOR] = "major", [MUSIC_QUALITY_MINOR] = "minor",
        [MUSIC_QUALITY_DIMINISHED] = "diminished", [MUSIC_QUALITY_AUGMENTED] = "augmented",
        [MUSIC_QUALITY_OTHER] = "other"
    };
    return quality >= MUSIC_QUALITY_MAJOR && quality <= MUSIC_QUALITY_OTHER ? names[quality] : NULL;
}

const char *musicDurationName(MusicDuration duration) {
    static const char *names[] = {
        [MUSIC_DURATION_WHOLE] = "whole", [MUSIC_DURATION_HALF] = "half", [MUSIC_DURATION_QUARTER] = "quarter",
        [MUSIC_DURATION_EIGHTH] = "eighth", [MUSIC_DURATION_SIXTEENTH] = "sixteenth"
    };
    return duration >= MUSIC_DURATION_WHOLE && duration <= MUSIC_DURATION_SIXTEENTH ? names[duration] : NULL;
}

static bool validNote(MusicNote note) {
    return note.pitch >= MUSIC_LOWEST_PITCH && note.pitch <= MUSIC_HIGHEST_PITCH;
}

bool musicNoteInScale(MusicNote note, const MusicScale *scale) {
    if (!scale || !validNote(note)) return false;
    for (int i = 0; i < 7; ++i)
        if (scale->notes[i].pitch % 12 == note.pitch % 12) return true;
    return false;
}

bool musicNoteInChord(MusicNote note, const MusicChord *chord) {
    if (!validChord(chord) || !validNote(note)) return false;
    return (musicChordMask(chord) >> (note.pitch % 12)) & 1u;
}

bool musicNoteInKey(MusicNote note, MusicKey key) {
    int tonic = musicPitchClass(key.tonic);
    return tonic >= 0 && validNote(note) && musicInKey(&note.pitch, 1, tonic, key.mode);
}

bool musicChordInKey(const MusicChord *chord, MusicKey key) {
    if (!validChord(chord)) return false;
    for (size_t i = 0; i < chord->count; ++i)
        if (!musicNoteInKey(chord->notes[i], key)) return false;
    return true;
}
