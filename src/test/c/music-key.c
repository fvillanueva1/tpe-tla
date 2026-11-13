#include "../../main/c/backend/music/Music.h"
#include <assert.h>
#include <string.h>

static MusicKey key(const char *tonic, MusicMode mode) {
    MusicKey result = {.mode = mode, .strict = false};
    assert(musicParseSpelling(tonic, &result.tonic) == MUSIC_OK);
    return result;
}

/* Compares the scale with seven space-separated notes ("C4 D4 E4 ..."). */
static void scale(const char *tonic, MusicMode mode, const char *expected) {
    MusicScale result;
    char text[7 * MUSIC_NOTE_TEXT_SIZE] = "", note[MUSIC_NOTE_TEXT_SIZE];
    assert(musicKeyScale(key(tonic, mode), &result) == MUSIC_OK);
    for (int i = 0; i < 7; ++i) {
        musicFormatNote(result.notes[i], note);
        if (i) strcat(text, " ");
        strcat(text, note);
    }
    assert(!strcmp(text, expected));
}

/* Compares the tonic and mode of a derived key. */
static void related(MusicStatus (*relation)(MusicKey, MusicKey *), const char *tonic, MusicMode mode,
                    const char *expectedTonic, MusicMode expectedMode) {
    MusicKey result;
    assert(relation(key(tonic, mode), &result) == MUSIC_OK);
    /* The expected tonic can have a double accidental ("F##"), which the language does not write. */
    int accidental = 0;
    for (const char *c = expectedTonic + 1; *c; ++c) accidental += *c == '#' ? 1 : -1;
    assert(result.tonic.letter == expectedTonic[0] && result.tonic.accidental == accidental);
    assert(result.mode == expectedMode);
}

static void scales(void) {
    scale("C", MUSIC_MODE_MAJOR, "C4 D4 E4 F4 G4 A4 B4");
    scale("G", MUSIC_MODE_MAJOR, "G4 A4 B4 C5 D5 E5 F#5");
    scale("F", MUSIC_MODE_MAJOR, "F4 G4 A4 Bb4 C5 D5 E5");
    scale("A", MUSIC_MODE_MINOR, "A4 B4 C5 D5 E5 F5 G5");
    scale("D", MUSIC_MODE_DORIAN, "D4 E4 F4 G4 A4 B4 C5");
    scale("E", MUSIC_MODE_PHRYGIAN, "E4 F4 G4 A4 B4 C5 D5");
    scale("F", MUSIC_MODE_LYDIAN, "F4 G4 A4 B4 C5 D5 E5");
    scale("G", MUSIC_MODE_MIXOLYDIAN, "G4 A4 B4 C5 D5 E5 F5");
    scale("B", MUSIC_MODE_LOCRIAN, "B4 C5 D5 E5 F5 G5 A5");
    /* Accidentals come from the key, never respelled enharmonically. */
    scale("Eb", MUSIC_MODE_MAJOR, "Eb4 F4 G4 Ab4 Bb4 C5 D5");
    scale("F#", MUSIC_MODE_MAJOR, "F#4 G#4 A#4 B4 C#5 D#5 E#5");
    scale("Cb", MUSIC_MODE_MAJOR, "Cb4 Db4 Eb4 Fb4 Gb4 Ab4 Bb4");
    scale("G#", MUSIC_MODE_MAJOR, "G#4 A#4 B#4 C#5 D#5 E#5 F##5");
    scale("Fb", MUSIC_MODE_MAJOR, "Fb4 Gb4 Ab4 Bbb4 Cb5 Db5 Eb5");

    MusicScale c;
    MusicNote n;
    assert(musicKeyScale(key("C", MUSIC_MODE_MAJOR), &c) == MUSIC_OK && c.notes[0].pitch == 60);
    assert(musicScaleNote(&c, 4, &n) == MUSIC_OK && n.pitch == 67 && n.spelling.letter == 'G');
    assert(musicScaleNote(&c, 0, &n) == MUSIC_OK && musicScaleNote(&c, 6, &n) == MUSIC_OK && n.pitch == 71);
    assert(musicScaleNote(&c, 7, &n) == MUSIC_OUT_OF_RANGE && musicScaleNote(&c, -1, &n) == MUSIC_OUT_OF_RANGE);
    assert(musicScaleNote(NULL, 0, &n) == MUSIC_INVALID_ARGUMENT);

    MusicKey bad = {{'H', 0}, MUSIC_MODE_MAJOR, false};
    assert(musicKeyScale(bad, &c) == MUSIC_INVALID_ARGUMENT);
    bad = (MusicKey){{'C', 0}, (MusicMode)99, false};
    assert(musicKeyScale(bad, &c) == MUSIC_INVALID_ARGUMENT);
    assert(musicKeyScale(key("C", MUSIC_MODE_MAJOR), NULL) == MUSIC_INVALID_ARGUMENT);
}

