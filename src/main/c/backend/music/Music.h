#ifndef MUSIC_HEADER
#define MUSIC_HEADER

/**
 * Cadence music library. Plain C with no dependency on the AST: every
 * function returns a result or an error code and prints nothing, so the
 * caller (semantic analysis or runtime) decides the message.
 *
 * Pitches are MIDI numbers (C4 = 60). The language admits notes from C0 (12)
 * to B8 (119).
 */

#include <stdbool.h>
#include <stddef.h>

#define MUSIC_LOWEST_PITCH 12
#define MUSIC_HIGHEST_PITCH 119

/** Same order as the AST's Mode, so the semantic analysis can convert by value. */
typedef enum {
    MUSIC_MODE_DORIAN,
    MUSIC_MODE_LOCRIAN,
    MUSIC_MODE_LYDIAN,
    MUSIC_MODE_MAJOR,
    MUSIC_MODE_MINOR,
    MUSIC_MODE_MIXOLYDIAN,
    MUSIC_MODE_PHRYGIAN
} MusicMode;

/** Bit flags with the same values as the AST's CheckRule. */
typedef enum {
    MUSIC_RULE_PARALLEL_FIFTHS = 1,
    MUSIC_RULE_PARALLEL_OCTAVES = 2,
    MUSIC_RULE_VOICE_CROSSING = 4
} MusicRule;

/** Note lengths, from the longest to the shortest. */
typedef enum {
    MUSIC_DURATION_WHOLE,
    MUSIC_DURATION_HALF,
    MUSIC_DURATION_QUARTER,
    MUSIC_DURATION_EIGHTH,
    MUSIC_DURATION_SIXTEENTH
} MusicDuration;

/** Length of a note, chord or chord of a progression when the program does not give one. */
#define MUSIC_DEFAULT_DURATION MUSIC_DURATION_QUARTER

/** Result of the operations that can fail on their input. */
typedef enum {
    MUSIC_OK,
    MUSIC_INVALID_ARGUMENT,
    MUSIC_MISSING_OCTAVE,
    MUSIC_OUT_OF_RANGE,
    MUSIC_TOO_MANY_NOTES,
    MUSIC_NO_MEMORY,
    /* "voice" found no SATB distribution within the tessituras. */
    MUSIC_NO_REALIZATION,
    /* A chord of an explicit voicing does not have exactly four notes. */
    MUSIC_NOT_FOUR_VOICES,
    /* "modulate" got a progression whose chords do not say which key they are in. */
    MUSIC_MISSING_KEY
} MusicStatus;

/**
 * How a note is written: letter 'A'-'G' and accidental from -2 to +2 (flats
 * are negative). The language writes -1, 0 and +1; transposition can produce
 * double accidentals (E#4 + M3 = G##4).
 */
typedef struct {
    char letter;
    signed char accidental;
} MusicSpelling;

/** A note: its MIDI pitch and its spelling (Cb4 and B3 share the pitch 59). */
typedef struct {
    int pitch;
    MusicSpelling spelling;
} MusicNote;

/** An interval: semitones and diatonic steps (aug4 = 6 / 3, d5 = 6 / 4). */
typedef struct {
    int semitones;
    int steps;
} MusicInterval;

/** A key: tonic without octave, mode and whether it is strict. */
typedef struct {
    MusicSpelling tonic;
    MusicMode mode;
    bool strict;
} MusicKey;

typedef struct {
    MusicNote notes[7];
} MusicScale;

typedef enum {
    MUSIC_QUALITY_MAJOR,
    MUSIC_QUALITY_MINOR,
    MUSIC_QUALITY_DIMINISHED,
    MUSIC_QUALITY_AUGMENTED,
    MUSIC_QUALITY_OTHER
} MusicQuality;

/** Maximum number of notes in a chord, repeated notes included. */
#define MUSIC_MAX_CHORD_NOTES 16

/** Stacked thirds of a chord built on a degree; 0 for a chord written by notes. */
typedef enum { MUSIC_TRIAD = 3, MUSIC_SEVENTH = 4, MUSIC_NINTH = 5 } MusicChordSize;

/**
 * A chord: its notes, in the order they were written or built. "root" is the
 * index of the root note in "notes" and "quality" its triad quality.
 *
 * A chord built on a degree remembers it ("size" != 0, "degree" 0-6) and its
 * key ("hasKey"), so "modulate" can rebuild it on the same degree of another
 * key. Transformations that change the notes freely (inversion) forget the
 * degree but keep the key.
 */
typedef struct {
    MusicNote notes[MUSIC_MAX_CHORD_NOTES];
    size_t count;
    size_t root;
    MusicQuality quality;
    MusicChordSize size;
    int degree;
    bool hasKey;
    MusicKey key;
} MusicChord;

