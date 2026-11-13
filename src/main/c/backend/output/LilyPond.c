#include "OutputInternal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Durations from the longest; LilyPond names them by their fraction of a whole. */
static const struct { uint32_t ticks; const char *name; } durations[] = {
    {4 * OUTPUT_TICKS_PER_QUARTER, "1"}, {2 * OUTPUT_TICKS_PER_QUARTER, "2"}, {OUTPUT_TICKS_PER_QUARTER, "4"},
    {OUTPUT_TICKS_PER_QUARTER / 2, "8"}, {OUTPUT_TICKS_PER_QUARTER / 4, "16"}
};
#define SIXTEENTH (OUTPUT_TICKS_PER_QUARTER / 4)

/* "c'", "fis''", "bes,": letter, "is"/"es" per accidental and octave marks (c' is C4). */
static void noteName(MusicNote note, char *text) {
    size_t length = 0;
    text[length++] = (char)(note.spelling.letter - 'A' + 'a');
    for (int i = 0; i < note.spelling.accidental; ++i) { text[length++] = 'i'; text[length++] = 's'; }
    for (int i = 0; i > note.spelling.accidental; --i) { text[length++] = 'e'; text[length++] = 's'; }
    int octave = musicNoteOctave(note);
    for (int i = 3; i < octave; ++i) text[length++] = '\'';
    for (int i = octave; i < 3; ++i) text[length++] = ',';
    text[length] = '\0';
}

/* Writes "body" lasting "ticks", splitting it into tied values ("c'2~ c'8") or repeated rests ("r2 r8"). */
static bool appendTimed(OutputBuffer *buffer, const char *body, uint32_t ticks, bool rest) {
    bool first = true;
    for (size_t i = 0; i < sizeof(durations) / sizeof(durations[0]); ++i)
        while (ticks >= durations[i].ticks) {
            if (!first && !outputBufferText(buffer, rest ? " " : "~ ")) return false;
            if (!outputBufferText(buffer, body) || !outputBufferText(buffer, durations[i].name)) return false;
            ticks -= durations[i].ticks;
            first = false;
        }
    return outputBufferText(buffer, " ");
}

static int compareStarts(const void *a, const void *b) {
    const OutputEvent *x = *(const OutputEvent *const *)a, *y = *(const OutputEvent *const *)b;
    if (x->start != y->start) return x->start < y->start ? -1 : 1;
    return x < y ? -1 : x > y;
}

/* The music of one channel: chords of the events that start together, rests between them. */
static OutputStatus appendChannel(OutputBuffer *buffer, const OutputEvent **events, size_t count) {
    qsort(events, count, sizeof(*events), compareStarts);
    uint32_t now = 0;
    for (size_t i = 0; i < count;) {
        const OutputEvent *first = events[i];
        size_t end = i + 1;
        while (end < count && events[end]->start == first->start) ++end;
        for (size_t j = i; j < end; ++j) if (events[j]->duration != first->duration) return OUTPUT_INVALID_ARGUMENT;
        if (first->start < now || first->start % SIXTEENTH || first->duration % SIXTEENTH) return OUTPUT_INVALID_ARGUMENT;
        if (first->start > now && !appendTimed(buffer, "r", first->start - now, true)) return OUTPUT_NO_MEMORY;
        char body[MUSIC_MAX_CHORD_NOTES * 16 + 4] = "";
        if (end - i > 1) strcat(body, "<");
        for (size_t j = i; j < end && j - i < MUSIC_MAX_CHORD_NOTES * 2; ++j) {
            char name[16];
            noteName(events[j]->note, name);
            if (j > i) strcat(body, " ");
            if (strlen(body) + strlen(name) + 2 >= sizeof(body)) return OUTPUT_INVALID_ARGUMENT;
            strcat(body, name);
        }
        if (end - i > 1) strcat(body, ">");
        if (!appendTimed(buffer, body, first->duration, false)) return OUTPUT_NO_MEMORY;
        now = first->start + first->duration;
        i = end;
    }
    return OUTPUT_OK;
}

static const char *clefFor(int channel, bool satb, const OutputEvent **events, size_t count) {
    static const char *satbClefs[] = {"treble", "treble", "treble_8", "bass"};
    if (satb) return satbClefs[channel];
    long sum = 0;
    for (size_t i = 0; i < count; ++i) sum += events[i]->note.pitch;
    return count && sum / (long)count < 60 ? "bass" : "treble";
}

OutputStatus outputEncodeLilyPond(const OutputScore *score, OutputBuffer *buffer) {
    static const char *voiceNames[] = {"Soprano", "Alto", "Tenor", "Bass"};
    if (!buffer) return OUTPUT_INVALID_ARGUMENT;
    *buffer = (OutputBuffer){0};
    if (!outputValidScore(score)) return OUTPUT_INVALID_ARGUMENT;
    size_t used[16] = {0};
    bool satb = false;
    for (size_t i = 0; i < score->count; ++i) {
        ++used[score->events[i].channel];
        if (score->events[i].channel > OUTPUT_MELODY_CHANNEL) satb = true;
    }
    for (int channel = VOICE_COUNT; channel < 16 && satb; ++channel) if (used[channel]) satb = false;
    const OutputEvent **events = malloc((score->count ? score->count : 1) * sizeof(*events));
    if (!events) return OUTPUT_NO_MEMORY;
    char line[96];
    OutputStatus status = outputBufferText(buffer, "\\version \"2.24.0\"\n\\header { tagline = ##f }\n\\score {\n  <<\n")
        ? OUTPUT_OK : OUTPUT_NO_MEMORY;
    bool tempo = false, any = false;
    for (int channel = 0; channel < 16 && status == OUTPUT_OK; ++channel) {
        if (!used[channel] && (channel || score->count)) continue;
        size_t count = 0;
        for (size_t i = 0; i < score->count; ++i) if (score->events[i].channel == channel) events[count++] = &score->events[i];
        if (satb) snprintf(line, sizeof(line), "    \\new Staff \\with { instrumentName = \"%s\" } {\n", voiceNames[channel]);
        else snprintf(line, sizeof(line), "    \\new Staff {\n");
        if (!outputBufferText(buffer, line)) { status = OUTPUT_NO_MEMORY; break; }
        snprintf(line, sizeof(line), "      \\clef \"%s\"\n", clefFor(channel, satb, events, count));
        if (!outputBufferText(buffer, line)) { status = OUTPUT_NO_MEMORY; break; }
        if (!tempo) {
            snprintf(line, sizeof(line), "      \\tempo 4 = %d\n", score->tempo);
            if (!outputBufferText(buffer, line)) { status = OUTPUT_NO_MEMORY; break; }
            tempo = true;
        }
        if (!outputBufferText(buffer, "      ")) { status = OUTPUT_NO_MEMORY; break; }
        status = appendChannel(buffer, events, count);
        if (status == OUTPUT_OK && !outputBufferText(buffer, "\n    }\n")) status = OUTPUT_NO_MEMORY;
        any = true;
    }
    free(events);
    if (status == OUTPUT_OK && (!any || !outputBufferText(buffer, "  >>\n  \\layout { }\n}\n"))) status = OUTPUT_NO_MEMORY;
    if (status != OUTPUT_OK) outputBufferDestroy(buffer);
    return status;
}
