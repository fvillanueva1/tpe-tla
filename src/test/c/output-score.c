#include "../../main/c/backend/output/Output.h"
#include <assert.h>
#include <stddef.h>

/* realloc fails while "failRealloc" is set. */
void *__real_realloc(void *, size_t);
static bool failRealloc = false;
void *__wrap_realloc(void *pointer, size_t size) { return failRealloc ? NULL : __real_realloc(pointer, size); }

static MusicNote note(const char *lexeme) {
    MusicNote result;
    assert(musicParseNote(lexeme, &result) == MUSIC_OK);
    return result;
}

static void event(const OutputScore *score, size_t i, int pitch, uint32_t start, uint32_t duration, int channel) {
    assert(i < score->count);
    const OutputEvent *e = &score->events[i];
    assert(e->note.pitch == pitch && e->start == start && e->duration == duration && e->channel == channel);
}

static void basics(void) {
    OutputScore score;
    assert(outputScoreInit(&score, OUTPUT_DEFAULT_TEMPO, OUTPUT_DEFAULT_INSTRUMENT) == OUTPUT_OK);
    assert(!score.count && !score.end && score.tempo == 120 && score.instrument == 0);
    assert(outputScoreInit(&score, 0, 0) == OUTPUT_INVALID_ARGUMENT);
    assert(outputScoreInit(&score, 90, 128) == OUTPUT_INVALID_ARGUMENT && outputScoreInit(&score, 90, -1) == OUTPUT_INVALID_ARGUMENT);
    assert(outputScoreInit(NULL, 90, 0) == OUTPUT_INVALID_ARGUMENT);

    assert(outputDurationTicks(MUSIC_DURATION_WHOLE) == 1920 && outputDurationTicks(MUSIC_DURATION_QUARTER) == 480);
    assert(outputDurationTicks(MUSIC_DURATION_SIXTEENTH) == 120 && outputDurationTicks((MusicDuration)9) == 0);
    assert(outputDurationTicks(MUSIC_DEFAULT_DURATION) == OUTPUT_TICKS_PER_QUARTER);

    assert(outputScoreInit(&score, 90, 52) == OUTPUT_OK);
    assert(outputScoreAdd(&score, note("C4"), 960, 480, 2) == OUTPUT_OK && score.end == 1440);
    assert(outputScoreAdd(&score, note("D4"), 0, 480, 0) == OUTPUT_OK && score.end == 1440);
    assert(outputScoreAdd(&score, note("D4"), 0, 0, 0) == OUTPUT_INVALID_ARGUMENT);
    assert(outputScoreAdd(&score, note("D4"), 0, 480, 16) == OUTPUT_INVALID_ARGUMENT);
    assert(outputScoreAdd(&score, note("D4"), UINT32_MAX, 1, 0) == OUTPUT_INVALID_ARGUMENT);
    assert(outputScoreAdd(&score, (MusicNote){5, {'F', 0}}, 0, 1, 0) == OUTPUT_INVALID_ARGUMENT);
    assert(score.count == 2);
    event(&score, 0, 60, 960, 480, 2);

    /* Growing past the first block keeps every event. */
    for (int i = 0; i < 200; ++i) assert(outputScoreAdd(&score, note("E4"), (uint32_t)i, 1, 1) == OUTPUT_OK);
    assert(score.count == 202 && score.events[201].start == 199);
    outputScoreDestroy(&score);
    assert(!score.events && !score.count && !score.end);
    outputScoreDestroy(&score);
    outputScoreDestroy(NULL);
}

