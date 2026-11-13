#include "../../main/c/backend/runtime/Runtime.h"
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char residual[256];
int __real_unlink(const char *);
int __wrap_unlink(const char *path) {
    if (!strncmp(path, "first.mid.", strlen("first.mid."))) {
        assert(strlen(path) < sizeof(residual));
        strcpy(residual, path);
        errno = EACCES;
        return -1;
    }
    return __real_unlink(path);
}

/* Exercise the runtime's diagnostic and error status, beyond the writer's return value. */
int main(void) {
    char directory[] = "/tmp/cadence-runtime-cleanup-XXXXXX";
    assert(mkdtemp(directory) && chdir(directory) == 0);
    Expression note = {.type = NOTE_LITERAL, .text = "C4"};
    Export first = {.value = &note, .format = EXPORT_MIDI, .path = "first.mid"};
    Export second = {.value = &note, .format = EXPORT_MIDI, .path = "second.mid"};
    Statement statements[] = {{.type = EXPORT_STATEMENT, .export = &first}, {.type = EXPORT_STATEMENT, .export = &second}};
    StatementList tail = {&statements[1], NULL}, head = {&statements[0], &tail};
    Program program = {&head};
    FILE *diagnostic = tmpfile();
    int saved = dup(STDERR_FILENO);
    assert(diagnostic && saved >= 0 && dup2(fileno(diagnostic), STDERR_FILENO) >= 0);
    assert(executeProgram(&program) == FAILED);
    assert(fflush(stderr) == 0 && dup2(saved, STDERR_FILENO) >= 0 && close(saved) == 0);
    rewind(diagnostic);
    char actual[512], expected[512];
    snprintf(expected, sizeof(expected), "Runtime error: cannot remove file '%s'.\n", residual);
    assert(residual[0] && fgets(actual, sizeof(actual), diagnostic) && !strcmp(actual, expected));
    assert(fgetc(diagnostic) == EOF && fclose(diagnostic) == 0);
    assert(access("first.mid", F_OK) != 0 && access("second.mid", F_OK) != 0);
    DIR *dir = opendir(".");
    assert(dir);
    size_t count = 0;
    for (struct dirent *entry; (entry = readdir(dir));) if (entry->d_name[0] != '.') ++count;
    assert(closedir(dir) == 0 && count == 1 && __real_unlink(residual) == 0);
    assert(chdir("/") == 0 && rmdir(directory) == 0);
    return 0;
}
