# Decisiones de diseño — Stage II

Decisiones tomadas después del feedback del Stage I y al planificar el
frontend. Si alguna cambia, se actualiza este documento.

## Lenguaje

- `=` solo asigna; `==` compara.
- Los operadores relacionales se aceptan también entre enteros.
- `log` usa interpolación por nombre: `log "Se descartó el grado {grado}";`.
  Solo se admiten nombres de variable entre llaves, no expresiones. El lexer
  emite un token por cada fragmento de texto y por cada `{variable}`; en el AST
  el mensaje es una lista de partes (texto o nombre de variable). Que la
  variable exista se valida en el Stage III.
- `export` admite un instrumento opcional: `... at 90 bpm with piano;`. El
  instrumento es un identificador común; que sea un instrumento válido se
  valida en el Stage III contra una tabla de instrumentos MIDI.
- La cuarta aumentada se escribe `aug4`, porque `A4` es la nota La de la
  octava 4 y el lexer no puede distinguirlos.
- Los grados disminuidos llevan el sufijo ASCII `dim` (`iidim`, `viidim`) en
  lugar de `°`, que no es ASCII.
- Vectores: `chord[] acordes = [triad on I of K, triad on V of K];`, con acceso
  `acordes[0]`.
- Tesitura: rangos fijos por defecto para SATB. No hay sintaxis para
  declararla.

## Precedencia

De menor a mayor: `or`, `and`, `not`, relacionales (`== != < <= > >=`, y `in`
más adelante), `+ -`, `* /`. `not` queda por debajo de los relacionales, así
que `not a == b` es `not (a == b)`. Los relacionales no son asociativos:
`a < b < c` se rechaza. Un programa vacío se rechaza.

## Tonalidades

- Tonalidad: una nota sin octava y un modo (`major`, `minor`, `dorian`,
  `phrygian`, `lydian`, `mixolydian`, `locrian`), con `strict` opcional al
  final: `key k = G major strict;`.
- Escala de una tonalidad: `scale of k`.
- Relaciones tonales, que producen una tonalidad: `relative minor of k`,
  `dominant of k`, `subdominant of k`. Se pueden anidar.
- `of` liga más fuerte que cualquier operador: `scale of k == j` es
  `(scale of k) == j`.
- Acceder a un grado de una escala usa el acceso por índice de los vectores.

## Acordes

- Por grado: `triad on V of k`, `seventh on ii of k`, `ninth on I of k`. El
  grado puede ser un grado literal (`V`, `viidim`) o cualquier expresión, por
  ejemplo la variable de un `for` (`triad on grado of k`). Que sea un grado
  válido lo decide el análisis semántico.
- Por notas explícitas: `{C4, E4, G4}`, con al menos una nota. Los elementos
  pueden ser cualquier expresión.
- `of` liga más fuerte que cualquier operador: `triad on V of k == j` es
  `(triad on V of k) == j`.

## Progresiones, vectores, índices y propiedades

- Lista entre corchetes: `[c1, c2]`, `[]`. Es una lista de expresiones; que sea
  una progresión de acordes o un vector lo decide el tipo declarado.
- Progresión de grados sobre una tonalidad: `[I, V, VI, IV] in k`.
- `in` es un único operador binario `expresión in expresión`. Sirve para la
  progresión de grados sobre una tonalidad y para la pertenencia
  (`E4 in scale of k`, `E4 in c`); el análisis semántico los distingue por el
  tipo del lado izquierdo. No es asociativo (`a in b in c` se rechaza) y está
  en el nivel de los relacionales.
- Vectores: el tipo lleva `[]` (`chord[] cs = [...]`, `integer[] xs = [1, 2]`)
  y se accede por índice: `cs[0]`. Solo se declaran vectores de una dimensión.
  No se puede asignar a un elemento (`cs[0] = c;` se rechaza).
- Propiedades: `c.root`, `c.quality`, `n.octave`, `k.tonic`, `k.mode`,
  `p.length`. El nombre de la propiedad es un identificador; que exista lo
  valida el Stage III. La calidad `diminished` es un literal.
- El índice y la propiedad ligan más fuerte que `of`: `scale of ks[0]` es
  `scale of (ks[0])`. Para indexar una escala hay que usar paréntesis:
  `(scale of k)[3]`.

## Transformaciones

- Inversión: `invert c by 1`.
- Arpegio: `arpeggiate c as eighth`. Las duraciones son literales de
  expresión, así que también puede ser una variable (`arpeggiate c as d`).
- Modulación: `modulate p to G major`, `modulate p to dominant of k`,
  `modulate p to relative minor of k`. El destino es cualquier expresión de
  tonalidad.