static void values(void) {
    OutputScore score;
    assert(outputScoreInit(&score, 120, 0) == OUTPUT_OK);
    /* A note, then a scale and a chord after it. */
    assert(outputAddNote(&score, note("A4")) == OUTPUT_OK);
    event(&score, 0, 69, 0, 480, OUTPUT_MELODY_CHANNEL);
    MusicKey g = {{'G', 0}, MUSIC_MODE_MAJOR, false};
    MusicScale scale;
    assert(musicKeyScale(g, &scale) == MUSIC_OK && outputAddScale(&score, &scale) == OUTPUT_OK);
    event(&score, 1, 67, 480, 480, 0);
    event(&score, 7, 78, 480 * 7, 480, 0);
    assert(score.end == 480 * 8);

    MusicChord chord;
    assert(musicDegreeChord(g, 4, MUSIC_TRIAD, &chord) == MUSIC_OK && outputAddChord(&score, &chord) == OUTPUT_OK);
    event(&score, 8, 74, 3840, 480, 0);
    event(&score, 10, 81, 3840, 480, 0);
    assert(score.end == 4320 && score.count == 11);
    outputScoreDestroy(&score);

    /* A progression keeps each chord's duration. */
    MusicKey c = {{'C', 0}, MUSIC_MODE_MAJOR, false};
    MusicProgression progression;
    assert(musicProgressionInit(&progression, 2));
    assert(musicDegreeChord(c, 0, MUSIC_TRIAD, &progression.steps[0].chord) == MUSIC_OK);
    assert(musicDegreeChord(c, 4, MUSIC_SEVENTH, &progression.steps[1].chord) == MUSIC_OK);
    progression.steps[0].duration = MUSIC_DURATION_HALF;
    progression.steps[1].duration = MUSIC_DURATION_EIGHTH;
    assert(outputScoreInit(&score, 120, 0) == OUTPUT_OK && outputAddProgression(&score, &progression) == OUTPUT_OK);
    assert(score.count == 7 && score.end == 960 + 240);
    event(&score, 2, 67, 0, 960, 0);
    event(&score, 3, 67, 960, 240, 0);
    event(&score, 6, 77, 960, 240, 0);

    /* A bad duration in the middle leaves the score as it was. */
    progression.steps[1].duration = (MusicDuration)9;
    assert(outputAddProgression(&score, &progression) == OUTPUT_INVALID_ARGUMENT && score.count == 7 && score.end == 1200);
    MusicProgression empty = {NULL, 0}, broken = {NULL, 1};
    assert(outputAddProgression(&score, &empty) == OUTPUT_OK && score.count == 7);
    assert(outputAddProgression(&score, &broken) == OUTPUT_INVALID_ARGUMENT);
    musicProgressionDestroy(&progression);
    outputScoreDestroy(&score);

    /* A voicing puts each voice in its channel. */
    MusicVoicing voicing;
    assert(musicVoicingInit(&voicing, 2));
    const int chords[2][VOICE_COUNT] = {{72, 67, 64, 48}, {71, 67, 62, 55}};
    for (int i = 0; i < 2; ++i) for (int v = 0; v < VOICE_COUNT; ++v) voicing.chords[i][v] = chords[i][v];
    voicing.durations[0] = MUSIC_DURATION_QUARTER;
    voicing.durations[1] = MUSIC_DURATION_WHOLE;
    assert(outputScoreInit(&score, 60, 52) == OUTPUT_OK && outputAddVoicing(&score, &voicing) == OUTPUT_OK);
    assert(score.count == 8 && score.end == 480 + 1920);
    event(&score, 0, 72, 0, 480, VOICE_SOPRANO);
    event(&score, 3, 48, 0, 480, VOICE_BASS);
    event(&score, 5, 67, 480, 1920, VOICE_ALTO);
    assert(score.events[6].note.spelling.letter == 'D');
    voicing.chords[1][VOICE_TENOR] = 3;
    assert(outputAddVoicing(&score, &voicing) == OUTPUT_INVALID_ARGUMENT && score.count == 8 && score.end == 2400);
    musicVoicingDestroy(&voicing);

    MusicChord none = {.count = 0};
    assert(outputAddChord(&score, &none) == OUTPUT_INVALID_ARGUMENT && outputAddScale(&score, NULL) == OUTPUT_INVALID_ARGUMENT);
    assert(outputAddNote(NULL, note("C4")) == OUTPUT_INVALID_ARGUMENT);
    outputScoreDestroy(&score);
}

static void memory(void) {
    OutputScore score;
    assert(outputScoreInit(&score, 120, 0) == OUTPUT_OK);
    failRealloc = true;
    assert(outputAddNote(&score, note("C4")) == OUTPUT_NO_MEMORY && !score.count && !score.end);
    failRealloc = false;
    for (int i = 0; i < 64; ++i) assert(outputAddNote(&score, note("C4")) == OUTPUT_OK);
    /* The 65th event needs to grow the array: the chord fails as a whole. */
    MusicChord chord;
    MusicKey c = {{'C', 0}, MUSIC_MODE_MAJOR, false};
    assert(musicDegreeChord(c, 0, MUSIC_TRIAD, &chord) == MUSIC_OK);
    failRealloc = true;
    assert(outputAddChord(&score, &chord) == OUTPUT_NO_MEMORY && score.count == 64 && score.end == 64 * 480);
    failRealloc = false;
    outputScoreDestroy(&score);
}

int main(void) {
    basics();
    values();
    memory();
    return 0;
}
