#ifndef MUSIC_VALIDATION_HEADER
#define MUSIC_VALIDATION_HEADER

#include "../../frontend/syntactic-analysis/AbstractSyntaxTree.h"
#include <stdbool.h>
#include <stddef.h>

/* MIDI pitches, ordered soprano, alto, tenor, bass. */
enum { VOICE_SOPRANO, VOICE_ALTO, VOICE_TENOR, VOICE_BASS, VOICE_COUNT };

bool musicInKey(const int *pitches, size_t count, int tonic, Mode mode);
bool musicInTessitura(const int voices[VOICE_COUNT]);
bool musicWithoutVoiceCrossing(const int voices[VOICE_COUNT]);
bool musicWithoutParallelFifths(const int first[VOICE_COUNT], const int second[VOICE_COUNT]);
bool musicWithoutParallelOctaves(const int first[VOICE_COUNT], const int second[VOICE_COUNT]);

#endif
