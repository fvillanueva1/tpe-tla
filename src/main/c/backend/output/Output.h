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
    OUTPUT_WRITE_ERROR,
    OUTPUT_NOT_IMPLEMENTED
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

/** An empty score; tempo in BPM (1 to 60000000) and General MIDI program (0 to 127). */
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
 * Writers. They write to a temporary file next to "path" and rename it only
 * when everything was written, so a failure never leaves a partial file, and
 * they never overwrite an existing file (OUTPUT_FILE_EXISTS).
 */

/** Standard MIDI file: tempo, instrument and the events in their channels. */
OutputStatus outputWriteMidi(const OutputScore *score, const char *path);
/** LilyPond (.ly) score, one staff per channel. */
OutputStatus outputWriteLilyPond(const OutputScore *score, const char *path);

#endif
