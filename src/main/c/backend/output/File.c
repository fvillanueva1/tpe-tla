#include "OutputInternal.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static OutputStatus encode(const OutputFile *file, OutputBuffer *buffer) {
    switch (file->format) {
    case OUTPUT_FORMAT_MIDI: return outputEncodeMidi(file->score, buffer);
    case OUTPUT_FORMAT_LILYPOND: return outputEncodeLilyPond(file->score, buffer);
    default: return OUTPUT_INVALID_ARGUMENT;
    }
}

/* Creates the file with the buffer; "x" makes fopen fail if it already exists. */
static OutputStatus writeFile(const char *path, const OutputBuffer *buffer) {
    FILE *file = fopen(path, "wbx");
    if (!file) return errno == EEXIST ? OUTPUT_FILE_EXISTS : OUTPUT_WRITE_ERROR;
    bool ok = fwrite(buffer->data, 1, buffer->length, file) == buffer->length;
    if (fclose(file) != 0) ok = false;
    if (!ok) remove(path);
    return ok ? OUTPUT_OK : OUTPUT_WRITE_ERROR;
}

OutputStatus outputWriteFiles(const OutputFile *files, size_t count, size_t *failed) {
    if (failed) *failed = 0;
    if (!files && count) return OUTPUT_INVALID_ARGUMENT;
    for (size_t i = 0; i < count; ++i) {
        if (failed) *failed = i;
        if (!files[i].path || !*files[i].path || !files[i].score) return OUTPUT_INVALID_ARGUMENT;
        for (size_t j = 0; j < i; ++j) if (!strcmp(files[i].path, files[j].path)) return OUTPUT_FILE_EXISTS;
        if (access(files[i].path, F_OK) == 0) return OUTPUT_FILE_EXISTS;
    }
    OutputStatus status = OUTPUT_OK;
    size_t written = 0;
    for (; written < count && status == OUTPUT_OK; ++written) {
        OutputBuffer buffer = {0};
        status = encode(&files[written], &buffer);
        if (status == OUTPUT_OK) status = writeFile(files[written].path, &buffer);
        outputBufferDestroy(&buffer);
    }
    if (status != OUTPUT_OK) {
        /* The file at written - 1 failed; remove the ones before it. */
        if (failed) *failed = written - 1;
        for (size_t i = 0; i + 1 < written; ++i) remove(files[i].path);
    }
    return status;
}

OutputStatus outputWriteMidi(const OutputScore *score, const char *path) {
    OutputFile file = {score, OUTPUT_FORMAT_MIDI, path};
    return outputWriteFiles(&file, 1, NULL);
}

OutputStatus outputWriteLilyPond(const OutputScore *score, const char *path) {
    OutputFile file = {score, OUTPUT_FORMAT_LILYPOND, path};
    return outputWriteFiles(&file, 1, NULL);
}
