#include "OutputInternal.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void outputBufferDestroy(OutputBuffer *buffer) {
    if (!buffer) return;
    free(buffer->data);
    buffer->data = NULL;
    buffer->length = buffer->capacity = 0;
}

bool outputBufferAppend(OutputBuffer *buffer, const void *data, size_t length) {
    if (length > SIZE_MAX - buffer->length) return false;
    if (buffer->length + length > buffer->capacity) {
        size_t capacity = buffer->capacity ? buffer->capacity : 256;
        while (capacity < buffer->length + length) {
            if (capacity > SIZE_MAX / 2) return false;
            capacity *= 2;
        }
        unsigned char *grown = realloc(buffer->data, capacity);
        if (!grown) return false;
        buffer->data = grown;
        buffer->capacity = capacity;
    }
    if (length) memcpy(buffer->data + buffer->length, data, length);
    buffer->length += length;
    return true;
}

bool outputBufferText(OutputBuffer *buffer, const char *text) {
    return outputBufferAppend(buffer, text, strlen(text));
}

bool outputValidScore(const OutputScore *score) {
    if (!score || (!score->events && score->count) || score->tempo < 1 ||
        score->instrument < 0 || score->instrument > 127) return false;
    for (size_t i = 0; i < score->count; ++i) {
        const OutputEvent *e = &score->events[i];
        if (e->channel < 0 || e->channel > 15 || !e->duration || e->start > UINT32_MAX - e->duration ||
            e->note.pitch < MUSIC_LOWEST_PITCH || e->note.pitch > MUSIC_HIGHEST_PITCH) return false;
    }
    return true;
}
