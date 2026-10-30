#include "MusicValidation.h"

bool musicInKey(const int *pitches, size_t count, int tonic, Mode mode) {
    static const int offsets[][7] = {
        [MODE_DORIAN] = {0, 2, 3, 5, 7, 9, 10},
        [MODE_LOCRIAN] = {0, 1, 3, 5, 6, 8, 10},
        [MODE_LYDIAN] = {0, 2, 4, 6, 7, 9, 11},
        [MODE_MAJOR] = {0, 2, 4, 5, 7, 9, 11},
        [MODE_MINOR] = {0, 2, 3, 5, 7, 8, 10},
        [MODE_MIXOLYDIAN] = {0, 2, 4, 5, 7, 9, 10},
        [MODE_PHRYGIAN] = {0, 1, 3, 5, 7, 8, 10}
    };
    if ((!pitches && count) || tonic < 0 || tonic > 11 || mode < MODE_DORIAN || mode > MODE_PHRYGIAN) return false;
    for (size_t i = 0; i < count; ++i) {
        if (pitches[i] < 12 || pitches[i] > 119) return false;
        int pitchClass = (pitches[i] - tonic) % 12;
        bool found = false;
        for (size_t j = 0; j < 7; ++j) if (offsets[mode][j] == pitchClass) { found = true; break; }
        if (!found) return false;
    }
    return true;
}

bool musicInTessitura(const int voices[VOICE_COUNT]) {
    static const int low[VOICE_COUNT] = {60, 53, 48, 40};
    static const int high[VOICE_COUNT] = {81, 74, 69, 64};
    if (!voices) return false;
    for (int i = 0; i < VOICE_COUNT; ++i)
        if (voices[i] < low[i] || voices[i] > high[i]) return false;
    return true;
}

bool musicWithoutVoiceCrossing(const int voices[VOICE_COUNT]) {
    if (!voices) return false;
    for (int i = 0; i + 1 < VOICE_COUNT; ++i)
        if (voices[i] < voices[i + 1]) return false;
    return true;
}

static bool withoutParallels(const int first[VOICE_COUNT], const int second[VOICE_COUNT], int interval) {
    if (!first || !second) return false;
    for (int i = 0; i < VOICE_COUNT; ++i) {
        if (first[i] < 12 || first[i] > 119 || second[i] < 12 || second[i] > 119) return false;
        for (int j = 0; j < i; ++j) {
            int a = first[i] - first[j], b = second[i] - second[j];
            if (a < 0) a = -a;
            if (b < 0) b = -b;
            int di = second[i] - first[i], dj = second[j] - first[j];
            if (a % 12 == interval && b % 12 == interval &&
                (interval != 0 || (a >= 12 && b >= 12)) &&
                ((di > 0 && dj > 0) || (di < 0 && dj < 0))) return false;
        }
    }
    return true;
}

bool musicWithoutParallelFifths(const int first[VOICE_COUNT], const int second[VOICE_COUNT]) {
    return withoutParallels(first, second, 7);
}
bool musicWithoutParallelOctaves(const int first[VOICE_COUNT], const int second[VOICE_COUNT]) {
    return withoutParallels(first, second, 0);
}

MusicValidationResult musicCheckVoicing(const int (*chords)[VOICE_COUNT], size_t count,
                                        unsigned int rules, size_t *failedChord) {
    const unsigned int supported = CHECK_PARALLEL_FIFTHS | CHECK_PARALLEL_OCTAVES | CHECK_VOICE_CROSSING;
    if ((!chords && count) || (rules & ~supported)) return MUSIC_INVALID_INPUT;
    for (size_t i = 0; i < count; ++i) {
        MusicValidationResult result = MUSIC_VALID;
        if (!musicInTessitura(chords[i])) result = MUSIC_TESSITURA_ERROR;
        else if ((rules & CHECK_VOICE_CROSSING) && !musicWithoutVoiceCrossing(chords[i])) result = MUSIC_VOICE_CROSSING_ERROR;
        else if (i && (rules & CHECK_PARALLEL_FIFTHS) && !musicWithoutParallelFifths(chords[i - 1], chords[i])) result = MUSIC_PARALLEL_FIFTHS_ERROR;
        else if (i && (rules & CHECK_PARALLEL_OCTAVES) && !musicWithoutParallelOctaves(chords[i - 1], chords[i])) result = MUSIC_PARALLEL_OCTAVES_ERROR;
        if (result != MUSIC_VALID) { if (failedChord) *failedChord = i; return result; }
    }
    return MUSIC_VALID;
}
