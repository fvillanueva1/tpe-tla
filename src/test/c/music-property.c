#include "../../main/c/backend/music/Music.h"
#include <assert.h>
#include <string.h>

static MusicKey key(const char *tonic, MusicMode mode) {
    MusicKey result = {.mode = mode};
    assert(musicParseSpelling(tonic, &result.tonic) == MUSIC_OK);
    return result;
}

static MusicNote note(const char *lexeme) {
    MusicNote result;
    assert(musicParseNote(lexeme, &result) == MUSIC_OK);
    return result;
}

static MusicChord chord(const char *const *lexemes, size_t count) {
    MusicNote notes[MUSIC_MAX_CHORD_NOTES];
    MusicChord result;
    for (size_t i = 0; i < count; ++i) notes[i] = note(lexemes[i]);
    assert(musicChordFromNotes(notes, count, &result) == MUSIC_OK);
    return result;
}

static void properties(void) {
    MusicNote n;
    char text[MUSIC_NOTE_TEXT_SIZE];
    /* accept/properties.cad: chord acorde = {C4, E4, G4}; note raiz = acorde.root; */
    MusicChord c = chord((const char *[]){"C4", "E4", "G4"}, 3);
    assert(musicChordRoot(&c, &n) == MUSIC_OK && n.pitch == 60);
    assert(musicChordQuality(&c) == MUSIC_QUALITY_MAJOR);
    MusicChord inverted = chord((const char *[]){"F4", "A4", "D5"}, 3);
    assert(musicChordRoot(&inverted, &n) == MUSIC_OK);
    musicFormatNote(n, text);
    assert(!strcmp(text, "D5") && musicChordQuality(&inverted) == MUSIC_QUALITY_MINOR);
    MusicChord degree;
    assert(musicDegreeChord(key("C", MUSIC_MODE_MAJOR), 6, MUSIC_TRIAD, &degree) == MUSIC_OK);
    assert(musicChordQuality(&degree) == MUSIC_QUALITY_DIMINISHED);
    MusicChord empty = {.count = 0};
    assert(musicChordRoot(&empty, &n) == MUSIC_INVALID_ARGUMENT && musicChordRoot(NULL, &n) == MUSIC_INVALID_ARGUMENT);
    assert(musicChordQuality(&empty) == MUSIC_QUALITY_OTHER && musicChordQuality(NULL) == MUSIC_QUALITY_OTHER);

    assert(musicNoteOctave(note("G3")) == 3 && musicNoteOctave(note("Cb5")) == 5);

    assert(musicKeyTonic(key("Bb", MUSIC_MODE_MINOR), &n) == MUSIC_OK && n.pitch == 70);
    musicFormatNote(n, text);
    assert(!strcmp(text, "Bb4"));
    assert(musicKeyTonic(key("Cb", MUSIC_MODE_MAJOR), &n) == MUSIC_OK && n.pitch == 59);
    assert(musicKeyMode(key("D", MUSIC_MODE_DORIAN)) == MUSIC_MODE_DORIAN);
    MusicKey bad = {{'H', 0}, MUSIC_MODE_MAJOR, false};
    assert(musicKeyTonic(bad, &n) == MUSIC_INVALID_ARGUMENT && musicKeyTonic(key("C", MUSIC_MODE_MAJOR), NULL) == MUSIC_INVALID_ARGUMENT);

    MusicProgression p;
    assert(musicProgressionInit(&p, 3) && p.count == 3);
    musicProgressionDestroy(&p);
}

static void names(void) {
    assert(!strcmp(musicModeName(MUSIC_MODE_MAJOR), "major") && !strcmp(musicModeName(MUSIC_MODE_PHRYGIAN), "phrygian"));
    assert(!strcmp(musicModeName(MUSIC_MODE_DORIAN), "dorian") && !musicModeName((MusicMode)99));
    assert(!strcmp(musicQualityName(MUSIC_QUALITY_DIMINISHED), "diminished") && !musicQualityName((MusicQuality)99));
    assert(!strcmp(musicQualityName(MUSIC_QUALITY_OTHER), "other"));
    assert(!strcmp(musicDurationName(MUSIC_DURATION_EIGHTH), "eighth") && !musicDurationName((MusicDuration)99));
    assert(!strcmp(musicDurationName(MUSIC_DURATION_WHOLE), "whole"));
}

static void membership(void) {
    MusicScale scale;
    assert(musicKeyScale(key("C", MUSIC_MODE_MAJOR), &scale) == MUSIC_OK);
    /* accept/40-if-membership.cad: E4 in scale of tonalidad. */
    assert(musicNoteInScale(note("E4"), &scale) && musicNoteInScale(note("E7"), &scale));
    assert(musicNoteInScale(note("Fb4"), &scale) && !musicNoteInScale(note("F#4"), &scale));
    assert(!musicNoteInScale(note("C4"), NULL) && !musicNoteInScale((MusicNote){5, {'F', 0}}, &scale));

    MusicChord c = chord((const char *[]){"C4", "E4", "G4"}, 3);
    assert(musicNoteInChord(note("G4"), &c) && musicNoteInChord(note("C6"), &c) && !musicNoteInChord(note("D4"), &c));
    assert(!musicNoteInChord(note("C4"), NULL));

    MusicKey g = key("G", MUSIC_MODE_MAJOR);
    g.strict = true;
    assert(musicNoteInKey(note("F#4"), g) && musicNoteInKey(note("Gb4"), g) && !musicNoteInKey(note("F4"), g));
    MusicKey bb = key("Bb", MUSIC_MODE_MAJOR);
    assert(musicNoteInKey(note("Bb2"), bb) && musicNoteInKey(note("Eb5"), bb) && !musicNoteInKey(note("B4"), bb));
    MusicKey bad = {{'H', 0}, MUSIC_MODE_MAJOR, false};
    assert(!musicNoteInKey(note("C4"), bad));

    MusicChord gChord = chord((const char *[]){"G3", "B3", "D4", "F#4"}, 4);
    assert(musicChordInKey(&gChord, g) && !musicChordInKey(&c, key("D", MUSIC_MODE_MAJOR)));
    /* A degree chord is always in its key (natural minor: E G B D); with the raised third (G#) it is not. */
    MusicChord dominant;
    assert(musicDegreeChord(key("A", MUSIC_MODE_MINOR), 4, MUSIC_SEVENTH, &dominant) == MUSIC_OK);
    assert(musicChordInKey(&dominant, key("A", MUSIC_MODE_MINOR)));
    MusicChord raised = chord((const char *[]){"E4", "G#4", "B4"}, 3);
    assert(!musicChordInKey(&raised, key("A", MUSIC_MODE_MINOR)));
    MusicChord empty = {.count = 0};
    assert(!musicChordInKey(&empty, g) && !musicChordInKey(NULL, g));
}

int main(void) {
    properties();
    names();
    membership();
    return 0;
}