static void relations(void) {
    related(musicDominantKey, "C", MUSIC_MODE_MAJOR, "G", MUSIC_MODE_MAJOR);
    related(musicDominantKey, "A", MUSIC_MODE_MINOR, "E", MUSIC_MODE_MINOR);
    related(musicDominantKey, "F#", MUSIC_MODE_MAJOR, "C#", MUSIC_MODE_MAJOR);
    related(musicDominantKey, "Bb", MUSIC_MODE_MAJOR, "F", MUSIC_MODE_MAJOR);
    related(musicDominantKey, "B#", MUSIC_MODE_MAJOR, "F##", MUSIC_MODE_MAJOR);
    related(musicSubdominantKey, "C", MUSIC_MODE_MAJOR, "F", MUSIC_MODE_MAJOR);
    related(musicSubdominantKey, "F", MUSIC_MODE_MAJOR, "Bb", MUSIC_MODE_MAJOR);
    related(musicSubdominantKey, "E", MUSIC_MODE_DORIAN, "A", MUSIC_MODE_DORIAN);
    related(musicRelativeMinorKey, "C", MUSIC_MODE_MAJOR, "A", MUSIC_MODE_MINOR);
    related(musicRelativeMinorKey, "Eb", MUSIC_MODE_MAJOR, "C", MUSIC_MODE_MINOR);
    related(musicRelativeMinorKey, "G", MUSIC_MODE_MAJOR, "E", MUSIC_MODE_MINOR);
    related(musicRelativeMinorKey, "A", MUSIC_MODE_MINOR, "A", MUSIC_MODE_MINOR);
    related(musicRelativeMinorKey, "D", MUSIC_MODE_DORIAN, "A", MUSIC_MODE_MINOR);
    related(musicRelativeMinorKey, "F", MUSIC_MODE_LYDIAN, "A", MUSIC_MODE_MINOR);
    related(musicRelativeMinorKey, "B", MUSIC_MODE_LOCRIAN, "A", MUSIC_MODE_MINOR);
    related(musicRelativeMinorKey, "Db", MUSIC_MODE_MAJOR, "Bb", MUSIC_MODE_MINOR);

    /* Derived keys keep "strict"; they can be nested. */
    MusicKey strict = key("C", MUSIC_MODE_MAJOR), result, nested;
    strict.strict = true;
    assert(musicDominantKey(strict, &result) == MUSIC_OK && result.strict);
    assert(musicRelativeMinorKey(result, &nested) == MUSIC_OK && nested.strict &&
           nested.tonic.letter == 'E' && nested.tonic.accidental == 0 && nested.mode == MUSIC_MODE_MINOR);

    MusicKey bad = {{'H', 0}, MUSIC_MODE_MAJOR, false};
    assert(musicDominantKey(bad, &result) == MUSIC_INVALID_ARGUMENT);
    assert(musicSubdominantKey(strict, NULL) == MUSIC_INVALID_ARGUMENT);
    assert(musicRelativeMinorKey(bad, &result) == MUSIC_INVALID_ARGUMENT);
}

int main(void) {
    scales();
    relations();
    return 0;
}
