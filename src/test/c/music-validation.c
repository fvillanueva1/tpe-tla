#include "../../main/c/backend/semantic-analysis/MusicValidation.h"
#include <assert.h>
int main(void) {
    int c_major[] = {60, 64, 67};
    int outside[] = {60, 66, 67};
    assert(musicInKey(c_major, 3, 0, MODE_MAJOR));
    assert(!musicInKey(outside, 3, 0, MODE_MAJOR));
    assert(musicInKey(outside, 3, 0, MODE_LYDIAN));
    int g_major[] = {67, 71, 66};
    assert(musicInKey(g_major, 3, 7, MODE_MAJOR));
    assert(!musicInKey(g_major, 3, 0, MODE_MAJOR));
    assert(!musicInKey(NULL, 1, 0, MODE_MAJOR));
    assert(!musicInKey(outside, 3, 0, (Mode)99));
    int first[] = {72, 67, 60, 48};
    int second[] = {74, 69, 62, 50};
    int contrary[] = {71, 69, 60, 48};
    int crossed[] = {60, 67, 60, 48};
    int out_of_range[] = {59, 67, 60, 48};
    assert(musicInTessitura(first));
    assert(!musicInTessitura(out_of_range));
    assert(musicWithoutVoiceCrossing(first));
    assert(!musicWithoutVoiceCrossing(crossed));
    assert(!musicWithoutParallelFifths(first, second));
    assert(!musicWithoutParallelOctaves(first, second));
    assert(musicWithoutParallelFifths(first, contrary));
    assert(musicWithoutParallelOctaves(first, contrary));
    assert(musicWithoutParallelFifths(first, first));
    assert(musicWithoutParallelOctaves(first, first));
    int unison_first[] = {72, 72, 60, 48};
    int unison_second[] = {74, 74, 60, 48};
    assert(musicWithoutVoiceCrossing(unison_first));
    assert(musicWithoutParallelOctaves(unison_first, unison_second));
    assert(!musicInTessitura(NULL));
    assert(!musicWithoutParallelFifths(NULL, second));
    size_t failed = 99;
    const int sequence[][VOICE_COUNT] = {{72, 67, 60, 48}, {74, 69, 62, 50}};
    assert(musicCheckVoicing(sequence, 2, CHECK_PARALLEL_FIFTHS, &failed) == MUSIC_PARALLEL_FIFTHS_ERROR && failed == 1);
    assert(musicCheckVoicing(sequence, 2, CHECK_PARALLEL_OCTAVES, &failed) == MUSIC_PARALLEL_OCTAVES_ERROR && failed == 1);
    assert(musicCheckVoicing(sequence, 2, CHECK_VOICE_CROSSING, NULL) == MUSIC_VALID);
    const int crossing[][VOICE_COUNT] = {{60, 67, 60, 48}};
    assert(musicCheckVoicing(crossing, 1, CHECK_VOICE_CROSSING, &failed) == MUSIC_VOICE_CROSSING_ERROR && failed == 0);
    const int range[][VOICE_COUNT] = {{59, 67, 60, 48}};
    assert(musicCheckVoicing(range, 1, 0, &failed) == MUSIC_TESSITURA_ERROR && failed == 0);
    assert(musicCheckVoicing(NULL, 1, 0, NULL) == MUSIC_INVALID_INPUT);
    assert(musicCheckVoicing(NULL, 0, 0, NULL) == MUSIC_VALID);
    assert(musicCheckVoicing(sequence, 2, 8, NULL) == MUSIC_INVALID_INPUT);
    return 0;
}
