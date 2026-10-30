#include "Semantic.h"
#include <stdio.h>
#include <string.h>

typedef struct Symbol { const char *name; int type; struct Symbol *next; } Symbol;
typedef struct Scope { Symbol *symbols; struct Scope *parent; Program *program; ProcedureDefinition *procedure; } Scope;

static bool declare(Scope *scope, const char *name, int type) {
    for (Symbol *s = scope->symbols; s; s = s->next)
        if (!strcmp(s->name, name)) { fprintf(stderr, "Semantic error: '%s' is already declared in this scope.\n", name); return false; }
    Symbol *s = malloc(sizeof(*s));
    if (!s) { fprintf(stderr, "Semantic error: out of memory.\n"); return false; }
    *s = (Symbol){name, type, scope->symbols}; scope->symbols = s; return true;
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
static int typeOf(Scope *, Expression *);
static bool compatible(Scope *, Expression *, int);
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
static ProcedureDefinition *findProcedure(Scope *scope, const char *name) {
    for (StatementList *item = scope->program->statements; item; item = item->next)
        if (item->statement->type == PROCEDURE_DEFINITION && !strcmp(item->statement->procedureDefinition->name, name))
            return item->statement->procedureDefinition;
    return NULL;
}
static bool call(Scope *scope, ProcedureCall *call, bool needsValue) {
    ProcedureDefinition *p = findProcedure(scope, call->name);
    if (!p) { fprintf(stderr, "Semantic error: unknown procedure '%s'.\n", call->name); return false; }
    if (needsValue && !p->hasReturnType) { fprintf(stderr, "Semantic error: procedure '%s' has no return value.\n", call->name); return false; }
    ParameterList *param = p->parameters; ExpressionList *arg = call->arguments;
    while (param && arg) {
        if (!expression(scope, arg->expression) || !compatible(scope, arg->expression, param->parameter->dataType + (param->parameter->isVector ? 20 : 0))) return false;
        param = param->next; arg = arg->next;
    }
    if (param || arg) { fprintf(stderr, "Semantic error: incorrect argument count for '%s'.\n", call->name); return false; }
    return true;
}
static bool expression(Scope *scope, Expression *e) {
    if (!e) return true;
    switch (e->type) {
    case VARIABLE: return lookup(scope, e->text);
    case STRING_LITERAL: return parts(scope, e->stringParts);
    case LIST_LITERAL: case NOTES_CHORD: return expressions(scope, e->elements);
    case PROCEDURE_CALL: return call(scope, e->call, true);
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
    Scope local = {NULL, parent, parent->program, parent->procedure}; bool ok = statements(&local, list); clear(&local); return ok;
}
/* shortcut: unsupported expression types remain unchecked, expand as their rules are implemented. */
static int typeOf(Scope *scope, Expression *e) {
    if (!e) return -1;
    switch (e->type) {
    case INTEGER_LITERAL: return TYPE_INTEGER;
    case BOOLEAN_LITERAL: return TYPE_BOOLEAN;
    case NOTE_LITERAL: return TYPE_NOTE;
    case INTERVAL_LITERAL: return TYPE_INTERVAL;
    case KEY_LITERAL: return TYPE_KEY;
    case STRING_LITERAL: return TYPE_STRING;
    case NOTES_CHORD:
        for (ExpressionList *item = e->elements; item; item = item->next)
            if (!compatible(scope, item->expression, TYPE_NOTE)) return -2;
        return TYPE_CHORD;
    case TRIAD_CHORD: case SEVENTH_CHORD: case NINTH_CHORD:
        if (!compatible(scope, e->rightExpression, TYPE_KEY)) return -2;
        if (e->leftExpression->type != DEGREE_LITERAL && !compatible(scope, e->leftExpression, TYPE_INTEGER)) return -2;
        return TYPE_CHORD;
    case SCALE_OF_KEY:
        return compatible(scope, e->operand, TYPE_KEY) ? TYPE_SCALE : -2;
    case VOICE_EXPRESSION:
        return compatible(scope, e->voice->progression, TYPE_PROGRESSION) ? TYPE_VOICING : -2;
    case INVERSION:
        return compatible(scope, e->leftExpression, TYPE_CHORD) && compatible(scope, e->rightExpression, TYPE_INTEGER) ? TYPE_CHORD : -2;
    case MODULATION:
        return compatible(scope, e->leftExpression, TYPE_PROGRESSION) && compatible(scope, e->rightExpression, TYPE_KEY) ? TYPE_PROGRESSION : -2;
    case ARPEGGIATION:
        return compatible(scope, e->leftExpression, TYPE_CHORD) ? TYPE_PROGRESSION : -2;
    case PROCEDURE_CALL: { ProcedureDefinition *p = findProcedure(scope, e->call->name); return p && p->hasReturnType ? p->returnType + (p->returnsVector ? 20 : 0) : -1; }
    case INDEX_ACCESS: {
        int object = typeOf(scope, e->leftExpression), index = typeOf(scope, e->rightExpression);
        if (object == -2 || index == -2) return -2;
        if (index != -1 && index != TYPE_INTEGER) { fprintf(stderr, "Semantic error: index must be an integer.\n"); return -2; }
        if (object == -1) return -1;
        if (object == TYPE_SCALE) return TYPE_NOTE;
        if (object >= 20 && object < 30) return object - 20;
        fprintf(stderr, "Semantic error: value is not indexable.\n"); return -2;
    }
    case PROPERTY_ACCESS: {
        int object = typeOf(scope, e->property.object);
        if (object == -2 || object == -1) return object;
        if (object == TYPE_NOTE && !strcmp(e->property.name, "octave")) return TYPE_INTEGER;
        if (object == TYPE_CHORD && !strcmp(e->property.name, "root")) return TYPE_NOTE;
        if (object == TYPE_CHORD && !strcmp(e->property.name, "quality")) return -1;
        if (object == TYPE_KEY && !strcmp(e->property.name, "tonic")) return TYPE_NOTE;
        if (object == TYPE_KEY && !strcmp(e->property.name, "mode")) return -1;
        if (object == TYPE_PROGRESSION && !strcmp(e->property.name, "length")) return TYPE_INTEGER;
        fprintf(stderr, "Semantic error: unknown property '%s'.\n", e->property.name); return -2;
    }
    case DOMINANT_OF_KEY: case SUBDOMINANT_OF_KEY: case RELATIVE_MINOR_OF_KEY: {
        int t = typeOf(scope, e->operand);
        if (t == -2) return -2;
        if (t != -1 && t != TYPE_KEY) { fprintf(stderr, "Semantic error: tonal relation requires a key.\n"); return -2; }
        return TYPE_KEY;
    }
    case IN_EXPRESSION: {
        int a = typeOf(scope, e->leftExpression), b = typeOf(scope, e->rightExpression);
        if (a == -2 || b == -2) return -2;
        if (a == TYPE_NOTE && (b == TYPE_SCALE || b == TYPE_CHORD)) return TYPE_BOOLEAN;
        return -1;
    }
    case VARIABLE:
        for (Scope *p = scope; p; p = p->parent)
            for (Symbol *symbol = p->symbols; symbol; symbol = symbol->next)
                if (!strcmp(symbol->name, e->text)) return symbol->type;
        return -1;
    case NEGATION: {
        int t = typeOf(scope, e->operand);
        if (t == -1 || t == -2) return t;
        if (t == TYPE_BOOLEAN) return TYPE_BOOLEAN;
        fprintf(stderr, "Semantic error: not requires a boolean.\n"); return -2;
    }
    case CONJUNCTION: case DISJUNCTION: {
        int a = typeOf(scope, e->leftExpression), b = typeOf(scope, e->rightExpression);
        if (a == -2 || b == -2) return -2;
        if ((a != -1 && a != TYPE_BOOLEAN) || (b != -1 && b != TYPE_BOOLEAN)) {
            fprintf(stderr, "Semantic error: logical operators require booleans.\n"); return -2;
        }
        return TYPE_BOOLEAN;
    }
    case EQUALITY: case INEQUALITY: case GREATER_THAN: case GREATER_THAN_OR_EQUAL:
    case LESS_THAN: case LESS_THAN_OR_EQUAL: {
        int a = typeOf(scope, e->leftExpression), b = typeOf(scope, e->rightExpression);
        if (a == -2 || b == -2) return -2;
        if (a != -1 && b != -1 && (a != b || (a != TYPE_INTEGER && a != TYPE_NOTE && a != TYPE_INTERVAL))) {
            fprintf(stderr, "Semantic error: incompatible comparison types.\n"); return -2;
        }
        return TYPE_BOOLEAN;
    }
    case ADDITION: case SUBTRACTION: case MULTIPLICATION: case DIVISION: {
        int a = typeOf(scope, e->leftExpression), b = typeOf(scope, e->rightExpression);
        if (a == -2 || b == -2) return -2;
        if (a == -1 || b == -1) return -1;
        if (a == TYPE_INTEGER && b == TYPE_INTEGER) return TYPE_INTEGER;
        if ((e->type == ADDITION || e->type == SUBTRACTION) && b == TYPE_INTERVAL &&
            (a == TYPE_NOTE || a == TYPE_CHORD || a == TYPE_PROGRESSION)) return a;
        if (e->type == ADDITION && a == TYPE_PROGRESSION && (b == TYPE_PROGRESSION || b == TYPE_CHORD)) return TYPE_PROGRESSION;
        if (e->type == MULTIPLICATION && a == TYPE_PROGRESSION && b == TYPE_INTEGER) return TYPE_PROGRESSION;
        fprintf(stderr, "Semantic error: incompatible operand types.\n"); return -2;
    }
    default: return -1;
    }
}
static bool compatible(Scope *scope, Expression *e, int expected) {
    if (e && e->type == LIST_LITERAL && (expected >= 20 || expected == TYPE_PROGRESSION)) {
        int element = expected >= 20 ? expected - 20 : TYPE_CHORD;
        for (ExpressionList *item = e->elements; item; item = item->next)
            if (!compatible(scope, item->expression, element)) return false;
        return true;
    }
    int actual = typeOf(scope, e);
    if (actual == -2) return false;
    if (actual != -1 && actual != expected) {
        fprintf(stderr, "Semantic error: expression type does not match declaration or assignment.\n"); return false;
    }
    return true;
}
static int symbolType(Scope *scope, const char *name) {
    for (; scope; scope = scope->parent)
        for (Symbol *s = scope->symbols; s; s = s->next)
            if (!strcmp(s->name, name)) return s->type;
    return -1;
}
static bool statement(Scope *scope, Statement *s) {
    switch (s->type) {
    case DECLARATION:
        return expression(scope, s->expression) && compatible(scope, s->expression, s->dataType + (s->isVector ? 20 : 0)) && declare(scope, s->name, s->dataType + (s->isVector ? 20 : 0));
    case ASSIGNMENT:
        return lookup(scope, s->name) && expression(scope, s->expression) && compatible(scope, s->expression, symbolType(scope, s->name));
    case LOG_STATEMENT: return parts(scope, s->log->message);
    case EXPORT_STATEMENT: return expression(scope, s->export->value) && expression(scope, s->export->tempo) && (!s->export->tempo || compatible(scope, s->export->tempo, TYPE_INTEGER));
    case CHECK_STATEMENT: return expression(scope, s->check->voicing) && compatible(scope, s->check->voicing, TYPE_VOICING);
    case PROCEDURE_CALL_STATEMENT: return call(scope, s->procedureCall, false);
    case RETURN_STATEMENT:
        if (!scope->procedure) { fprintf(stderr, "Semantic error: return outside procedure.\n"); return false; }
        if (scope->procedure->hasReturnType != (s->returnStatement->value != NULL)) { fprintf(stderr, "Semantic error: incorrect return value.\n"); return false; }
        return expression(scope, s->returnStatement->value) && (!s->returnStatement->value || compatible(scope, s->returnStatement->value, scope->procedure->returnType + (scope->procedure->returnsVector ? 20 : 0)));
    case IF_STATEMENT:
        return expression(scope, s->ifStatement->condition) && compatible(scope, s->ifStatement->condition, TYPE_BOOLEAN) && block(scope, s->ifStatement->thenBlock) && block(scope, s->ifStatement->elseBlock);
    case WHILE_STATEMENT: return expression(scope, s->whileStatement->condition) && compatible(scope, s->whileStatement->condition, TYPE_BOOLEAN) && block(scope, s->whileStatement->body);
    case FOR_STATEMENT: {
        For *f = s->forStatement; Scope local = {NULL, scope, scope->program, scope->procedure};
        bool ok = expression(scope, f->from) && expression(scope, f->to) && compatible(scope, f->from, TYPE_INTEGER) && compatible(scope, f->to, TYPE_INTEGER) && declare(&local, f->variable, TYPE_INTEGER) && statements(&local, f->body);
        clear(&local); return ok;
    }
    case PROCEDURE_DEFINITION: {
        ProcedureDefinition *p = s->procedureDefinition; Scope local = {NULL, scope, scope->program, p}; bool ok = true;
        if (scope->parent) { fprintf(stderr, "Semantic error: nested procedure definition.\n"); return false; }
        for (StatementList *item = scope->program->statements; item; item = item->next)
            if (item->statement != s && item->statement->type == PROCEDURE_DEFINITION && !strcmp(item->statement->procedureDefinition->name, p->name)) { fprintf(stderr, "Semantic error: duplicate procedure '%s'.\n", p->name); return false; }
        for (ParameterList *item = p->parameters; item && ok; item = item->next) ok = declare(&local, item->parameter->name, item->parameter->dataType + (item->parameter->isVector ? 20 : 0));
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
    Scope global = {NULL, NULL, program, NULL}; bool ok = statements(&global, program->statements); clear(&global);
    return ok ? SUCCEEDED : FAILED;
}
