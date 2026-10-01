# Tokens y nodos del AST — Stage II

Nombres acordados para el lexer, el parser y el AST. Cada `%token` está
declarado en `BisonGrammar.y`. No se cambia ningún nombre sin avisar al equipo
y actualizar este documento.

Convención: `SCREAMING_SNAKE_CASE` para terminales y `camelCase` para
no-terminales (ver el documento de análisis sintáctico de la cátedra).

## Tokens

Los tokens con valor semántico `string` llevan una copia del lexema; el que
consume el valor es responsable de liberarlo (hay un `%destructor`).

### Literales

| Token | Valor | Lexema |
|-------|-------|--------|
| `INTEGER` | `integer` | `0`, `120` |
| `NOTE` | `string` | `C4`, `F#3`, `Bb5`, `C` (sin octava) |
| `DEGREE` | `string` | `I`, `V`, `ii`, `viidim` |
| `INTERVAL` | `string` | `P1`, `m3`, `M6`, `aug4`, `P8` |
| `DURATION` | `string` | `whole`, `half`, `quarter`, `eighth`, `sixteenth` |
| `ID` | `string` | nombres de variables, procedimientos e instrumentos |
| `STRING_TEXT` | `string` | fragmento de texto dentro de un string |

### Tipos

`BOOLEAN_TYPE`, `CHORD_TYPE`, `INTEGER_TYPE`, `INTERVAL_TYPE`, `KEY_TYPE`,
`NOTE_TYPE`, `PROGRESSION_TYPE`, `SCALE_TYPE`, `STRING_TYPE`, `VOICING_TYPE`.

`scale` es un solo token (`SCALE_TYPE`) en `scale s = ...` y en
`E4 in scale of K`; la gramática distingue el uso.

### Modos y relaciones tonales

`MAJOR`, `MINOR`, `DORIAN`, `PHRYGIAN`, `LYDIAN`, `MIXOLYDIAN`, `LOCRIAN`,
`STRICT`, `RELATIVE`, `DOMINANT`, `SUBDOMINANT`, `DIMINISHED`.

`relative minor` son dos tokens: `RELATIVE` y `MINOR`. `DIMINISHED` es la
calidad de acorde que aparece en `actual.quality != diminished`.

### Armonía, voces y salida

| Grupo | Tokens |
|-------|--------|
| Acordes | `TRIAD`, `SEVENTH`, `NINTH`, `INVERT`, `BY`, `ARPEGGIATE`, `MODULATE` |
| Voces | `VOICE`, `SATB`, `CHECK`, `PARALLEL_FIFTHS`, `PARALLEL_OCTAVES`, `VOICE_CROSSING` |
| Salida | `EXPORT`, `MIDI`, `SHEET`, `BPM`, `LOG` |

### Control y procedimientos

`IF`, `ELSE`, `FOR`, `WHILE`, `DEFINE`, `RETURN`, `TRUE`, `FALSE`.

### Palabras compartidas

`AS`, `AT`, `IN`, `OF`, `ON`, `TO`, `WITH`. Cada una sirve a varias
construcciones (`TO` en `for`, `modulate` y `export`; `IN` en progresiones y en
pertenencia).

### Operadores

| Token | Lexema | Token | Lexema |
|-------|--------|-------|--------|
| `ADD` | `+` | `EQUAL` | `==` |
| `SUB` | `-` | `NOT_EQUAL` | `!=` |
| `MUL` | `*` | `LESS` | `<` |
| `DIV` | `/` | `LESS_EQUAL` | `<=` |
| `ASSIGN` | `=` | `GREATER` | `>` |
| `AND` | `and` | `GREATER_EQUAL` | `>=` |
| `OR` | `or` | `NOT` | `not` |

### Puntuación

`OPEN_PARENTHESIS`, `CLOSE_PARENTHESIS`, `OPEN_BRACE`, `CLOSE_BRACE`,
`OPEN_BRACKET`, `CLOSE_BRACKET`, `COMMA`, `SEMICOLON`, `DOT`, `ARROW` (`->`).

### Strings con interpolación

Un string como `"Grado {grado} descartado"` produce esta secuencia:

```
OPEN_STRING STRING_TEXT OPEN_INTERPOLATION ID CLOSE_INTERPOLATION STRING_TEXT CLOSE_STRING
```

Entre llaves solo puede haber un nombre de variable. Un string sin
interpolación es `OPEN_STRING STRING_TEXT CLOSE_STRING`.

### Internos

`OPEN_COMMENT`, `CLOSE_COMMENT` (solo se loguean), `IGNORED` y `UNKNOWN`
(error léxico).

## Del proyecto base

El lexer base trata `{ ... }` como la importación de un archivo
(contexto `IMPORT_EXPRESSION`). En Cadence las llaves delimitan bloques,
acordes (`{C4, E4, G4}`) y la interpolación, así que esa regla debe eliminarse
del lexer.

## Nodos del AST

Los nodos se agregan junto con la regla de gramática que los usa. Cada uno
tiene su función `destroy<Nodo>` en `AbstractSyntaxTree.c`.

Existen:

| Nodo | Contenido |
|------|-----------|
| `Program` | La lista de sentencias |
| `StatementList` | Una sentencia y la lista siguiente (no vacía, en orden) |
| `Statement` | Declaración (`StatementType` `DECLARATION`, con `DataType` y si es vector) o asignación (`ASSIGNMENT`) |
| `Expression` | Literal (entero, booleano, nota, intervalo, duración, grado, tonalidad), variable, operación binaria, negación, escala/relación tonal de una tonalidad, acorde (por grado o por notas), lista entre corchetes, `in`, acceso por índice, acceso a una propiedad, inversión, arpegio o modulación; el tipo es `ExpressionType` |
| `ExpressionList` | Una expresión y la lista siguiente (no vacía, en orden) |
| `Mode` | Enumerado con los modos (`MODE_MAJOR`, `MODE_MINOR`, ...) |
| `DataType` | Enumerado con los tipos del lenguaje (`TYPE_CHORD`, `TYPE_NOTE`, ...) |

Se agregarán con estos nombres:

| Grupo | Nodos |
|-------|-------|
| Sentencias | `If`, `For`, `While`, `Return`, `Log`, `Export`, `Check`, `ProcedureDefinition`, `ProcedureCall` |
| Expresiones | llamada |
| Dominio | `Voice` |
