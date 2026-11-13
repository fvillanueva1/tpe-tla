#include "../../main/c/backend/music/Music.h"
#include <assert.h>
#include <string.h>

static MusicKey key(const char *tonic, MusicMode mode) {
    MusicKey result = {.mode = mode};
    assert(musicParseSpelling(tonic, &result.tonic) == MUSIC_OK);
    return result;
}

static void text(const MusicChord *chord, char *buffer) {
    char note[MUSIC_NOTE_TEXT_SIZE];
    buffer[0] = '\0';
    for (size_t i = 0; i < chord->count; ++i) {
        musicFormatNote(chord->notes[i], note);
        if (i) strcat(buffer, " ");
        strcat(buffer, note);
    }
}

/* Builds the chord on a degree lexeme and compares its notes and quality. */
static void degree(const char *tonic, MusicMode mode, const char *lexeme, MusicChordSize size,
                   const char *expected, MusicQuality quality) {
    int index;
    MusicChord chord;
    char buffer[MUSIC_MAX_CHORD_NOTES * MUSIC_NOTE_TEXT_SIZE];
    assert(musicParseDegree(lexeme, &index) == MUSIC_OK);
    assert(musicDegreeChord(key(tonic, mode), index, size, &chord) == MUSIC_OK);
    text(&chord, buffer);
    assert(!strcmp(buffer, expected) && chord.root == 0 && chord.quality == quality);
}

/* Builds an explicit chord from note lexemes and checks its root note and quality. */
static void notes(const char *const *lexemes, size_t count, const char *root, MusicQuality quality) {
    MusicNote values[MUSIC_MAX_CHORD_NOTES];
    MusicChord chord;
    char name[MUSIC_NOTE_TEXT_SIZE];
    for (size_t i = 0; i < count; ++i) assert(musicParseNote(lexemes[i], &values[i]) == MUSIC_OK);
    assert(musicChordFromNotes(values, count, &chord) == MUSIC_OK && chord.count == count);
    for (size_t i = 0; i < count; ++i) assert(chord.notes[i].pitch == values[i].pitch);
    musicFormatNote(chord.notes[chord.root], name);
    assert(!strcmp(name, root) && chord.quality == quality);
}

static void degrees(void) {
    int d;
    assert(musicParseDegree("I", &d) == MUSIC_OK && d == 0);
    assert(musicParseDegree("vii", &d) == MUSIC_OK && d == 6);
    assert(musicParseDegree("iidim", &d) == MUSIC_OK && d == 1);
    assert(musicParseDegree("IV", &d) == MUSIC_OK && d == 3);
    assert(musicParseDegree("VIII", &d) == MUSIC_INVALID_ARGUMENT && musicParseDegree("Vdim", &d) == MUSIC_INVALID_ARGUMENT);
    assert(musicParseDegree("iix", &d) == MUSIC_INVALID_ARGUMENT && musicParseDegree("", &d) == MUSIC_INVALID_ARGUMENT);
    assert(musicParseDegree(NULL, &d) == MUSIC_INVALID_ARGUMENT);

    /* The quality comes from the mode, whatever the case of the degree. */
    degree("C", MUSIC_MODE_MAJOR, "I", MUSIC_TRIAD, "C4 E4 G4", MUSIC_QUALITY_MAJOR);
    degree("C", MUSIC_MODE_MAJOR, "ii", MUSIC_TRIAD, "D4 F4 A4", MUSIC_QUALITY_MINOR);
    degree("C", MUSIC_MODE_MAJOR, "II", MUSIC_TRIAD, "D4 F4 A4", MUSIC_QUALITY_MINOR);
    degree("C", MUSIC_MODE_MAJOR, "V", MUSIC_TRIAD, "G4 B4 D5", MUSIC_QUALITY_MAJOR);
    degree("C", MUSIC_MODE_MAJOR, "viidim", MUSIC_TRIAD, "B4 D5 F5", MUSIC_QUALITY_DIMINISHED);
    degree("A", MUSIC_MODE_MINOR, "i", MUSIC_TRIAD, "A4 C5 E5", MUSIC_QUALITY_MINOR);
    degree("A", MUSIC_MODE_MINOR, "III", MUSIC_TRIAD, "C5 E5 G5", MUSIC_QUALITY_MAJOR);
    degree("A", MUSIC_MODE_MINOR, "iidim", MUSIC_TRIAD, "B4 D5 F5", MUSIC_QUALITY_DIMINISHED);
    degree("G", MUSIC_MODE_MAJOR, "V", MUSIC_TRIAD, "D5 F#5 A5", MUSIC_QUALITY_MAJOR);
    degree("F", MUSIC_MODE_MAJOR, "IV", MUSIC_TRIAD, "Bb4 D5 F5", MUSIC_QUALITY_MAJOR);
    degree("D", MUSIC_MODE_DORIAN, "IV", MUSIC_TRIAD, "G4 B4 D5", MUSIC_QUALITY_MAJOR);
    /* Sevenths and ninths pass the seventh degree into the next octave. */
    degree("C", MUSIC_MODE_MAJOR, "V", MUSIC_SEVENTH, "G4 B4 D5 F5", MUSIC_QUALITY_MAJOR);
    degree("C", MUSIC_MODE_MAJOR, "ii", MUSIC_SEVENTH, "D4 F4 A4 C5", MUSIC_QUALITY_MINOR);
    degree("C", MUSIC_MODE_MAJOR, "I", MUSIC_NINTH, "C4 E4 G4 B4 D5", MUSIC_QUALITY_MAJOR);
    degree("C", MUSIC_MODE_MAJOR, "vii", MUSIC_NINTH, "B4 D5 F5 A5 C6", MUSIC_QUALITY_DIMINISHED);
    degree("Eb", MUSIC_MODE_MAJOR, "V", MUSIC_NINTH, "Bb4 D5 F5 Ab5 C6", MUSIC_QUALITY_MAJOR);

    MusicChord chord;
    assert(musicDegreeChord(key("C", MUSIC_MODE_MAJOR), 7, MUSIC_TRIAD, &chord) == MUSIC_OUT_OF_RANGE);
    assert(musicDegreeChord(key("C", MUSIC_MODE_MAJOR), -1, MUSIC_TRIAD, &chord) == MUSIC_OUT_OF_RANGE);
    assert(musicDegreeChord(key("C", MUSIC_MODE_MAJOR), 0, (MusicChordSize)2, &chord) == MUSIC_INVALID_ARGUMENT);
    MusicKey bad = {{'H', 0}, MUSIC_MODE_MAJOR, false};
    assert(musicDegreeChord(bad, 0, MUSIC_TRIAD, &chord) == MUSIC_INVALID_ARGUMENT);
    assert(musicDegreeChord(key("C", MUSIC_MODE_MAJOR), 0, MUSIC_TRIAD, NULL) == MUSIC_INVALID_ARGUMENT);
}

