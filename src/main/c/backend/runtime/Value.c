#include "Value.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char * _durationNames[] = {
	[DURATION_WHOLE] = "whole",
	[DURATION_HALF] = "half",
	[DURATION_QUARTER] = "quarter",
	[DURATION_EIGHTH] = "eighth",
	[DURATION_SIXTEENTH] = "sixteenth"
};

bool durationFromLexeme(const char * lexeme, Duration * duration) {
	for (size_t k = 0; k < sizeof(_durationNames) / sizeof(_durationNames[0]); ++k) {
		if (strcmp(lexeme, _durationNames[k]) == 0) {
			*duration = (Duration) k;
			return true;
		}
	}
	return false;
}

const char * durationName(Duration duration) {
	return _durationNames[duration];
}

bool copyValue(const Value * source, Value * target) {
	*target = *source;
	if (source->isVector) {
		target->vector.items = NULL;
		target->vector.count = 0;
		if (source->vector.count == 0) return true;
		Value * items = calloc(source->vector.count, sizeof(Value));
		if (items == NULL) return false;
		for (size_t k = 0; k < source->vector.count; ++k) {
			if (!copyValue(&source->vector.items[k], &items[k])) {
				for (size_t j = 0; j < k; ++j) destroyValue(&items[j]);
				free(items);
				return false;
			}
		}
		target->vector.items = items;
		target->vector.count = source->vector.count;
		return true;
	}
	if (source->type == TYPE_PROGRESSION) return musicProgressionCopy(&source->progression, &target->progression);
	if (source->type == TYPE_VOICING) return musicVoicingCopy(&source->voicing, &target->voicing);
	if (source->type == TYPE_STRING) {
		target->string = strdup(source->string);
		return target->string != NULL;
	}
	return true;
}

void destroyValue(Value * value) {
	if (value->isVector) {
		for (size_t k = 0; k < value->vector.count; ++k) destroyValue(&value->vector.items[k]);
		free(value->vector.items);
		value->vector.items = NULL;
		value->vector.count = 0;
	}
	else if (value->type == TYPE_PROGRESSION) musicProgressionDestroy(&value->progression);
	else if (value->type == TYPE_VOICING) musicVoicingDestroy(&value->voicing);
	else if (value->type == TYPE_STRING) {
		free(value->string);
		value->string = NULL;
	}
}

bool appendText(TextBuffer * buffer, const char * text, size_t length) {
	if (buffer->length + length + 1 > buffer->capacity) {
		size_t capacity = buffer->capacity ? buffer->capacity : 32;
		while (buffer->length + length + 1 > capacity) capacity *= 2;
		char * grown = realloc(buffer->text, capacity);
		if (grown == NULL) return false;
		buffer->text = grown;
		buffer->capacity = capacity;
	}
	memcpy(buffer->text + buffer->length, text, length);
	buffer->length += length;
	buffer->text[buffer->length] = '\0';
	return true;
}

static bool appendNotes(TextBuffer * buffer, const MusicNote * notes, size_t count, bool chord) {
	if (!appendText(buffer, chord ? "{" : "[", 1)) return false;
	for (size_t k = 0; k < count; ++k) {
		char text[MUSIC_NOTE_TEXT_SIZE];
		musicFormatNote(notes[k], text);
		if ((k && !appendText(buffer, ", ", 2)) || !appendText(buffer, text, strlen(text))) return false;
	}
	return appendText(buffer, chord ? "}" : "]", 1);
}

