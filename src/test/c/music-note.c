#include "../../main/c/backend/music/Music.h"
#include <assert.h>
#include <string.h>

static MusicNote note(const char *lexeme) {
    MusicNote result;
    assert(musicParseNote(lexeme, &result) == MUSIC_OK);
    return result;
}

static MusicInterval interval(const char *lexeme) {
    MusicInterval result;
    assert(musicParseInterval(lexeme, &result) == MUSIC_OK);
    return result;
}

/* Transposes and compares with the expected text ("C#4", "G##4"). */
static void transposes(const char *from, const char *by, bool down, const char *expected) {
    MusicNote result;
    char text[MUSIC_NOTE_TEXT_SIZE];
    assert(musicTransposeNote(note(from), interval(by), down, &result) == MUSIC_OK);
    musicFormatNote(result, text);
    assert(!strcmp(text, expected));
}

static void parsing(void) {
    MusicNote n;
    assert(note("C4").pitch == 60 && note("A4").pitch == 69 && note("C#4").pitch == 61 && note("Db4").pitch == 61);
    assert(note("C0").pitch == MUSIC_LOWEST_PITCH && note("B8").pitch == MUSIC_HIGHEST_PITCH);
    n = note("Bb3");
    assert(n.pitch == 58 && n.spelling.letter == 'B' && n.spelling.accidental == -1);
    /* Cb and B# cross the octave boundary but keep their written octave. */
    assert(note("Cb4").pitch == 59 && note("B#3").pitch == 60);
    assert(musicNoteOctave(note("Cb4")) == 4 && musicNoteOctave(note("B#3")) == 3 && musicNoteOctave(note("C0")) == 0);

    assert(musicParseNote("Cb0", &n) == MUSIC_OUT_OF_RANGE && musicParseNote("B#8", &n) == MUSIC_OUT_OF_RANGE);
    assert(musicParseNote("C#", &n) == MUSIC_MISSING_OCTAVE && musicParseNote("C", &n) == MUSIC_MISSING_OCTAVE);
    assert(musicParseNote("H4", &n) == MUSIC_INVALID_ARGUMENT && musicParseNote("C9", &n) == MUSIC_INVALID_ARGUMENT);
    assert(musicParseNote("C44", &n) == MUSIC_INVALID_ARGUMENT && musicParseNote("", &n) == MUSIC_INVALID_ARGUMENT);
    assert(musicParseNote(NULL, &n) == MUSIC_INVALID_ARGUMENT && musicParseNote("C4", NULL) == MUSIC_INVALID_ARGUMENT);

    MusicSpelling s;
    assert(musicParseSpelling("F#", &s) == MUSIC_OK && s.letter == 'F' && s.accidental == 1);
    assert(musicParseSpelling("G", &s) == MUSIC_OK && s.accidental == 0);
    assert(musicParseSpelling("G4", &s) == MUSIC_INVALID_ARGUMENT && musicParseSpelling("X", &s) == MUSIC_INVALID_ARGUMENT);

    MusicInterval i;
    assert(interval("P1").semitones == 0 && interval("P8").semitones == 12 && interval("P8").steps == 7);
    assert(interval("aug4").semitones == interval("d5").semitones);
    assert(interval("aug4").steps == 3 && interval("d5").steps == 4);
    assert(musicParseInterval("P3", &i) == MUSIC_INVALID_ARGUMENT && musicParseInterval(NULL, &i) == MUSIC_INVALID_ARGUMENT);
}

static void classes(void) {
    assert(musicPitchClass((MusicSpelling){'C', 0}) == 0 && musicPitchClass((MusicSpelling){'B', 0}) == 11);
    assert(musicPitchClass((MusicSpelling){'C', -1}) == 11 && musicPitchClass((MusicSpelling){'B', 1}) == 0);
    assert(musicPitchClass((MusicSpelling){'H', 0}) == -1 && musicPitchClass((MusicSpelling){'C', 3}) == -1);

    MusicNote n;
    assert(musicNoteFromSpelling((MusicSpelling){'C', -1}, 4, &n) == MUSIC_OK && n.pitch == 59);
    assert(musicNoteFromSpelling((MusicSpelling){'C', -1}, 0, &n) == MUSIC_OUT_OF_RANGE);
    assert(musicNoteFromSpelling((MusicSpelling){'C', 0}, 10, &n) == MUSIC_INVALID_ARGUMENT);
    assert(musicNoteFromPitch(61, &n) == MUSIC_OK && n.spelling.letter == 'C' && n.spelling.accidental == 1);
    assert(musicNoteFromPitch(11, &n) == MUSIC_OUT_OF_RANGE && musicNoteFromPitch(120, &n) == MUSIC_OUT_OF_RANGE);
    assert(musicNoteOctave((MusicNote){60, {'H', 0}}) == MUSIC_INVALID_OCTAVE);

    char text[MUSIC_NOTE_TEXT_SIZE];
    musicFormatNote(note("Bb3"), text);
    assert(!strcmp(text, "Bb3"));
    musicFormatNote((MusicNote){12, {'B', 1}}, text);
    assert(!strcmp(text, "B#-1"));
    musicFormatNote((MusicNote){60, {'H', 0}}, text);
    assert(!strcmp(text, ""));
}

static void transposition(void) {
    transposes("C4", "M3", false, "E4");
    transposes("C4", "aug4", false, "F#4");
    transposes("C4", "d5", false, "Gb4");
    transposes("A4", "m3", false, "C5");
    transposes("B3", "m2", false, "C4");
    transposes("C4", "m2", true, "B3");
    transposes("E4", "M3", true, "C4");
    transposes("C4", "P8", true, "C3");
    transposes("D4", "P1", false, "D4");
    /* Spellings that need double accidentals or that cross octaves. */
    transposes("E#4", "M3", false, "G##4");
    transposes("Cb4", "d5", false, "Gbb4");
    transposes("Cb4", "P8", false, "Cb5");
    transposes("B#3", "m2", true, "A##3");

    MusicNote result = {0, {'C', 0}};
    assert(musicTransposeNote(note("B8"), interval("m2"), false, &result) == MUSIC_OUT_OF_RANGE);
    assert(musicTransposeNote(note("C0"), interval("m2"), true, &result) == MUSIC_OUT_OF_RANGE);
    assert(result.pitch == 0);
    assert(musicTransposeNote(note("G8"), interval("M3"), false, &result) == MUSIC_OK && result.pitch == 119);
    MusicInterval negative = {-1, 0};
    assert(musicTransposeNote(note("C4"), negative, false, &result) == MUSIC_INVALID_ARGUMENT);
    assert(musicTransposeNote(note("C4"), interval("P5"), false, NULL) == MUSIC_INVALID_ARGUMENT);
}

int main(void) {
    parsing();
    classes();
    transposition();
    return 0;
}
