#ifndef MUSIC_VALIDATION_HEADER
#define MUSIC_VALIDATION_HEADER

#include "../../frontend/syntactic-analysis/AbstractSyntaxTree.h"
#include <stdbool.h>
#include <stddef.h>

/* MIDI pitches, ordered soprano, alto, tenor, bass. */
enum { VOICE_SOPRANO, VOICE_ALTO, VOICE_TENOR, VOICE_BASS, VOICE_COUNT };

/* Compute seven ascending MIDI pitches starting at the given tonic. */
bool musicScale(int tonic, Mode mode, int notes[7]);
bool musicInKey(const int *pitches, size_t count, int tonic, Mode mode);

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

/* failedChord, when non-NULL, receives the zero-based index of the failing chord. */
MusicValidationResult musicCheckVoicing(const int (*chords)[VOICE_COUNT], size_t count,
                                        unsigned int rules, size_t *failedChord);

#endif
