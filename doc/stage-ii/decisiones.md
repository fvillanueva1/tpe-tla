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

## Proceso

- `BisonGrammar.y` declara `%expect 0` desde el primer commit: ningún commit
  entra con conflictos de Bison.
- Los tests de rechazo que dependen del análisis semántico viven en
  `src/test/c/pending/`, que `test.sh` no ejecuta. Se mueven a `reject/` en el
  Stage III. Así el CI se mantiene verde.
- Los nombres de tokens y de nodos del AST se acuerdan al inicio y se
  registran en un documento junto con los `%token`.
