%top{

#include "../../support/type/CompilationStatus.h"
#include "../../support/type/FlexContext.h"
#include "../../support/type/TokenLabel.h"
#include "../syntactic-analysis/AbstractSyntaxTree.h"
#include "../syntactic-analysis/BisonParser.h"

}

%{

/**
 * @see https://westes.github.io/flex/manual/Definitions-Section.html#Definitions-Section
 */

extern CompilationStatus EnterMultilineCommentLexemeAction(FlexContext context);
extern CompilationStatus EOFLexemeAction();
extern CompilationStatus IgnoredLexemeAction();
extern CompilationStatus IntegerLexemeAction();
extern CompilationStatus LeaveMultilineCommentLexemeAction();
extern CompilationStatus TokenLexemeAction(TokenLabel label);
extern CompilationStatus UnknownLexemeAction();

%}

/**
 * Flex contexts (a.k.a. start conditions).
 *
 * @see https://westes.github.io/flex/manual/Start-Conditions.html
 */
%x MULTILINE_COMMENT

/**
 * Reusable patterns.
 *
 * @see https://westes.github.io/flex/manual/Matching.html
 * @see https://westes.github.io/flex/manual/Patterns.html
 */
whitespace			[[:space:]]+

%%

%{

/**
 * @see https://westes.github.io/flex/manual/Rules-Section.html#Rules-Section
 */

%}

"/*"								{ return EnterMultilineCommentLexemeAction(MULTILINE_COMMENT); }
<MULTILINE_COMMENT>"*/"				{ return LeaveMultilineCommentLexemeAction(); }
<MULTILINE_COMMENT>[^*]+			{ return IgnoredLexemeAction(); }
<MULTILINE_COMMENT>.				{ return IgnoredLexemeAction(); }

	/* Tipos de datos. */
"note"								{ return TokenLexemeAction(NOTE_TYPE); }
"interval"							{ return TokenLexemeAction(INTERVAL_TYPE); }
"key"								{ return TokenLexemeAction(KEY); }
"scale"								{ return TokenLexemeAction(SCALE); }
"chord"								{ return TokenLexemeAction(CHORD); }
"progression"						{ return TokenLexemeAction(PROGRESSION); }
"voicing"							{ return TokenLexemeAction(VOICING); }
"integer"							{ return TokenLexemeAction(INTEGER_TYPE); }
"boolean"							{ return TokenLexemeAction(BOOLEAN_TYPE); }
"string"							{ return TokenLexemeAction(STRING_TYPE); }

	/* Modos y calidades. */
"major"								{ return TokenLexemeAction(MAJOR); }
"minor"								{ return TokenLexemeAction(MINOR); }
"dorian"							{ return TokenLexemeAction(DORIAN); }
"phrygian"							{ return TokenLexemeAction(PHRYGIAN); }
"lydian"							{ return TokenLexemeAction(LYDIAN); }
"mixolydian"						{ return TokenLexemeAction(MIXOLYDIAN); }
"locrian"							{ return TokenLexemeAction(LOCRIAN); }
"strict"							{ return TokenLexemeAction(STRICT); }
"diminished"						{ return TokenLexemeAction(DIMINISHED); }
"augmented"							{ return TokenLexemeAction(AUGMENTED); }

	/* Acordes y progresiones. */
"triad"								{ return TokenLexemeAction(TRIAD); }
"seventh"							{ return TokenLexemeAction(SEVENTH); }
"ninth"								{ return TokenLexemeAction(NINTH); }
"on"								{ return TokenLexemeAction(ON); }
"of"								{ return TokenLexemeAction(OF); }
"in"								{ return TokenLexemeAction(IN); }

	/* Transformaciones. */
