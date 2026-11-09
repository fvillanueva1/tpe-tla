#include "Music.h"
#include <stdint.h>
#include <string.h>

static bool validChord(const MusicChord *chord) {
    return chord && chord->count && chord->count <= MUSIC_MAX_CHORD_NOTES && chord->root < chord->count;
}

static bool validProgression(const MusicProgression *progression) {
    return progression && (progression->steps || !progression->count);
}

/* Reserves "count" steps; "result" is empty when it fails. */
static MusicStatus reserve(MusicProgression *result, size_t count) {
    if (count > SIZE_MAX / sizeof(MusicStep)) { result->steps = NULL; result->count = 0; return MUSIC_NO_MEMORY; }
    return musicProgressionInit(result, count) ? MUSIC_OK : MUSIC_NO_MEMORY;
}

static MusicStatus fail(MusicProgression *result, MusicStatus status) {
    musicProgressionDestroy(result);
    return status;
}

MusicStatus musicDegreeProgression(MusicKey key, const int *degrees, size_t count, MusicProgression *result) {
    if (!result) return MUSIC_INVALID_ARGUMENT;
    result->steps = NULL; result->count = 0;
    if (!degrees && count) return MUSIC_INVALID_ARGUMENT;
    MusicStatus status = reserve(result, count);
    if (status != MUSIC_OK) return status;
    for (size_t i = 0; i < count; ++i) {
        status = musicDegreeChord(key, degrees[i], MUSIC_TRIAD, &result->steps[i].chord);
        if (status != MUSIC_OK) return fail(result, status);
        result->steps[i].duration = MUSIC_DEFAULT_DURATION;
    }
    return MUSIC_OK;
}

MusicStatus musicChordProgression(const MusicChord *chords, size_t count, MusicProgression *result) {
    if (!result) return MUSIC_INVALID_ARGUMENT;
    result->steps = NULL; result->count = 0;
    if (!chords && count) return MUSIC_INVALID_ARGUMENT;
    for (size_t i = 0; i < count; ++i) if (!validChord(&chords[i])) return MUSIC_INVALID_ARGUMENT;
    MusicStatus status = reserve(result, count);
    if (status != MUSIC_OK) return status;
    for (size_t i = 0; i < count; ++i) result->steps[i] = (MusicStep){chords[i], MUSIC_DEFAULT_DURATION};
    return MUSIC_OK;
}

MusicStatus musicConcatenate(const MusicProgression *first, const MusicProgression *second, MusicProgression *result) {
    if (!result) return MUSIC_INVALID_ARGUMENT;
    result->steps = NULL; result->count = 0;
    if (!validProgression(first) || !validProgression(second)) return MUSIC_INVALID_ARGUMENT;
    if (first->count > SIZE_MAX - second->count) return MUSIC_NO_MEMORY;
    MusicStatus status = reserve(result, first->count + second->count);
    if (status != MUSIC_OK) return status;
    if (first->count) memcpy(result->steps, first->steps, first->count * sizeof(MusicStep));
    if (second->count) memcpy(result->steps + first->count, second->steps, second->count * sizeof(MusicStep));
    return MUSIC_OK;
}

MusicStatus musicAppendChord(const MusicProgression *progression, const MusicChord *chord, MusicProgression *result) {
    if (!result) return MUSIC_INVALID_ARGUMENT;
    result->steps = NULL; result->count = 0;
    if (!validChord(chord)) return MUSIC_INVALID_ARGUMENT;
    MusicStep step = {*chord, MUSIC_DEFAULT_DURATION};
    MusicProgression single = {&step, 1};
    return musicConcatenate(progression, &single, result);
}

MusicStatus musicRepeat(const MusicProgression *progression, int times, MusicProgression *result) {
    if (!result) return MUSIC_INVALID_ARGUMENT;
    result->steps = NULL; result->count = 0;
    if (!validProgression(progression)) return MUSIC_INVALID_ARGUMENT;
    if (times < 0) return MUSIC_OUT_OF_RANGE;
    size_t count = progression->count;
    if (count && (size_t)times > SIZE_MAX / count) return MUSIC_NO_MEMORY;
    MusicStatus status = reserve(result, count * (size_t)times);
    if (status != MUSIC_OK) return status;
    for (int i = 0; i < times && count; ++i)
        memcpy(result->steps + (size_t)i * count, progression->steps, count * sizeof(MusicStep));
    return MUSIC_OK;
}
