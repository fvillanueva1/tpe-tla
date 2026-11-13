#include "Output.h"
#include <stdlib.h>

OutputStatus outputScoreInit(OutputScore *score, int tempo, int instrument) {
    if (!score || tempo < 1 || instrument < 0 || instrument > 127) return OUTPUT_INVALID_ARGUMENT;
    *score = (OutputScore){.tempo = tempo, .instrument = instrument};
    return OUTPUT_OK;
}

void outputScoreDestroy(OutputScore *score) {
    if (!score) return;
    free(score->events);
    score->events = NULL;
    score->count = score->capacity = 0;
    score->end = 0;
}

OutputStatus outputScoreAdd(OutputScore *score, MusicNote note, uint32_t start, uint32_t duration, int channel) {
    if (!score || !duration || channel < 0 || channel > 15 || start > UINT32_MAX - duration ||
        note.pitch < MUSIC_LOWEST_PITCH || note.pitch > MUSIC_HIGHEST_PITCH) return OUTPUT_INVALID_ARGUMENT;
    if (score->count == score->capacity) {
        size_t capacity = score->capacity ? score->capacity * 2 : 64;
        if (capacity < score->capacity || capacity > SIZE_MAX / sizeof(OutputEvent)) return OUTPUT_NO_MEMORY;
        OutputEvent *events = realloc(score->events, capacity * sizeof(OutputEvent));
        if (!events) return OUTPUT_NO_MEMORY;
        score->events = events;
        score->capacity = capacity;
    }
    score->events[score->count++] = (OutputEvent){note, start, duration, channel};
    if (start + duration > score->end) score->end = start + duration;
    return OUTPUT_OK;
}

uint32_t outputDurationTicks(MusicDuration duration) {
    if (duration < MUSIC_DURATION_WHOLE || duration > MUSIC_DURATION_SIXTEENTH) return 0;
    return 4 * OUTPUT_TICKS_PER_QUARTER >> duration;
}

/* Undoes what a failed value appended, so the score stays as it was. */
static OutputStatus rollback(OutputScore *score, size_t count, uint32_t end, OutputStatus status) {
    score->count = count;
    score->end = end;
    return status;
}

/* Appends the notes as one block that starts at "end" and lasts "duration". */
static OutputStatus block(OutputScore *score, const MusicNote *notes, size_t count, MusicDuration duration) {
    uint32_t ticks = outputDurationTicks(duration), start = score->end;
    if (!ticks) return OUTPUT_INVALID_ARGUMENT;
    for (size_t i = 0; i < count; ++i) {
        OutputStatus status = outputScoreAdd(score, notes[i], start, ticks, OUTPUT_MELODY_CHANNEL);
        if (status != OUTPUT_OK) return status;
    }
    if (!count && start > UINT32_MAX - ticks) return OUTPUT_INVALID_ARGUMENT;
    score->end = start + ticks;
    return OUTPUT_OK;
}

OutputStatus outputAddNote(OutputScore *score, MusicNote note) {
    if (!score) return OUTPUT_INVALID_ARGUMENT;
    size_t count = score->count;
    uint32_t end = score->end;
    OutputStatus status = block(score, &note, 1, MUSIC_DEFAULT_DURATION);
    return status == OUTPUT_OK ? OUTPUT_OK : rollback(score, count, end, status);
}

OutputStatus outputAddScale(OutputScore *score, const MusicScale *scale) {
    if (!score || !scale) return OUTPUT_INVALID_ARGUMENT;
    size_t count = score->count;
    uint32_t end = score->end;
    for (int i = 0; i < 7; ++i) {
        OutputStatus status = block(score, &scale->notes[i], 1, MUSIC_DEFAULT_DURATION);
        if (status != OUTPUT_OK) return rollback(score, count, end, status);
    }
    return OUTPUT_OK;
}

OutputStatus outputAddChord(OutputScore *score, const MusicChord *chord) {
    if (!score || !chord || !chord->count || chord->count > MUSIC_MAX_CHORD_NOTES) return OUTPUT_INVALID_ARGUMENT;
    size_t count = score->count;
    uint32_t end = score->end;
    OutputStatus status = block(score, chord->notes, chord->count, MUSIC_DEFAULT_DURATION);
    return status == OUTPUT_OK ? OUTPUT_OK : rollback(score, count, end, status);
}

OutputStatus outputAddProgression(OutputScore *score, const MusicProgression *progression) {
    if (!score || !progression || (!progression->steps && progression->count)) return OUTPUT_INVALID_ARGUMENT;
    size_t count = score->count;
    uint32_t end = score->end;
    for (size_t i = 0; i < progression->count; ++i) {
        const MusicChord *chord = &progression->steps[i].chord;
        OutputStatus status = chord->count <= MUSIC_MAX_CHORD_NOTES ?
            block(score, chord->notes, chord->count, progression->steps[i].duration) : OUTPUT_INVALID_ARGUMENT;
        if (status != OUTPUT_OK) return rollback(score, count, end, status);
    }
    return OUTPUT_OK;
}

OutputStatus outputAddVoicing(OutputScore *score, const MusicVoicing *voicing) {
    if (!score || !voicing || ((!voicing->chords || !voicing->durations) && voicing->count)) return OUTPUT_INVALID_ARGUMENT;
    size_t count = score->count;
    uint32_t end = score->end;
    for (size_t i = 0; i < voicing->count; ++i) {
        uint32_t ticks = outputDurationTicks(voicing->durations[i]), start = score->end;
        OutputStatus status = ticks ? OUTPUT_OK : OUTPUT_INVALID_ARGUMENT;
        for (int voice = 0; voice < VOICE_COUNT && status == OUTPUT_OK; ++voice) {
            MusicNote note;
            status = musicNoteFromPitch(voicing->chords[i][voice], &note) == MUSIC_OK ?
                outputScoreAdd(score, note, start, ticks, voice) : OUTPUT_INVALID_ARGUMENT;
        }
        if (status != OUTPUT_OK) return rollback(score, count, end, status);
    }
    return OUTPUT_OK;
}
