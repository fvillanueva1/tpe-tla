#include "Music.h"
#include <ctype.h>
#include <string.h>

MusicStatus musicParseDegree(const char *lexeme, int *degree) {
    static const char *names[] = {"I", "II", "III", "IV", "V", "VI", "VII"};
    if (!lexeme || !degree) return MUSIC_INVALID_ARGUMENT;
    char roman[4];
    size_t length = 0;
    while (lexeme[length] && length < sizeof(roman) - 1 && strchr("IViv", lexeme[length])) {
        roman[length] = (char)toupper((unsigned char)lexeme[length]);
        ++length;
    }
    roman[length] = '\0';
    /* After the numeral only the "dim" suffix of a lowercase degree is allowed. */
    const char *rest = lexeme + length;
    if (*rest && (strcmp(rest, "dim") || !islower((unsigned char)lexeme[0]))) return MUSIC_INVALID_ARGUMENT;
    for (int i = 0; i < 7; ++i)
        if (!strcmp(roman, names[i])) { *degree = i; return MUSIC_OK; }
    return MUSIC_INVALID_ARGUMENT;
}

/* Index of the note with pitch class "pitchClass" in the chord, or -1. */
static int findClass(const MusicChord *chord, int pitchClass) {
    for (size_t i = 0; i < chord->count; ++i)
        if (chord->notes[i].pitch % 12 == pitchClass) return (int)i;
    return -1;
}

static bool hasInterval(const MusicChord *chord, int root, int semitones) {
    return findClass(chord, (root + semitones) % 12) >= 0;
}

/* Triad quality from the root's third and fifth. */
static MusicQuality quality(const MusicChord *chord, int root) {
    bool minorThird = hasInterval(chord, root, 3), majorThird = hasInterval(chord, root, 4);
    if (majorThird && hasInterval(chord, root, 7)) return MUSIC_QUALITY_MAJOR;
    if (minorThird && hasInterval(chord, root, 7)) return MUSIC_QUALITY_MINOR;
    if (minorThird && hasInterval(chord, root, 6)) return MUSIC_QUALITY_DIMINISHED;
    if (majorThird && hasInterval(chord, root, 8)) return MUSIC_QUALITY_AUGMENTED;
    return MUSIC_QUALITY_OTHER;
}

MusicStatus musicDegreeChord(MusicKey key, int degree, MusicChordSize size, MusicChord *chord) {
    MusicScale scale;
    if (!chord || (size != MUSIC_TRIAD && size != MUSIC_SEVENTH && size != MUSIC_NINTH)) return MUSIC_INVALID_ARGUMENT;
    if (degree < 0 || degree > 6) return MUSIC_OUT_OF_RANGE;
    if (musicKeyScale(key, &scale) != MUSIC_OK) return MUSIC_INVALID_ARGUMENT;
    MusicChord result = {.count = (size_t)size, .root = 0};
    for (int i = 0; i < (int)size; ++i) {
        int step = degree + 2 * i;
        MusicNote note = scale.notes[step % 7];
        for (int octave = 0; octave < step / 7; ++octave)
            if (musicTransposeNote(note, (MusicInterval){12, 7}, false, &note) != MUSIC_OK) return MUSIC_INVALID_ARGUMENT;
        result.notes[i] = note;
    }
    result.quality = quality(&result, result.notes[0].pitch % 12);
    *chord = result;
    return MUSIC_OK;
}

MusicStatus musicChordFromNotes(const MusicNote *notes, size_t count, MusicChord *chord) {
    if (!notes || !chord || !count) return MUSIC_INVALID_ARGUMENT;
    if (count > MUSIC_MAX_CHORD_NOTES) return MUSIC_TOO_MANY_NOTES;
    MusicChord result = {.count = count};
    for (size_t i = 0; i < count; ++i) {
        if (notes[i].pitch < MUSIC_LOWEST_PITCH || notes[i].pitch > MUSIC_HIGHEST_PITCH ||
            musicPitchClass(notes[i].spelling) != notes[i].pitch % 12) return MUSIC_INVALID_ARGUMENT;
        result.notes[i] = notes[i];
    }
    /* Candidates from the lowest pitch up: first with a perfect fifth, then with any fifth. */
    size_t order[MUSIC_MAX_CHORD_NOTES];
    for (size_t i = 0; i < count; ++i) {
        size_t j = i;
        for (; j > 0 && result.notes[order[j - 1]].pitch > result.notes[i].pitch; --j) order[j] = order[j - 1];
        order[j] = i;
    }
    result.root = order[0];
    bool found = false;
    for (int pass = 0; pass < 2 && !found; ++pass)
        for (size_t i = 0; i < count && !found; ++i) {
            int root = result.notes[order[i]].pitch % 12;
            bool third = hasInterval(&result, root, 3) || hasInterval(&result, root, 4);
            bool fifth = hasInterval(&result, root, 7) ||
                (pass && (hasInterval(&result, root, 6) || hasInterval(&result, root, 8)));
            if (third && fifth) { result.root = order[i]; found = true; }
        }
    result.quality = quality(&result, result.notes[result.root].pitch % 12);
    *chord = result;
    return MUSIC_OK;
}

unsigned short musicChordMask(const MusicChord *chord) {
    unsigned short mask = 0;
    if (!chord) return 0;
    for (size_t i = 0; i < chord->count && i < MUSIC_MAX_CHORD_NOTES; ++i)
        mask |= (unsigned short)(1u << (chord->notes[i].pitch % 12));
    return mask;
}

int musicChordClassCount(const MusicChord *chord) {
    int count = 0;
    for (unsigned short mask = musicChordMask(chord); mask; mask &= (unsigned short)(mask - 1)) ++count;
    return count;
}
