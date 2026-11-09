#include "OutputInternal.h"
#include <stdlib.h>

/* A note-on or note-off at a tick; "order" keeps the order of the events for equal ticks. */
typedef struct {
    uint32_t tick;
    bool on;
    int channel;
    int pitch;
    size_t order;
} Message;

/* Earlier ticks first; at the same tick the notes that end go before the ones that start. */
static int compareMessages(const void *a, const void *b) {
    const Message *x = a, *y = b;
    if (x->tick != y->tick) return x->tick < y->tick ? -1 : 1;
    if (x->on != y->on) return x->on ? 1 : -1;
    return x->order < y->order ? -1 : x->order > y->order;
}

/* MIDI variable-length quantity: 7 bits per byte, the high bit set on all but the last. */
static bool appendQuantity(OutputBuffer *buffer, uint32_t value) {
    unsigned char bytes[5];
    size_t count = 0;
    do { bytes[count++] = value & 0x7f; value >>= 7; } while (value);
    for (size_t i = count; i-- > 0;) {
        unsigned char byte = (unsigned char)(bytes[i] | (i ? 0x80 : 0));
        if (!outputBufferAppend(buffer, &byte, 1)) return false;
    }
    return true;
}

static bool appendBytes(OutputBuffer *buffer, int a, int b, int c, int d) {
    unsigned char bytes[] = {(unsigned char)a, (unsigned char)b, (unsigned char)c, (unsigned char)d};
    return outputBufferAppend(buffer, bytes, d < 0 ? 3 : 4);
}

static OutputStatus encodeTrack(const OutputScore *score, OutputBuffer *track) {
    uint32_t micros = 60000000u / (uint32_t)score->tempo;
    if (micros > 0xffffff || !micros) return OUTPUT_INVALID_ARGUMENT;
    if (!appendBytes(track, 0, 0xff, 0x51, 3) ||
        !appendBytes(track, (int)(micros >> 16), (int)((micros >> 8) & 0xff), (int)(micros & 0xff), -1)) return OUTPUT_NO_MEMORY;
    bool used[16] = {false};
    for (size_t i = 0; i < score->count; ++i) used[score->events[i].channel] = true;
    for (int channel = 0; channel < 16; ++channel)
        if (used[channel] && !appendBytes(track, 0, 0xc0 | channel, score->instrument, -1)) return OUTPUT_NO_MEMORY;
    if (score->count > SIZE_MAX / 2 / sizeof(Message)) return OUTPUT_NO_MEMORY;
    Message *messages = malloc(2 * score->count * sizeof(Message) + 1);
    if (!messages) return OUTPUT_NO_MEMORY;
    for (size_t i = 0; i < score->count; ++i) {
        const OutputEvent *e = &score->events[i];
        messages[2 * i] = (Message){e->start, true, e->channel, e->note.pitch, 2 * i};
        messages[2 * i + 1] = (Message){e->start + e->duration, false, e->channel, e->note.pitch, 2 * i + 1};
    }
    qsort(messages, 2 * score->count, sizeof(Message), compareMessages);
    uint32_t now = 0;
    bool ok = true;
    for (size_t i = 0; i < 2 * score->count && ok; ++i) {
        const Message *m = &messages[i];
        ok = appendQuantity(track, m->tick - now) &&
            appendBytes(track, (m->on ? 0x90 : 0x80) | m->channel, m->pitch, m->on ? 64 : 0, -1);
        now = m->tick;
    }
    free(messages);
    return ok && appendBytes(track, 0, 0xff, 0x2f, 0) ? OUTPUT_OK : OUTPUT_NO_MEMORY;
}

OutputStatus outputEncodeMidi(const OutputScore *score, OutputBuffer *buffer) {
    if (!buffer) return OUTPUT_INVALID_ARGUMENT;
    *buffer = (OutputBuffer){0};
    if (!outputValidScore(score)) return OUTPUT_INVALID_ARGUMENT;
    OutputBuffer track = {0};
    OutputStatus status = encodeTrack(score, &track);
    if (status == OUTPUT_OK && track.length > 0xffffffffu) status = OUTPUT_NO_MEMORY;
    if (status == OUTPUT_OK) {
        uint32_t length = (uint32_t)track.length;
        const unsigned char header[] = {
            'M','T','h','d', 0,0,0,6, 0,0, 0,1, (unsigned char)(OUTPUT_TICKS_PER_QUARTER >> 8), (unsigned char)OUTPUT_TICKS_PER_QUARTER,
            'M','T','r','k', (unsigned char)(length >> 24), (unsigned char)(length >> 16), (unsigned char)(length >> 8), (unsigned char)length
        };
        if (!outputBufferAppend(buffer, header, sizeof(header)) || !outputBufferAppend(buffer, track.data, track.length))
            status = OUTPUT_NO_MEMORY;
    }
    outputBufferDestroy(&track);
    if (status != OUTPUT_OK) outputBufferDestroy(buffer);
    return status;
}
