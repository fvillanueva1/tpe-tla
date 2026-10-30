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
    return 0;
}
