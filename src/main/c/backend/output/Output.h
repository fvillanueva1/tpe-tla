#ifndef OUTPUT_HEADER
#define OUTPUT_HEADER

/**
 * Output of "export": the runtime turns the exported value into a list of
 * note events (a score) and a writer turns the score into a file. Writers do
 * not know the language; they only write events.
 */

#include "../music/Music.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** Ticks per quarter note, as in the MIDI header (a quarter lasts 480 ticks). */
#define OUTPUT_TICKS_PER_QUARTER 480u
/** Tempo and instrument when "export" does not give them. */
#define OUTPUT_DEFAULT_TEMPO 120
#define OUTPUT_DEFAULT_INSTRUMENT 0

/** Channel of a single line; a voicing uses one channel per voice (VOICE_SOPRANO ... VOICE_BASS). */
#define OUTPUT_MELODY_CHANNEL 0

typedef enum {
    OUTPUT_OK,
    OUTPUT_INVALID_ARGUMENT,
    OUTPUT_NO_MEMORY,
    /* The destination already exists; writers never overwrite. */
    OUTPUT_FILE_EXISTS,
    OUTPUT_WRITE_ERROR
} OutputStatus;

/** A note that sounds from "start" for "duration" ticks. The note keeps its spelling for LilyPond. */
typedef struct {
    MusicNote note;
    uint32_t start;
    uint32_t duration;
    int channel;
} OutputEvent;

/** Owns its events. "end" is the tick where the next appended value starts. */
typedef struct {
    OutputEvent *events;
    size_t count;
    size_t capacity;
    uint32_t end;
    int tempo;
    int instrument;
} OutputScore;

/** An empty score; positive tempo in BPM (MIDI encoding enforces its own range) and General MIDI program (0 to 127). */
OutputStatus outputScoreInit(OutputScore *score, int tempo, int instrument);
/** Releases the events; it can be called more than once. */
void outputScoreDestroy(OutputScore *score);
/** Appends one event; it moves "end" when the event ends after it. On failure the score is unchanged. */
OutputStatus outputScoreAdd(OutputScore *score, MusicNote note, uint32_t start, uint32_t duration, int channel);

/** Ticks of a duration: a whole is 1920, a sixteenth 120. 0 for an unknown duration. */
uint32_t outputDurationTicks(MusicDuration duration);

/*
 * Values of the language, appended from "end" one after the other, so a
 * program can export several values to the same score. On failure the score
 * is left as it was.
 */

/** A note lasting a quarter (MUSIC_DEFAULT_DURATION). */
OutputStatus outputAddNote(OutputScore *score, MusicNote note);
/** The seven notes one after the other, a quarter each, as exportLiteralScale did. */
OutputStatus outputAddScale(OutputScore *score, const MusicScale *scale);
/** All the notes of the chord at once, lasting a quarter. */
OutputStatus outputAddChord(OutputScore *score, const MusicChord *chord);
/** Each chord with its own duration, one after the other. */
OutputStatus outputAddProgression(OutputScore *score, const MusicProgression *progression);
/**
 * Each SATB chord with its duration, voice i in channel i. A voicing keeps
 * only pitches, so its notes are spelled with sharps (musicNoteFromPitch).
 */
OutputStatus outputAddVoicing(OutputScore *score, const MusicVoicing *voicing);

/*
 * Encoders: the file contents in memory. The buffer is the caller's; it is
 * left empty on failure.
 */

typedef struct {
    unsigned char *data;
    size_t length;
    size_t capacity;
} OutputBuffer;

void outputBufferDestroy(OutputBuffer *buffer);
/**
 * Standard MIDI file, format 0: one track with the tempo, a program change
 * for each channel in use and the events. A voicing keeps each voice in its
 * own channel (soprano 0 ... bass 3). Velocity 64. OUTPUT_INVALID_ARGUMENT
 * when the tempo does not fit MIDI's 24-bit field (outside 4 to 60000000 BPM).
 */
OutputStatus outputEncodeMidi(const OutputScore *score, OutputBuffer *buffer);
/**
 * LilyPond score: one staff per channel in use (Soprano, Alto, Tenor and Bass
 * for a voicing), notes with their spelling, simultaneous notes as chords and
 * silences as rests. Durations must be multiples of a sixteenth (120 ticks);
 * longer values are tied. OUTPUT_INVALID_ARGUMENT when two events of a
 * channel overlap without starting and ending together.
 */
OutputStatus outputEncodeLilyPond(const OutputScore *score, OutputBuffer *buffer);

/*
 * Writers. Each file is written to a temporary file next to its destination
 * and published only when every file of the call was written, so a failure
 * never leaves a partial file nor some of the files. They never overwrite: if
 * a destination exists (or is repeated in the call) nothing is written and
 * they return OUTPUT_FILE_EXISTS. File systems without hard links return
 * OUTPUT_WRITE_ERROR rather than risk replacing a destination.
 */

typedef enum { OUTPUT_FORMAT_MIDI, OUTPUT_FORMAT_LILYPOND } OutputFormat;

typedef struct {
    const OutputScore *score;
    OutputFormat format;
    const char *path;
} OutputFile;

/**
 * Writes all the files or none: if publishing one fails, the ones already
 * published are removed. "failed", when not NULL, receives the index of the
 * file that failed. The runtime calls it once, at the end of a program that
 * ran without errors, with every "export" of the program.
 */
OutputStatus outputWriteFiles(const OutputFile *files, size_t count, size_t *failed);
/** A single MIDI file (outputWriteFiles with one file). */
OutputStatus outputWriteMidi(const OutputScore *score, const char *path);
/** A single LilyPond file. */
OutputStatus outputWriteLilyPond(const OutputScore *score, const char *path);

#endif
