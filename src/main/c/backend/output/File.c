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
    temporary->path = name; /* Keep the name even on write failure, for centralized cleanup. */
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
    return ok ? OUTPUT_OK : OUTPUT_WRITE_ERROR;
}

/*
 * Gives the temporary file its final name without overwriting: link() fails if
 * the destination exists. Fail if the file system cannot publish safely.
 */
static OutputStatus publish(const char *temporary, const char *path) {
    if (link(temporary, path) == 0) return OUTPUT_OK;
    if (errno == EEXIST) return OUTPUT_FILE_EXISTS;
    return OUTPUT_WRITE_ERROR;
}

OutputStatus outputWriteFilesWithCleanup(const OutputFile *files, size_t count, size_t *failed, char **residualPath) {
    if (failed) *failed = 0;
    if (residualPath) *residualPath = NULL;
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
    /* First every file goes to its temporary; nothing is visible yet. */
    for (size_t i = 0; i < count; ++i) {
        OutputBuffer buffer = {0};
        status = encode(&files[i], &buffer);
        if (status == OUTPUT_OK) status = writeTemporary(files[i].path, &buffer, &temporaries[i]);
        outputBufferDestroy(&buffer);
        if (status != OUTPUT_OK) { if (failed) *failed = i; break; }
    }
    size_t published = 0;
    if (status == OUTPUT_OK) {
        for (; published < count; ++published) {
            status = publish(temporaries[published].path, files[published].path);
            if (status != OUTPUT_OK) { if (failed) *failed = published; break; }
        }
    }
    /* Try every cleanup, even after an earlier removal failed. ENOENT is already clean. */
    size_t cleanupIndex = count;
    bool cleanupTemporary = false;
    for (size_t i = 0; i < count; ++i) {
        if (temporaries[i].path && unlink(temporaries[i].path) != 0 && errno != ENOENT) {
            if (cleanupIndex == count) { cleanupIndex = i; cleanupTemporary = true; }
            status = OUTPUT_WRITE_ERROR;
        }
    }
    if (status != OUTPUT_OK) {
        /* Also roll back publications when a temporary could not be removed. */
        for (size_t i = 0; i < published; ++i) {
            struct stat current;
            bool cleanupFailed = false;
            if (lstat(files[i].path, &current) != 0) cleanupFailed = errno != ENOENT;
            /* A replacement belongs to its creator, not to this batch. */
            else if (current.st_dev == temporaries[i].identity.st_dev &&
                     current.st_ino == temporaries[i].identity.st_ino)
                cleanupFailed = unlink(files[i].path) != 0 && errno != ENOENT;
            if (cleanupFailed) {
                if (cleanupIndex == count) cleanupIndex = i;
                status = OUTPUT_WRITE_ERROR;
            }
        }
    }
    if (cleanupIndex != count) {
        if (failed) *failed = cleanupIndex;
        if (residualPath) {
            /* The existing name buffer fits either path: no allocation can hide a cleanup failure. */
            if (!cleanupTemporary) strcpy(temporaries[cleanupIndex].path, files[cleanupIndex].path);
            *residualPath = temporaries[cleanupIndex].path;
            temporaries[cleanupIndex].path = NULL;
        }
    }
    for (size_t i = 0; i < count; ++i) free(temporaries[i].path);
    free(temporaries);
    return status;
}

OutputStatus outputWriteFiles(const OutputFile *files, size_t count, size_t *failed) {
    return outputWriteFilesWithCleanup(files, count, failed, NULL);
}

OutputStatus outputWriteMidi(const OutputScore *score, const char *path) {
    OutputFile file = {score, OUTPUT_FORMAT_MIDI, path};
    return outputWriteFiles(&file, 1, NULL);
}

OutputStatus outputWriteLilyPond(const OutputScore *score, const char *path) {
    OutputFile file = {score, OUTPUT_FORMAT_LILYPOND, path};
    return outputWriteFiles(&file, 1, NULL);
}