"invert"							{ return TokenLexemeAction(INVERT); }
"by"								{ return TokenLexemeAction(BY); }
"arpeggiate"						{ return TokenLexemeAction(ARPEGGIATE); }
"as"								{ return TokenLexemeAction(AS); }
"modulate"							{ return TokenLexemeAction(MODULATE); }
"to"								{ return TokenLexemeAction(TO); }
"relative"							{ return TokenLexemeAction(RELATIVE); }
"dominant"							{ return TokenLexemeAction(DOMINANT); }
"subdominant"						{ return TokenLexemeAction(SUBDOMINANT); }

	/* Conducción de voces. */
"voice"								{ return TokenLexemeAction(VOICE); }
"SATB"								{ return TokenLexemeAction(SATB); }
"check"								{ return TokenLexemeAction(CHECK); }
"for"								{ return TokenLexemeAction(FOR); }
"parallel_fifths"					{ return TokenLexemeAction(PARALLEL_FIFTHS); }
"parallel_octaves"					{ return TokenLexemeAction(PARALLEL_OCTAVES); }
"voice_crossing"					{ return TokenLexemeAction(VOICE_CROSSING); }

	/* Control de flujo y procedimientos. */
"if"								{ return TokenLexemeAction(IF); }
"else"								{ return TokenLexemeAction(ELSE); }
"while"								{ return TokenLexemeAction(WHILE); }
"define"							{ return TokenLexemeAction(DEFINE); }
"return"							{ return TokenLexemeAction(RETURN); }

	/* Salida. */
"export"							{ return TokenLexemeAction(EXPORT); }
"midi"								{ return TokenLexemeAction(MIDI); }
"sheet"								{ return TokenLexemeAction(SHEET); }
"at"								{ return TokenLexemeAction(AT); }
"bpm"								{ return TokenLexemeAction(BPM); }
"with"								{ return TokenLexemeAction(WITH); }
"log"								{ return TokenLexemeAction(LOG); }

	/* Lógicos. */
"and"								{ return TokenLexemeAction(AND); }
"or"								{ return TokenLexemeAction(OR); }
"not"								{ return TokenLexemeAction(NOT); }
"true"								{ return TokenLexemeAction(TRUE); }
"false"								{ return TokenLexemeAction(FALSE); }

	/* Operadores relacionales y de asignación. */
"=="								{ return TokenLexemeAction(EQUAL_EQUAL); }
"!="								{ return TokenLexemeAction(NOT_EQUAL); }
"<="								{ return TokenLexemeAction(LESS_EQUAL); }
">="								{ return TokenLexemeAction(GREATER_EQUAL); }
"<"									{ return TokenLexemeAction(LESS); }
">"									{ return TokenLexemeAction(GREATER); }
"="									{ return TokenLexemeAction(ASSIGN); }
"->"								{ return TokenLexemeAction(ARROW); }

	/* Operadores aritméticos. */
"-"									{ return TokenLexemeAction(SUB); }
"*"									{ return TokenLexemeAction(MUL); }
"/"									{ return TokenLexemeAction(DIV); }
"+"									{ return TokenLexemeAction(ADD); }

	/* Puntuación. */
";"									{ return TokenLexemeAction(SEMICOLON); }
","									{ return TokenLexemeAction(COMMA); }
"."									{ return TokenLexemeAction(DOT); }
"("									{ return TokenLexemeAction(OPEN_PARENTHESIS); }
")"									{ return TokenLexemeAction(CLOSE_PARENTHESIS); }
"{"									{ return TokenLexemeAction(OPEN_BRACE); }
"}"									{ return TokenLexemeAction(CLOSE_BRACE); }
"["									{ return TokenLexemeAction(OPEN_BRACKET); }
"]"									{ return TokenLexemeAction(CLOSE_BRACKET); }

[[:digit:]]+						{ return IntegerLexemeAction(); }

{whitespace}						{ return IgnoredLexemeAction(); }
.									{ return UnknownLexemeAction(); }

<<EOF>>								{ return EOFLexemeAction(); }

%%

/**
 * @see https://westes.github.io/flex/manual/User-Code-Section.html#User-Code-Section
 */

#include "FlexActions.h"
#include "FlexExport.h"
