#include "../../main/c/backend/music/Music.h"
#include <assert.h>
#include <stddef.h>

/* Allocation number "failAt" (1-based) fails; 0 never fails. */
void *__real_calloc(size_t, size_t);
static int failAt = 0, calls = 0;
void *__wrap_calloc(size_t n, size_t size) { return ++calls == failAt ? NULL : __real_calloc(n, size); }
static void failOn(int call) { failAt = call; calls = 0; }

static void progressions(void) {
    MusicProgression empty, copy;
    assert(musicProgressionInit(&empty, 0) && !empty.steps && !empty.count);
    assert(musicProgressionCopy(&empty, &copy) && !copy.steps && !copy.count);

    MusicProgression progression;
    assert(musicProgressionInit(&progression, 2) && progression.count == 2);
    assert(progression.steps[1].chord.count == 0);
    MusicChord c = {.notes = {{60, {'C', 0}}, {64, {'E', 0}}, {67, {'G', 0}}}, .count = 3};
    progression.steps[0].chord = c;
    progression.steps[0].duration = MUSIC_DURATION_HALF;
    assert(musicProgressionCopy(&progression, &copy) && copy.count == 2 && copy.steps != progression.steps);
    progression.steps[0].chord.notes[0].pitch = 48;
    assert(copy.steps[0].chord.notes[0].pitch == 60 && copy.steps[0].chord.count == 3);
    assert(copy.steps[0].duration == MUSIC_DURATION_HALF && copy.steps[0].chord.notes[1].spelling.letter == 'E');

    musicProgressionDestroy(&copy);
    assert(!copy.steps && !copy.count);
    musicProgressionDestroy(&copy);

    failOn(1);
    copy.count = 7;
    assert(!musicProgressionCopy(&progression, &copy) && !copy.steps && !copy.count);
    failOn(1);
    assert(!musicProgressionInit(&copy, 3) && !copy.steps && !copy.count);
    failOn(0);
    MusicProgression broken = {NULL, 1};
    assert(!musicProgressionCopy(&broken, &copy) && !copy.steps && !copy.count);
    assert(!musicProgressionCopy(NULL, &copy) && !musicProgressionInit(NULL, 1));
    musicProgressionDestroy(&progression);
    musicProgressionDestroy(NULL);
}

static void voicings(void) {
    MusicVoicing voicing, copy;
    assert(musicVoicingInit(&voicing, 0) && !voicing.chords && !voicing.durations && !voicing.count);
    assert(musicVoicingInit(&voicing, 2) && voicing.count == 2 && voicing.chords[1][VOICE_BASS] == 0);
    const int chord[VOICE_COUNT] = {72, 67, 64, 48};
    for (int v = 0; v < VOICE_COUNT; ++v) voicing.chords[0][v] = chord[v];
    voicing.durations[1] = MUSIC_DURATION_WHOLE;
    assert(musicVoicingCopy(&voicing, &copy) && copy.count == 2 && copy.chords != voicing.chords);
    voicing.chords[0][VOICE_SOPRANO] = 0;
    assert(copy.chords[0][VOICE_SOPRANO] == 72 && copy.chords[0][VOICE_BASS] == 48);
    assert(copy.durations[1] == MUSIC_DURATION_WHOLE);
    musicVoicingDestroy(&copy);
    assert(!copy.chords && !copy.durations && !copy.count);
    musicVoicingDestroy(&copy);

    /* Either of the two arrays can fail; nothing is left allocated (ASan checks it). */
    for (int call = 1; call <= 2; ++call) {
        failOn(call);
        assert(!musicVoicingInit(&copy, 2) && !copy.chords && !copy.durations && !copy.count);
        failOn(call);
        assert(!musicVoicingCopy(&voicing, &copy) && !copy.chords && !copy.count);
    }
    failOn(0);
    MusicVoicing broken = {NULL, NULL, 1};
    assert(!musicVoicingCopy(&broken, &copy) && !copy.count);
    musicVoicingDestroy(&voicing);
}

int main(void) {
    progressions();
    voicings();
    return 0;
}