/** A chord of a progression and how long it lasts. */
typedef struct {
    MusicChord chord;
    MusicDuration duration;
} MusicStep;

/** Owns its steps. An empty progression has steps == NULL and count == 0. */
typedef struct {
    MusicStep *steps;
    size_t count;
} MusicProgression;

/* Voices of a SATB chord, ordered soprano, alto, tenor, bass. */
enum { VOICE_SOPRANO, VOICE_ALTO, VOICE_TENOR, VOICE_BASS, VOICE_COUNT };

/** Owns its arrays: one MIDI pitch per voice and one duration per chord. */
typedef struct {
    int (*chords)[VOICE_COUNT];
    MusicDuration *durations;
    size_t count;
} MusicVoicing;

/*
 * Memory. "Init" leaves "count" zeroed steps or chords; on failure (false)
 * the value is left empty. "Copy" is deep and leaves "target" empty on
 * failure. "Destroy" releases the arrays and can be called more than once.
 */
bool musicProgressionInit(MusicProgression *progression, size_t count);
bool musicProgressionCopy(const MusicProgression *source, MusicProgression *target);
void musicProgressionDestroy(MusicProgression *progression);
bool musicVoicingInit(MusicVoicing *voicing, size_t count);
bool musicVoicingCopy(const MusicVoicing *source, MusicVoicing *target);
void musicVoicingDestroy(MusicVoicing *voicing);

/*
 * Notes and intervals. A spelled octave can fall outside 0-8 while the pitch
 * is in range (Cb9 = B8), so the range is always checked on the pitch.
 */

/** Parses a note lexeme with octave ("C#4"). MUSIC_MISSING_OCTAVE for "C#". */
MusicStatus musicParseNote(const char *lexeme, MusicNote *note);
/** Parses a spelling without octave ("Bb"), as a key tonic is written. */
MusicStatus musicParseSpelling(const char *lexeme, MusicSpelling *spelling);
/** Parses an interval lexeme ("P5", "aug4"). */
MusicStatus musicParseInterval(const char *lexeme, MusicInterval *interval);

/** Pitch class from 0 (C) to 11 (B); Cb is 11 and B# is 0. */
int musicPitchClass(MusicSpelling spelling);
/** The note with this spelling in the given written octave (Cb4 = 59). */
MusicStatus musicNoteFromSpelling(MusicSpelling spelling, int octave, MusicNote *note);
/** The note of a pitch, spelled with sharps (61 = C#4). */
MusicStatus musicNoteFromPitch(int pitch, MusicNote *note);
/** Written octave: 4 for Cb4 (pitch 59) and 3 for B#3 (pitch 60); MUSIC_INVALID_OCTAVE for a bad spelling. */
#define MUSIC_INVALID_OCTAVE (-100)
int musicNoteOctave(MusicNote note);

/**
 * Moves the note up or down by the interval, keeping a correct spelling:
 * the letter moves by the interval's steps (C4 + aug4 = F#4, C4 + d5 = Gb4).
 * MUSIC_OUT_OF_RANGE when the resulting pitch leaves C0-B8.
 */
MusicStatus musicTransposeNote(MusicNote note, MusicInterval interval, bool down, MusicNote *result);

/** Longest text of a note: "C##-1" plus the terminator. */
#define MUSIC_NOTE_TEXT_SIZE 8
/** Writes the note as "C#4", "Bb3" or "G##4". */
void musicFormatNote(MusicNote note, char text[MUSIC_NOTE_TEXT_SIZE]);
/**
 * The spelled interval that takes "from" to "to" (C4 -> G4 = P5 up,
 * C4 -> A3 = m3 down), so transposing "from" by it gives "to" with its
 * spelling. Compound distances keep their octaves (C4 -> D5 = 14 / 8).
 */
MusicStatus musicIntervalBetween(MusicNote from, MusicNote to, MusicInterval *interval, bool *down);

/*
 * Keys and scales. A scale starts at the tonic in octave 4 (as "export" has
 * always done) and spells each degree with the next letter: G major has F#,
 * F major has Bb. The derived keys keep the mode and "strict" of the original,
 * except the relative minor, which is minor.
 */

/** The seven notes of the key, ascending from its tonic in octave 4. */
MusicStatus musicKeyScale(MusicKey key, MusicScale *scale);
/** Zero-based access, as "(scale of k)[i]"; MUSIC_OUT_OF_RANGE outside 0-6. */
MusicStatus musicScaleNote(const MusicScale *scale, int index, MusicNote *note);
/** "dominant of k": the tonic a fifth above (C major -> G major, A minor -> E minor). */
MusicStatus musicDominantKey(MusicKey key, MusicKey *result);
/** "subdominant of k": the tonic a fourth above (C major -> F major). */
MusicStatus musicSubdominantKey(MusicKey key, MusicKey *result);
/**
 * "relative minor of k": the minor key with the same notes. For major it is
 * the sixth degree (C major -> A minor); for the other modes, the degree
 * whose mode is minor (D dorian -> A minor); a minor key is its own relative.
 */
