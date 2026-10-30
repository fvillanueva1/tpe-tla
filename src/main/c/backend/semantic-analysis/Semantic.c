#include "Semantic.h"
#include <stdio.h>
#include <string.h>

typedef struct Symbol { const char *name; struct Symbol *next; } Symbol;
typedef struct Scope { Symbol *symbols; struct Scope *parent; } Scope;

static bool declare(Scope *scope, const char *name) {
    for (Symbol *s = scope->symbols; s; s = s->next)
        if (!strcmp(s->name, name)) { fprintf(stderr, "Semantic error: '%s' is already declared in this scope.\n", name); return false; }
    Symbol *s = malloc(sizeof(*s));
    if (!s) { fprintf(stderr, "Semantic error: out of memory.\n"); return false; }
    *s = (Symbol){name, scope->symbols}; scope->symbols = s; return true;
}
static bool lookup(Scope *scope, const char *name) {
    for (; scope; scope = scope->parent)
        for (Symbol *s = scope->symbols; s; s = s->next)
            if (!strcmp(s->name, name)) return true;
    fprintf(stderr, "Semantic error: variable '%s' is not declared.\n", name); return false;
}
static void clear(Scope *scope) {
    while (scope->symbols) { Symbol *s = scope->symbols; scope->symbols = s->next; free(s); }
}
static bool expression(Scope *, Expression *);
static bool statements(Scope *, StatementList *);
static bool parts(Scope *scope, StringPart *part) {
    for (; part; part = part->next)
        if (part->type == STRING_PART_VARIABLE && !lookup(scope, part->text)) return false;
    return true;
}
static bool expressions(Scope *scope, ExpressionList *list) {
    for (; list; list = list->next) if (!expression(scope, list->expression)) return false;
    return true;
}
static bool expression(Scope *scope, Expression *e) {
    if (!e) return true;
    switch (e->type) {
    case VARIABLE: return lookup(scope, e->text);
    case STRING_LITERAL: return parts(scope, e->stringParts);
    case LIST_LITERAL: case NOTES_CHORD: return expressions(scope, e->elements);
    case PROCEDURE_CALL: return expressions(scope, e->call->arguments);
    case PROPERTY_ACCESS: return expression(scope, e->property.object);
    case VOICE_EXPRESSION: return expression(scope, e->voice->progression);
    case NEGATION: case DOMINANT_OF_KEY: case RELATIVE_MINOR_OF_KEY:
    case SCALE_OF_KEY: case SUBDOMINANT_OF_KEY: return expression(scope, e->operand);
    case ADDITION: case ARPEGGIATION: case CONJUNCTION: case DISJUNCTION:
    case DIVISION: case EQUALITY: case GREATER_THAN: case GREATER_THAN_OR_EQUAL:
    case INDEX_ACCESS: case INEQUALITY: case IN_EXPRESSION: case INVERSION:
    case LESS_THAN: case LESS_THAN_OR_EQUAL: case MODULATION: case MULTIPLICATION:
    case NINTH_CHORD: case SEVENTH_CHORD: case SUBTRACTION: case TRIAD_CHORD:
        return expression(scope, e->leftExpression) && expression(scope, e->rightExpression);
    default: return true;
    }
}
static bool block(Scope *parent, StatementList *list) {
    Scope local = {NULL, parent}; bool ok = statements(&local, list); clear(&local); return ok;
}
static bool statement(Scope *scope, Statement *s) {
    switch (s->type) {
    case DECLARATION:
        return expression(scope, s->expression) && declare(scope, s->name);
    case ASSIGNMENT:
        return lookup(scope, s->name) && expression(scope, s->expression);
    case LOG_STATEMENT: return parts(scope, s->log->message);
    case EXPORT_STATEMENT: return expression(scope, s->export->value) && expression(scope, s->export->tempo);
    case CHECK_STATEMENT: return expression(scope, s->check->voicing);
    case PROCEDURE_CALL_STATEMENT: return expressions(scope, s->procedureCall->arguments);
    case RETURN_STATEMENT: return expression(scope, s->returnStatement->value);
    case IF_STATEMENT:
        return expression(scope, s->ifStatement->condition) && block(scope, s->ifStatement->thenBlock) && block(scope, s->ifStatement->elseBlock);
    case WHILE_STATEMENT: return expression(scope, s->whileStatement->condition) && block(scope, s->whileStatement->body);
    case FOR_STATEMENT: {
        For *f = s->forStatement; Scope local = {NULL, scope};
        bool ok = expression(scope, f->from) && expression(scope, f->to) && declare(&local, f->variable) && statements(&local, f->body);
        clear(&local); return ok;
    }
    case PROCEDURE_DEFINITION: {
        ProcedureDefinition *p = s->procedureDefinition; Scope local = {NULL, scope}; bool ok = true;
        for (ParameterList *item = p->parameters; item && ok; item = item->next) ok = declare(&local, item->parameter->name);
        if (ok) ok = statements(&local, p->body);
        clear(&local); return ok;
    }
    }
    return false;
}
static bool statements(Scope *scope, StatementList *list) {
    for (; list; list = list->next) if (!statement(scope, list->statement)) return false;
    return true;
}
CompilationStatus analyzeSemantics(Program *program) {
    Scope global = {NULL, NULL}; bool ok = statements(&global, program->statements); clear(&global);
    return ok ? SUCCEEDED : FAILED;
}
