#include "OutputInternal.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static OutputStatus encode(const OutputFile *file, OutputBuffer *buffer) {
    switch (file->format) {
    case OUTPUT_FORMAT_MIDI: return outputEncodeMidi(file->score, buffer);
    case OUTPUT_FORMAT_LILYPOND: return outputEncodeLilyPond(file->score, buffer);
    default: return OUTPUT_INVALID_ARGUMENT;
    }
}

static bool exists(const char *path) {
    struct stat information;
    return lstat(path, &information) == 0 || errno != ENOENT;
}

typedef struct {
    char *path;
    struct stat identity;
} TemporaryFile;

/* Writes the buffer to a new temporary file next to "path"; "temporary" receives its name. */
static OutputStatus writeTemporary(const char *path, const OutputBuffer *buffer, TemporaryFile *temporary) {
    size_t length = strlen(path);
    char *name = malloc(length + sizeof(".XXXXXX"));
    if (!name) return OUTPUT_NO_MEMORY;
    memcpy(name, path, length);
    memcpy(name + length, ".XXXXXX", sizeof(".XXXXXX"));
    int descriptor = mkstemp(name);
    if (descriptor < 0) { free(name); return OUTPUT_WRITE_ERROR; }
    /* mkstemp creates the file for the owner only; give it the usual permissions. */
    mode_t mask = umask(0);
    umask(mask);
    bool ok = fstat(descriptor, &temporary->identity) == 0 && fchmod(descriptor, 0666 & ~mask) == 0;
    for (size_t written = 0; ok && written < buffer->length;) {
        ssize_t result = write(descriptor, buffer->data + written, buffer->length - written);
        if (result < 0 && errno == EINTR) continue;
        ok = result > 0;
        if (ok) written += (size_t)result;
    }
    if (close(descriptor) != 0) ok = false;
    if (!ok) { unlink(name); free(name); return OUTPUT_WRITE_ERROR; }
    temporary->path = name;
    return OUTPUT_OK;
}

/*
 * Gives the temporary file its final name without overwriting: link() fails if
 * the destination exists. Fail if the file system cannot publish safely.
 */
static OutputStatus publish(const char *temporary, const char *path) {
    if (link(temporary, path) == 0) { unlink(temporary); return OUTPUT_OK; }
    if (errno == EEXIST) return OUTPUT_FILE_EXISTS;
    return OUTPUT_WRITE_ERROR;
}

OutputStatus outputWriteFiles(const OutputFile *files, size_t count, size_t *failed) {
    if (failed) *failed = 0;
    if (!files && count) return OUTPUT_INVALID_ARGUMENT;
    for (size_t i = 0; i < count; ++i) {
        if (failed) *failed = i;
        if (!files[i].path || !*files[i].path || !files[i].score) return OUTPUT_INVALID_ARGUMENT;
        for (size_t j = 0; j < i; ++j) if (!strcmp(files[i].path, files[j].path)) return OUTPUT_FILE_EXISTS;
        if (exists(files[i].path)) return OUTPUT_FILE_EXISTS;
    }
    if (!count) return OUTPUT_OK;
    TemporaryFile *temporaries = calloc(count, sizeof(*temporaries));
    if (!temporaries) return OUTPUT_NO_MEMORY;
    OutputStatus status = OUTPUT_OK;
    size_t index = 0;
    /* First every file goes to its temporary; nothing is visible yet. */
    for (; index < count && status == OUTPUT_OK; ++index) {
        OutputBuffer buffer = {0};
        status = encode(&files[index], &buffer);
        if (status == OUTPUT_OK) status = writeTemporary(files[index].path, &buffer, &temporaries[index]);
        outputBufferDestroy(&buffer);
    }
    size_t published = 0;
    if (status == OUTPUT_OK)
        for (index = 0; index < count && status == OUTPUT_OK; ++index, ++published)
            status = publish(temporaries[index].path, files[index].path);
    if (status != OUTPUT_OK) {
        if (failed) *failed = index - 1;
        /* Undo: the files already published and every temporary left. */
        for (size_t i = 0; i + 1 < published; ++i) {
            struct stat current;
            /* A replacement belongs to its creator, not to this batch. */
            if (lstat(files[i].path, &current) == 0 &&
                current.st_dev == temporaries[i].identity.st_dev &&
                current.st_ino == temporaries[i].identity.st_ino) unlink(files[i].path);
        }
        for (size_t i = 0; i < count; ++i) if (temporaries[i].path) unlink(temporaries[i].path);
    }
    for (size_t i = 0; i < count; ++i) free(temporaries[i].path);
    free(temporaries);
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
