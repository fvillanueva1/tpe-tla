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

/** How a note is written: letter 'A'-'G' and accidental -1 (flat), 0 or +1 (sharp). */
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

/**
 * A chord: its notes, in the order they were written or built. "root" is the
 * index of the root note in "notes" and "quality" its triad quality.
 */
typedef struct {
    MusicNote notes[MUSIC_MAX_CHORD_NOTES];
    size_t count;
    size_t root;
    MusicQuality quality;
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

typedef enum {
    MUSIC_VALID,
    MUSIC_INVALID_INPUT,
    MUSIC_TESSITURA_ERROR,
    MUSIC_VOICE_CROSSING_ERROR,
    MUSIC_PARALLEL_FIFTHS_ERROR,
    MUSIC_PARALLEL_OCTAVES_ERROR
} MusicValidationResult;

/* "rules" is a MusicRule mask. failedChord, when non-NULL, receives the zero-based index of the failing chord. */
MusicValidationResult musicCheckVoicing(const int (*chords)[VOICE_COUNT], size_t count,
                                        unsigned int rules, size_t *failedChord);

#endif
