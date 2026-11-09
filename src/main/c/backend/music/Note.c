#include "Music.h"
#include <stdio.h>
#include <string.h>

static const char letters[] = "CDEFGAB";
static const int naturals[] = {0, 2, 4, 5, 7, 9, 11};

/* Index of the letter in C D E F G A B, or -1. */
static int letterIndex(char letter) {
    const char *found = letter ? strchr(letters, letter) : NULL;
    return found ? (int)(found - letters) : -1;
}

static bool validSpelling(MusicSpelling spelling) {
    return letterIndex(spelling.letter) >= 0 && spelling.accidental >= -2 && spelling.accidental <= 2;
}

/* Reads the letter and the optional accidental; returns what follows them. */
static const char *readSpelling(const char *lexeme, MusicSpelling *spelling) {
    if (!lexeme || letterIndex(lexeme[0]) < 0) return NULL;
    spelling->letter = lexeme[0];
    spelling->accidental = lexeme[1] == '#' ? 1 : lexeme[1] == 'b' ? -1 : 0;
    return lexeme + 1 + (spelling->accidental != 0);
}

MusicStatus musicParseSpelling(const char *lexeme, MusicSpelling *spelling) {
    MusicSpelling parsed;
    const char *rest = readSpelling(lexeme, &parsed);
    if (!spelling || !rest || *rest) return MUSIC_INVALID_ARGUMENT;
    *spelling = parsed;
    return MUSIC_OK;
}

MusicStatus musicParseNote(const char *lexeme, MusicNote *note) {
    MusicSpelling spelling;
    const char *rest = readSpelling(lexeme, &spelling);
    if (!note || !rest) return MUSIC_INVALID_ARGUMENT;
    if (!*rest) return MUSIC_MISSING_OCTAVE;
    if (*rest < '0' || *rest > '8' || rest[1]) return MUSIC_INVALID_ARGUMENT;
    return musicNoteFromSpelling(spelling, *rest - '0', note);
}

MusicStatus musicParseInterval(const char *lexeme, MusicInterval *interval) {
    static const struct { const char *name; MusicInterval value; } intervals[] = {
        {"P1", {0, 0}}, {"m2", {1, 1}}, {"M2", {2, 1}}, {"m3", {3, 2}}, {"M3", {4, 2}},
        {"P4", {5, 3}}, {"aug4", {6, 3}}, {"d5", {6, 4}}, {"P5", {7, 4}}, {"m6", {8, 5}},
        {"M6", {9, 5}}, {"m7", {10, 6}}, {"M7", {11, 6}}, {"P8", {12, 7}}
    };
    if (!lexeme || !interval) return MUSIC_INVALID_ARGUMENT;
    for (size_t i = 0; i < sizeof(intervals) / sizeof(intervals[0]); ++i)
        if (!strcmp(lexeme, intervals[i].name)) { *interval = intervals[i].value; return MUSIC_OK; }
    return MUSIC_INVALID_ARGUMENT;
}

int musicPitchClass(MusicSpelling spelling) {
    if (!validSpelling(spelling)) return -1;
    return (naturals[letterIndex(spelling.letter)] + spelling.accidental + 12) % 12;
}

MusicStatus musicNoteFromSpelling(MusicSpelling spelling, int octave, MusicNote *note) {
    if (!note || !validSpelling(spelling) || octave < -1 || octave > 9) return MUSIC_INVALID_ARGUMENT;
    int pitch = (octave + 1) * 12 + naturals[letterIndex(spelling.letter)] + spelling.accidental;
    if (pitch < MUSIC_LOWEST_PITCH || pitch > MUSIC_HIGHEST_PITCH) return MUSIC_OUT_OF_RANGE;
    note->pitch = pitch;
    note->spelling = spelling;
    return MUSIC_OK;
}

MusicStatus musicNoteFromPitch(int pitch, MusicNote *note) {
    static const MusicSpelling sharps[] = {
        {'C', 0}, {'C', 1}, {'D', 0}, {'D', 1}, {'E', 0}, {'F', 0},
        {'F', 1}, {'G', 0}, {'G', 1}, {'A', 0}, {'A', 1}, {'B', 0}
    };
    if (!note) return MUSIC_INVALID_ARGUMENT;
    if (pitch < MUSIC_LOWEST_PITCH || pitch > MUSIC_HIGHEST_PITCH) return MUSIC_OUT_OF_RANGE;
    note->pitch = pitch;
    note->spelling = sharps[pitch % 12];
    return MUSIC_OK;
}

int musicNoteOctave(MusicNote note) {
    if (!validSpelling(note.spelling)) return MUSIC_INVALID_OCTAVE;
    /* The unwrapped class of Cb is -1 and that of B# is 12, so they stay in their written octave. */
    int written = naturals[letterIndex(note.spelling.letter)] + note.spelling.accidental;
    return (note.pitch - written) / 12 - 1;
}

MusicStatus musicTransposeNote(MusicNote note, MusicInterval interval, bool down, MusicNote *result) {
    if (!result || !validSpelling(note.spelling) || interval.semitones < 0 || interval.steps < 0 ||
        note.pitch < MUSIC_LOWEST_PITCH || note.pitch > MUSIC_HIGHEST_PITCH) return MUSIC_INVALID_ARGUMENT;
    int direction = down ? -1 : 1;
    int pitch = note.pitch + direction * interval.semitones;
    if (pitch < MUSIC_LOWEST_PITCH || pitch > MUSIC_HIGHEST_PITCH) return MUSIC_OUT_OF_RANGE;
    /* Move the letter by the steps; crossing B to C (or C to B going down) changes the octave. */
    int index = letterIndex(note.spelling.letter) + direction * interval.steps;
    int octave = musicNoteOctave(note) + (index >= 0 ? index / 7 : -((6 - index) / 7));
    index = ((index % 7) + 7) % 7;
    int accidental = pitch - ((octave + 1) * 12 + naturals[index]);
    if (accidental < -2 || accidental > 2) return musicNoteFromPitch(pitch, result);
    result->pitch = pitch;
    result->spelling.letter = letters[index];
    result->spelling.accidental = (signed char)accidental;
    return MUSIC_OK;
}

MusicStatus musicIntervalBetween(MusicNote from, MusicNote to, MusicInterval *interval, bool *down) {
    if (!interval || !down || !validSpelling(from.spelling) || !validSpelling(to.spelling)) return MUSIC_INVALID_ARGUMENT;
    int semitones = to.pitch - from.pitch;
    int steps = letterIndex(to.spelling.letter) + 7 * musicNoteOctave(to) -
        letterIndex(from.spelling.letter) - 7 * musicNoteOctave(from);
    /* B#3 -> Cb4 goes up a letter but down a semitone: no interval says that. */
    if ((semitones < 0 && steps > 0) || (semitones > 0 && steps < 0)) return MUSIC_INVALID_ARGUMENT;
    *down = semitones < 0 || steps < 0;
    interval->semitones = semitones < 0 ? -semitones : semitones;
    interval->steps = steps < 0 ? -steps : steps;
    return MUSIC_OK;
}

void musicFormatNote(MusicNote note, char text[MUSIC_NOTE_TEXT_SIZE]) {
    static const char *accidentals[] = {"bb", "b", "", "#", "##"};
    if (!text) return;
    if (!validSpelling(note.spelling)) { text[0] = '\0'; return; }
    snprintf(text, MUSIC_NOTE_TEXT_SIZE, "%c%s%d", note.spelling.letter,
             accidentals[note.spelling.accidental + 2], musicNoteOctave(note));
}