static void explicit(void) {
    notes((const char *[]){"C4", "E4", "G4"}, 3, "C4", MUSIC_QUALITY_MAJOR);
    notes((const char *[]){"E4", "G4", "C5"}, 3, "C5", MUSIC_QUALITY_MAJOR);
    notes((const char *[]){"G4", "C4", "E4"}, 3, "C4", MUSIC_QUALITY_MAJOR);
    notes((const char *[]){"A3", "C4", "E4"}, 3, "A3", MUSIC_QUALITY_MINOR);
    notes((const char *[]){"B3", "D4", "F4"}, 3, "B3", MUSIC_QUALITY_DIMINISHED);
    notes((const char *[]){"D4", "F4", "B4"}, 3, "B4", MUSIC_QUALITY_DIMINISHED);
    notes((const char *[]){"C4", "E4", "G#4"}, 3, "C4", MUSIC_QUALITY_AUGMENTED);
    notes((const char *[]){"G3", "B3", "D4", "F4"}, 4, "G3", MUSIC_QUALITY_MAJOR);
    notes((const char *[]){"C4", "E4"}, 2, "C4", MUSIC_QUALITY_OTHER);
    notes((const char *[]){"D4"}, 1, "D4", MUSIC_QUALITY_OTHER);
    notes((const char *[]){"C4", "C5", "E4", "G4", "C3"}, 5, "C3", MUSIC_QUALITY_MAJOR);
    notes((const char *[]){"Db4", "F4", "Ab4"}, 3, "Db4", MUSIC_QUALITY_MAJOR);

    MusicNote values[MUSIC_MAX_CHORD_NOTES + 1];
    MusicChord chord;
    for (size_t i = 0; i <= MUSIC_MAX_CHORD_NOTES; ++i) assert(musicParseNote("C4", &values[i]) == MUSIC_OK);
    assert(musicChordFromNotes(values, MUSIC_MAX_CHORD_NOTES, &chord) == MUSIC_OK);
    assert(musicChordFromNotes(values, MUSIC_MAX_CHORD_NOTES + 1, &chord) == MUSIC_TOO_MANY_NOTES);
    assert(musicChordFromNotes(values, 0, &chord) == MUSIC_INVALID_ARGUMENT);
    assert(musicChordFromNotes(NULL, 1, &chord) == MUSIC_INVALID_ARGUMENT);
    MusicNote wrong = {61, {'C', 0}};
    assert(musicChordFromNotes(&wrong, 1, &chord) == MUSIC_INVALID_ARGUMENT);
}

static void masks(void) {
    MusicChord chord;
    assert(musicDegreeChord(key("C", MUSIC_MODE_MAJOR), 4, MUSIC_SEVENTH, &chord) == MUSIC_OK);
    assert(musicChordMask(&chord) == ((1u << 7) | (1u << 11) | (1u << 2) | (1u << 5)));
    assert(musicChordClassCount(&chord) == 4);
    assert(musicDegreeChord(key("C", MUSIC_MODE_MAJOR), 0, MUSIC_NINTH, &chord) == MUSIC_OK && musicChordClassCount(&chord) == 5);
    MusicNote octaves[2];
    assert(musicParseNote("C4", &octaves[0]) == MUSIC_OK && musicParseNote("C5", &octaves[1]) == MUSIC_OK);
    assert(musicChordFromNotes(octaves, 2, &chord) == MUSIC_OK && musicChordMask(&chord) == 1 && musicChordClassCount(&chord) == 1);
    assert(musicChordMask(NULL) == 0 && musicChordClassCount(NULL) == 0);
}

int main(void) {
    degrees();
    explicit();
    masks();
    return 0;
}