MusicStatus musicRelativeMinorKey(MusicKey key, MusicKey *result);

/*
 * Chords. A degree is zero-based (0 = I). Its case and the "dim" suffix are
 * only for reading: the quality always comes from the key, so "ii", "II" and
 * "iidim" are the same degree.
 */

/** Parses a degree lexeme ("V", "ii", "viidim") into 0-6. */
MusicStatus musicParseDegree(const char *lexeme, int *degree);
/**
 * "triad on V of k" and the like: thirds of the scale stacked from the degree,
 * ascending from the scale note (octave 4 or 5), with the next octave when they
 * pass the seventh degree. MUSIC_OUT_OF_RANGE for a degree outside 0-6.
 */
MusicStatus musicDegreeChord(MusicKey key, int degree, MusicChordSize size, MusicChord *chord);
/**
 * "{C4, E4, G4}": keeps the order and the repeated notes. The root is the
 * lowest note that has a third and a perfect fifth above it in the chord; if
 * none has, the lowest with a third and any fifth; if none, the lowest note.
 * MUSIC_INVALID_ARGUMENT when empty, MUSIC_TOO_MANY_NOTES over
 * MUSIC_MAX_CHORD_NOTES.
 */
MusicStatus musicChordFromNotes(const MusicNote *notes, size_t count, MusicChord *chord);
/** Bit i is set when pitch class i is in the chord (bit 0 = C), as musicRealizeSATB expects. */
unsigned short musicChordMask(const MusicChord *chord);
/** Number of different pitch classes. */
int musicChordClassCount(const MusicChord *chord);

/*
 * Properties of the language: "n.octave" is musicNoteOctave and "p.length"
 * is the progression's count.
 */

/** "c.root": the root note of the chord. */
MusicStatus musicChordRoot(const MusicChord *chord, MusicNote *root);
/** "c.quality". MUSIC_QUALITY_OTHER for an empty or invalid chord. */
MusicQuality musicChordQuality(const MusicChord *chord);
/** "k.tonic": the tonic in octave 4, the octave of "scale of k". */
MusicStatus musicKeyTonic(MusicKey key, MusicNote *tonic);
/** "k.mode". */
MusicMode musicKeyMode(MusicKey key);
/** Names that "log" prints: "major", "dorian", "diminished", ... NULL for an unknown value. */
const char *musicModeName(MusicMode mode);
const char *musicQualityName(MusicQuality quality);
const char *musicDurationName(MusicDuration duration);

/*
 * Membership, by pitch class: E5 is in the scale of C major although the
 * scale is written in octave 4, and Fb4 counts as E4, as "==" compares
 * pitches.
 */

/** "n in scale of k". */
bool musicNoteInScale(MusicNote note, const MusicScale *scale);
/** "n in c". */
bool musicNoteInChord(MusicNote note, const MusicChord *chord);
/** "strict": the note belongs to the key. */
bool musicNoteInKey(MusicNote note, MusicKey key);
/** "strict": every note of the chord belongs to the key; false for an empty chord. */
bool musicChordInKey(const MusicChord *chord, MusicKey key);

/*
 * Progressions. Every function builds a new progression in "result", which
 * the caller destroys, and leaves it empty on failure. "result" must be a
 * different variable from the inputs, and what it held before is not freed:
 * for "p = p + q" the caller builds into a temporary, destroys p and moves it.
 */

/** "[I, V, I] in k": a triad on each zero-based degree, a quarter each. */
MusicStatus musicDegreeProgression(MusicKey key, const int *degrees, size_t count, MusicProgression *result);
/** "[c1, c2]": each chord lasting a quarter. */
MusicStatus musicChordProgression(const MusicChord *chords, size_t count, MusicProgression *result);
/** "p1 + p2". */
MusicStatus musicConcatenate(const MusicProgression *first, const MusicProgression *second, MusicProgression *result);
/** "p + c": the chord is appended lasting a quarter. */
MusicStatus musicAppendChord(const MusicProgression *progression, const MusicChord *chord, MusicProgression *result);
/** "p * n": n >= 0 (MUSIC_OUT_OF_RANGE otherwise); 0 gives an empty progression. */
MusicStatus musicRepeat(const MusicProgression *progression, int times, MusicProgression *result);

