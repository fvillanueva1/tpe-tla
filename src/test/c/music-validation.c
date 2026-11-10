#include "../../main/c/backend/semantic-analysis/MusicValidation.h"
#include <assert.h>
int main(void) {
    int scale[7];
    const int expectedC[] = {60, 62, 64, 65, 67, 69, 71};
    const int expectedB[] = {71, 73, 75, 76, 78, 80, 82};
    assert(musicScale(60, MODE_MAJOR, scale));
    for (int i = 0; i < 7; ++i) assert(scale[i] == expectedC[i]);
    assert(musicScale(71, MODE_MAJOR, scale));
    for (int i = 0; i < 7; ++i) assert(scale[i] == expectedB[i]);
    const int offsets[][7] = {
        [MODE_DORIAN] = {0, 2, 3, 5, 7, 9, 10}, [MODE_LOCRIAN] = {0, 1, 3, 5, 6, 8, 10},
        [MODE_LYDIAN] = {0, 2, 4, 6, 7, 9, 11}, [MODE_MAJOR] = {0, 2, 4, 5, 7, 9, 11},
        [MODE_MINOR] = {0, 2, 3, 5, 7, 8, 10}, [MODE_MIXOLYDIAN] = {0, 2, 4, 5, 7, 9, 10},
        [MODE_PHRYGIAN] = {0, 1, 3, 5, 7, 8, 10}
    };
    for (int mode = MODE_DORIAN; mode <= MODE_PHRYGIAN; ++mode) {
        assert(musicScale(60, (Mode)mode, scale));
        for (int i = 0; i < 7; ++i) assert(scale[i] == 60 + offsets[mode][i]);
    }
    scale[0] = -42;
    assert(!musicScale(119, MODE_MAJOR, scale));
    assert(scale[0] == -42);
    assert(!musicScale(60, MODE_MAJOR, NULL));
    assert(!musicScale(60, (Mode)99, scale));
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
    const unsigned short progression[] = {1u << 0, 1u << 2, 1u << 0};
    int realized[3][VOICE_COUNT];
    assert(musicRealizeSATB(progression, 3, realized) == MUSIC_VOICE_SUCCESS);
    int movement = 0;
    for (int i = 0; i < 3; ++i) {
        assert(musicInTessitura(realized[i]));
        assert(musicWithoutVoiceCrossing(realized[i]));
        for (int v = 0; v < VOICE_COUNT; ++v) {
            assert(progression[i] & (1u << (realized[i][v] % 12)));
            if (i) { int delta = realized[i][v] - realized[i - 1][v]; movement += delta < 0 ? -delta : delta; }
        }
    }
    assert(movement == 16);
    const unsigned short triads[] = {(1u << 0) | (1u << 4) | (1u << 7),
                                     (1u << 2) | (1u << 6) | (1u << 9),
                                     (1u << 7) | (1u << 11) | (1u << 2)};
    int triadVoices[3][VOICE_COUNT], triadMovement = 0;
    assert(musicRealizeSATB(triads, 3, triadVoices) == MUSIC_VOICE_SUCCESS);
    for (int i = 0; i < 3; ++i) {
        unsigned short seen = 0;
        assert(musicInTessitura(triadVoices[i]) && musicWithoutVoiceCrossing(triadVoices[i]));
        for (int v = 0; v < VOICE_COUNT; ++v) {
            seen |= 1u << (triadVoices[i][v] % 12);
            if (i) { int delta = triadVoices[i][v] - triadVoices[i - 1][v]; triadMovement += delta < 0 ? -delta : delta; }
        }
        assert(seen == triads[i]);
    }
    assert(triadMovement == 10);
    const unsigned short tooMany[] = {(1u << 0) | (1u << 2) | (1u << 4) | (1u << 7) | (1u << 10)};
    assert(musicRealizeSATB(tooMany, 1, realized) == MUSIC_VOICE_INVALID_CHORD);
    const unsigned short invalid[] = {0};
    assert(musicRealizeSATB(invalid, 1, realized) == MUSIC_VOICE_INVALID_CHORD);
    assert(musicRealizeSATB(NULL, 1, realized) == MUSIC_VOICE_INVALID_CHORD);
    assert(musicRealizeSATB(NULL, 0, NULL) == MUSIC_VOICE_SUCCESS);
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
