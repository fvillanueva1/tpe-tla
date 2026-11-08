#include "Music.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

bool musicProgressionInit(MusicProgression *progression, size_t count) {
    if (!progression) return false;
    progression->steps = NULL;
    progression->count = 0;
    if (!count) return true;
    MusicStep *steps = calloc(count, sizeof(*steps));
    if (!steps) return false;
    progression->steps = steps;
    progression->count = count;
    return true;
}

bool musicProgressionCopy(const MusicProgression *source, MusicProgression *target) {
    if (!target) return false;
    if (!source || (!source->steps && source->count) || !musicProgressionInit(target, source->count)) {
        target->steps = NULL; target->count = 0; return false;
    }
    if (source->count) memcpy(target->steps, source->steps, source->count * sizeof(*source->steps));
    return true;
}

void musicProgressionDestroy(MusicProgression *progression) {
    if (!progression) return;
    free(progression->steps);
    progression->steps = NULL;
    progression->count = 0;
}

bool musicVoicingInit(MusicVoicing *voicing, size_t count) {
    if (!voicing) return false;
    voicing->chords = NULL;
    voicing->durations = NULL;
    voicing->count = 0;
    if (!count) return true;
    int (*chords)[VOICE_COUNT] = calloc(count, sizeof(*chords));
    MusicDuration *durations = calloc(count, sizeof(*durations));
    if (!chords || !durations) { free(chords); free(durations); return false; }
    voicing->chords = chords;
    voicing->durations = durations;
    voicing->count = count;
    return true;
}

bool musicVoicingCopy(const MusicVoicing *source, MusicVoicing *target) {
    if (!target) return false;
    if (!source || ((!source->chords || !source->durations) && source->count) ||
        !musicVoicingInit(target, source->count)) {
        target->chords = NULL; target->durations = NULL; target->count = 0; return false;
    }
    if (source->count) {
        memcpy(target->chords, source->chords, source->count * sizeof(*source->chords));
        memcpy(target->durations, source->durations, source->count * sizeof(*source->durations));
    }
    return true;
}

void musicVoicingDestroy(MusicVoicing *voicing) {
    if (!voicing) return;
    free(voicing->chords);
    free(voicing->durations);
    voicing->chords = NULL;
    voicing->durations = NULL;
    voicing->count = 0;
}