bool appendValue(TextBuffer * buffer, const Value * value) {
	if (value->isVector) {
		if (!appendText(buffer, "[", 1)) return false;
		for (size_t k = 0; k < value->vector.count; ++k) {
			if (k > 0 && !appendText(buffer, ", ", 2)) return false;
			if (!appendValue(buffer, &value->vector.items[k])) return false;
		}
		return appendText(buffer, "]", 1);
	}
	char number[64];
	const char * text;
	switch (value->type) {
		case TYPE_NOTE:
			musicFormatNote(value->note, number);
			text = number;
			break;
		case TYPE_INTERVAL: {
			static const char * names[] = {"P1", "P4", "P5", "P8", "m2", "m3", "m6", "m7", "M2", "M3", "M6", "M7", "d5", "aug4"};
			text = "?";
			for (size_t k = 0; k < sizeof(names) / sizeof(names[0]); ++k) {
				MusicInterval interval;
				musicParseInterval(names[k], &interval);
				if (interval.steps == value->interval.steps && interval.semitones == value->interval.semitones) { text = names[k]; break; }
			}
			break;
		}
		case TYPE_KEY: {
			MusicNote tonic;
			musicKeyTonic(value->key, &tonic);
			char note[MUSIC_NOTE_TEXT_SIZE];
			musicFormatNote(tonic, note);
			/* A key has no octave; its tonic is always written in octave four. */
			note[strlen(note) - 1] = '\0';
			snprintf(number, sizeof(number), "%s %s%s", note, musicModeName(value->key.mode), value->key.strict ? " strict" : "");
			text = number;
			break;
		}
		case TYPE_SCALE:
			return appendNotes(buffer, value->scale.notes, 7, false);
		case TYPE_CHORD:
			return appendNotes(buffer, value->chord.notes, value->chord.count, true);
		case TYPE_PROGRESSION:
		case TYPE_VOICING: {
			if (!appendText(buffer, "[", 1)) return false;
			size_t count = value->type == TYPE_PROGRESSION ? value->progression.count : value->voicing.count;
			for (size_t k = 0; k < count; ++k) {
				if (k && !appendText(buffer, ", ", 2)) return false;
				MusicDuration duration;
				if (value->type == TYPE_PROGRESSION) {
					const MusicStep * step = &value->progression.steps[k];
					if (!appendNotes(buffer, step->chord.notes, step->chord.count, true)) return false;
					duration = step->duration;
				} else {
					MusicNote notes[VOICE_COUNT];
					for (size_t v = 0; v < VOICE_COUNT; ++v) musicNoteFromPitch(value->voicing.chords[k][v], &notes[v]);
					if (!appendNotes(buffer, notes, VOICE_COUNT, true)) return false;
					duration = value->voicing.durations[k];
				}
				if (duration != MUSIC_DEFAULT_DURATION) {
					const char * name = musicDurationName(duration);
					if (!appendText(buffer, " as ", 4) || !appendText(buffer, name, strlen(name))) return false;
				}
			}
			return appendText(buffer, "]", 1);
		}
		case VALUE_QUALITY:
			text = musicQualityName(value->quality);
			break;
		case VALUE_MODE:
			text = musicModeName(value->mode);
			break;
		case TYPE_INTEGER:
			snprintf(number, sizeof(number), "%d", value->integer);
			text = number;
			break;
		case TYPE_BOOLEAN:
			text = value->boolean ? "true" : "false";
			break;
		case TYPE_DURATION:
			text = durationName(value->duration);
			break;
		case TYPE_STRING:
			text = value->string;
			break;
		default:
			text = "?";
			break;
	}
	return appendText(buffer, text, strlen(text));
}

bool bindValue(RuntimeScope * scope, const char * name, Value * value) {
	Binding * binding = malloc(sizeof(Binding));
	if (binding == NULL) return false;
	binding->name = name;
	binding->value = *value;
	binding->next = scope->bindings;
	scope->bindings = binding;
	return true;
}

Binding * findBinding(RuntimeScope * scope, const char * name) {
	for (; scope != NULL; scope = scope->parent) {
		for (Binding * binding = scope->bindings; binding != NULL; binding = binding->next) {
			if (strcmp(binding->name, name) == 0) return binding;
		}
	}
	return NULL;
}

void clearScope(RuntimeScope * scope) {
	while (scope->bindings != NULL) {
		Binding * binding = scope->bindings;
		scope->bindings = binding->next;
		destroyValue(&binding->value);
		free(binding);
	}
}