- `by`, `as` y `to` tienen la precedencia más baja: lo que sigue es una
  expresión completa, así que `invert c by n + 1` es `invert c by (n + 1)`.
  Para comparar el resultado hay que usar paréntesis:
  `(invert c by 1) == d`. Estas formas no se encadenan sin paréntesis
  (`invert c by 1 by 2` se rechaza).

## Control de flujo y procedimientos

- Los bloques llevan llaves siempre, también con una sola sentencia. Así no
  hay ambigüedad con `else` (*dangling else*). Un bloque puede estar vacío:
  `{ }`.
- `if (condición) { ... }`, con `else { ... }` o `else if (...) { ... }`
  opcionales. La condición va entre paréntesis.
- `while (condición) { ... }`, con la condición entre paréntesis.
- `for variable = desde to hasta { ... }`. La variable no lleva tipo; que sea
  un entero y que no choque con otra se valida en el Stage III. Como los grados
  en minúscula son tokens, la variable no puede llamarse `i` ni `v`.
- Procedimientos: `define nombre(tipo a, tipo[] b) -> tipo { ... }`. Los
  parámetros pueden faltar (`define f() { ... }`) y el tipo de retorno es
  opcional, con `[]` si devuelve un vector (`-> chord[]`).
- `return expresión;` o `return;`.
- Una llamada es una expresión (`progression p = cadencia(k);`) o una sentencia
  (`cadencia(k);`). Los argumentos pueden faltar.
- `define` y `return` se aceptan en cualquier lugar donde va una sentencia. Que
  un `define` no esté anidado y que `return` esté dentro de un procedimiento se
  valida en el Stage III.

## Lexer

- Las notas sin octava (`C`, `G`, ...) son tokens de nota, así que no pueden
  usarse como nombres de variable.
- Las listas cerradas (notas, grados, intervalos) van acompañadas de reglas de
  error que devuelven `UNKNOWN` para las formas inválidas (`H4`, `C#9`, `VIII`,
  `P3`). Van después de las válidas, porque a igual longitud gana la primera.
- Literales del dominio y sus tokens (todos llevan el lexema como `string`):
  - `NOTE`: `[A-G]`, alteración opcional `#` o `b`, octava opcional `0`–`8`
    (`C`, `C#4`, `Bb`).
  - `INTERVAL`: `P1 P4 P5 P8 m2 m3 m6 m7 M2 M3 M6 M7 d5 aug4`.
  - `DEGREE`: `I`–`VII` en mayúscula, `i`–`vii` en minúscula, y las minúsculas
    con sufijo `dim` (`iidim`).
  - `DURATION`: `whole half quarter eighth sixteenth`.
- Formas reservadas que el lexer rechaza aunque parezcan identificadores:
  - Una letra `A`–`H`, `M` o `P`, alteración opcional y dígitos fuera de las
    formas válidas (`H4`, `C9`, `C10`, `P3`, `M4`).
  - `m` o `d` seguidos de dígitos, y `aug` seguido de dígitos, fuera de los
    intervalos válidos (`m5`, `d4`, `aug5`).
  - Secuencias de `I`, `V`, `X` o de `i`, `v` que no son un grado válido
    (`VIII`, `X`, `viii`, `vv`, `Vdim`).
- Los grados en minúscula (`i`, `ii`, `v`, `vi`, ...) son tokens de grado, así
  que no pueden usarse como nombres de variable.
- Strings (secuencia de tokens en `tokens.md`): las comillas emiten
  `OPEN_STRING` y `CLOSE_STRING`, y `{nombre}` emite `OPEN_INTERPOLATION`,
  `ID` y `CLOSE_INTERPOLATION`. El lexer usa dos contextos, `STRING` e
  `INTERPOLATION`, uno dentro del otro.
  - `STRING_TEXT` es texto literal o un escape (`\"`, `\\`, `\{`, `\}`, `\n`,
    `\t`). El valor es el lexema tal cual; los escapes se decodifican en el
    Stage III.
  - Dentro de las llaves todo nombre es `ID`, aunque sea una palabra clave o
    un literal fuera del string (`{key}`, `{v}`).
- Son errores léxicos dentro de un string: una `}` suelta, un escape
  desconocido (`\q`) y un salto de línea. Dentro de las llaves, cualquier cosa
  que no sea un nombre (`{1}`, `{a+b}`, `{ a }`, `{a{b}}`). `{}` pasa el lexer
  y lo rechaza la gramática. Un string o una interpolación sin cerrar fallan
  al llegar al final del archivo.

## Proceso

- `BisonGrammar.y` declara `%expect 0` desde el primer commit: ningún commit
  entra con conflictos de Bison.
- Los tests de rechazo que dependen del análisis semántico viven en
  `src/test/c/pending/`, que `test.sh` no ejecuta. Se mueven a `reject/` en el
  Stage III. Así el CI se mantiene verde.
- Los nombres de tokens y de nodos del AST se acuerdan al inicio y se
  registran en un documento junto con los `%token`.
