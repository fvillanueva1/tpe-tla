#ifndef OUTPUT_INTERNAL_HEADER
#define OUTPUT_INTERNAL_HEADER

/* Helpers shared by the encoders; not part of the contract with the runtime. */

#include "Output.h"

/** Appends bytes, growing the buffer; false when out of memory. */
bool outputBufferAppend(OutputBuffer *buffer, const void *data, size_t length);
/** Appends a NUL-free text. */
bool outputBufferText(OutputBuffer *buffer, const char *text);
/** True when the score and every event are within the limits that outputScoreAdd enforces. */
bool outputValidScore(const OutputScore *score);

#endif