/*
 * Transformations. A failure leaves the result untouched (or empty, for a
 * progression).
 */

/** "c + M3" / "c - M3": every note, keeping spelling, degree and key (the key moves too). */
MusicStatus musicTransposeChord(const MusicChord *chord, MusicInterval interval, bool down, MusicChord *result);
/** "p + M3" / "p - M3": every chord. */
MusicStatus musicTransposeProgression(const MusicProgression *progression, MusicInterval interval, bool down,
                                      MusicProgression *result);
/** The key with its tonic moved by the interval, same mode. */
MusicStatus musicTransposeKey(MusicKey key, MusicInterval interval, bool down, MusicKey *result);
/**
 * "invert c by n": n times, the lowest note goes up as many octaves as needed
 * to be above the highest one. The result is written from the lowest note up.
 * n goes from 0 to the number of notes (MUSIC_OUT_OF_RANGE otherwise, also when
 * a note leaves C0-B8). The root and quality stay; the degree is forgotten.
 */
MusicStatus musicInvertChord(const MusicChord *chord, int times, MusicChord *result);
/** "arpeggiate c as d": the notes from the lowest up, one after the other, each lasting "duration". */
MusicStatus musicArpeggiate(const MusicChord *chord, MusicDuration duration, MusicProgression *result);
/**
 * "modulate p to k": keeps the harmonic function. A chord built on a degree is
 * rebuilt on the same degree of "key" ([I, V] in C major to A minor gives
 * [i, v] in A minor). Any other chord moves by the interval from the tonic of
 * its key to the tonic of "key"; a chord without a key uses the key of the
 * first chord of the progression that has one. MUSIC_MISSING_KEY when no
 * chord has a key. Durations stay.
 */
MusicStatus musicModulate(const MusicProgression *progression, MusicKey key, MusicProgression *result);

/*
 * SATB voicings.
 */

/**
 * "voice p as SATB": distributes every chord in four voices within the
 * tessituras, without crossings and moving the voices as little as possible
 * (musicRealizeSATB). A chord with more than four pitch classes first loses
 * its fifth and then its seventh (a ninth keeps root, third, seventh and
 * ninth); if it still has more than four, MUSIC_TOO_MANY_NOTES.
 * MUSIC_NO_REALIZATION when a chord does not fit the tessituras.
 * Durations stay. "result" is left empty on failure.
 */
MusicStatus musicVoiceProgression(const MusicProgression *progression, MusicVoicing *result);
/**
 * "voicing v = [{C5, G4, E4, C3}, ...]": each chord gives soprano, alto, tenor
 * and bass in that order, with no redistribution, a quarter each.
 * MUSIC_NOT_FOUR_VOICES when a chord does not have exactly four notes.
 */
MusicStatus musicExplicitVoicing(const MusicChord *chords, size_t count, MusicVoicing *result);
/** "check v for ...": musicCheckVoicing over a voicing value. */
typedef enum {
    MUSIC_VALID,
    MUSIC_INVALID_INPUT,
    MUSIC_TESSITURA_ERROR,
    MUSIC_VOICE_CROSSING_ERROR,
    MUSIC_PARALLEL_FIFTHS_ERROR,
    MUSIC_PARALLEL_OCTAVES_ERROR
} MusicValidationResult;
MusicValidationResult musicCheckVoicingValue(const MusicVoicing *voicing, unsigned int rules, size_t *failedChord);

/* Compute seven ascending MIDI pitches starting at the given tonic. */
bool musicScale(int tonic, MusicMode mode, int notes[7]);
bool musicInKey(const int *pitches, size_t count, int tonic, MusicMode mode);

/* Chords are 12-bit pitch-class masks (bit 0 = C); output is ordered SATB. */
typedef enum { MUSIC_VOICE_SUCCESS, MUSIC_VOICE_INVALID_CHORD, MUSIC_VOICE_NO_MEMORY } MusicVoiceResult;
MusicVoiceResult musicRealizeSATB(const unsigned short *chords, size_t count, int (*voices)[VOICE_COUNT]);
bool musicInTessitura(const int voices[VOICE_COUNT]);
bool musicWithoutVoiceCrossing(const int voices[VOICE_COUNT]);
bool musicWithoutParallelFifths(const int first[VOICE_COUNT], const int second[VOICE_COUNT]);
bool musicWithoutParallelOctaves(const int first[VOICE_COUNT], const int second[VOICE_COUNT]);

/* "rules" is a MusicRule mask. failedChord, when non-NULL, receives the zero-based index of the failing chord. */
MusicValidationResult musicCheckVoicing(const int (*chords)[VOICE_COUNT], size_t count,
                                        unsigned int rules, size_t *failedChord);

#endif
