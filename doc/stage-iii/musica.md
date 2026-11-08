# Biblioteca musical de Cadence

`src/main/c/backend/music/` es C puro y no depende del AST. Cada función
devuelve un resultado o un código de error y no imprime nada: el que la llama
(la semántica o el runtime) decide el mensaje.

| Archivo | Contenido |
|---------|-----------|
| `Music.h` | Modelo musical y todas las declaraciones públicas |
| `Music.c` | Memoria de progresiones y voicings (crear, copiar, liberar) |
| `Note.c` | Notas, grafía, intervalos y transposición |
| `Key.c` | Escalas con grafía, acceso por grado y relaciones tonales |
| `MusicValidation.c` | Escalas, pertenencia a tonalidad, tesitura, cruce, paralelas y `check` |
| `VoiceLeading.c` | Realización SATB por programación dinámica |

`MusicValidation.c` y `VoiceLeading.c` vienen de `backend/semantic-analysis/`,
donde los escribió Tadeo; se movieron sin cambiar su comportamiento. Su
descripción detallada sigue en `doc/stage-ii/semantica.md`.

## Modelo (contrato con el runtime)

- **Altura:** número MIDI, C4 = 60. El lenguaje admite de C0 (12) a B8 (119),
  `MUSIC_LOWEST_PITCH` y `MUSIC_HIGHEST_PITCH`.
- **`MusicSpelling`:** cómo se escribe una nota: letra `'A'`–`'G'` y
  alteración de `-2` a `+2` (bemoles negativos). El lenguaje solo escribe
  `-1`, `0` y `+1`; la transposición puede producir dobles alteraciones
  (`E#4 + M3 = G##4`).
- **`MusicNote`:** altura más grafía. `Cb4` y `B3` tienen la misma altura (59)
  pero distinta grafía; la grafía hace falta para LilyPond y para transponer
  sin cambiar el nombre de la nota.
- **`MusicInterval`:** semitonos y pasos diatónicos (`aug4` = 6 / 3,
  `d5` = 6 / 4). Se comparan por semitonos (`aug4 == d5`).
- **`MusicKey`:** tónica sin octava (una grafía), modo y `strict`.
- **`MusicScale`:** las siete notas.
- **`MusicChord`:** hasta `MUSIC_MAX_CHORD_NOTES` (16) notas en el orden en que
  se escribieron o construyeron, el índice de la fundamental (`root`) y la
  calidad (`quality`). Es de tamaño fijo, así que se copia por asignación.
- **`MusicProgression`:** arreglo dinámico de `MusicStep` (acorde y duración).
- **`MusicVoicing`:** `int[n][4]` en orden soprano, contralto, tenor y bajo, y
  una duración por acorde.
- **`MusicDuration`:** `whole` a `sixteenth`. Reemplaza al `Duration` del
  runtime, para que haya un solo enum.

Las progresiones y los voicings son dueños de sus arreglos:
`musicProgressionInit` / `musicVoicingInit` reservan `n` elementos en cero,
`...Copy` hace una copia profunda y `...Destroy` libera y se puede llamar más
de una vez. Si falta memoria devuelven `false` y dejan el valor vacío, sin
memoria reservada.

## Errores

Las operaciones que pueden fallar por su entrada devuelven `MusicStatus`:
`MUSIC_OK`, `MUSIC_INVALID_ARGUMENT` (lexema o dato inválido),
`MUSIC_MISSING_OCTAVE` (una nota sin octava donde hace falta) o
`MUSIC_OUT_OF_RANGE` (la altura queda fuera de C0–B8). Las funciones de
Tadeo conservan sus códigos propios (`MusicVoiceResult` y
`MusicValidationResult`).

## Notas e intervalos

- `musicParseNote("C#4")` da altura y grafía. Una nota sin octava devuelve
  `MUSIC_MISSING_OCTAVE`; `Cb0` (11) y `B#8` (120) devuelven
  `MUSIC_OUT_OF_RANGE`.
- `musicParseSpelling("Bb")` lee una tónica, que se escribe sin octava.
- `musicParseInterval("aug4")` da semitonos y pasos.
- `musicPitchClass` da la clase de 0 a 11 (`Cb` = 11, `B#` = 0).
- `musicNoteFromSpelling(grafía, octava)` arma una nota en una octava escrita:
  así se obtiene la tónica en octava 4 para las escalas (`Cb` da 59, como antes).
- `musicNoteFromPitch` deletrea una altura con sostenidos. Es la grafía por
  defecto de las alturas calculadas sin grafía, como las de la realización SATB.
- `musicNoteOctave` da la octava escrita: `Cb4` tiene altura 59 pero octava 4.
  El rango siempre se comprueba sobre la altura, no sobre la octava escrita,
  así que `Cb9` (= B8) es válida si sale de una transposición.
- `musicTransposeNote(nota, intervalo, bajar, &resultado)` suma o resta los
  semitonos y mueve la letra según los pasos, así la grafía queda correcta:
  `C4 + aug4 = F#4`, `C4 + d5 = Gb4`, `A4 + m3 = C5`. Si hiciera falta una
  alteración triple, deletrea con sostenidos. Un resultado fuera de C0–B8
  devuelve `MUSIC_OUT_OF_RANGE` y no modifica el resultado.
- `musicFormatNote` escribe la nota como `C#4`, `Bb3` o `G##4`, para `log`
  y para los mensajes.

La semántica usa estas funciones para validar notas literales, tónicas,
transposiciones conocidas y `strict`. Antes tenía su propia tabla con C4 = 48
y sumaba 12 en algunos lugares; ahora todo está en MIDI. El generador arma la
tónica de la escala con `musicNoteFromSpelling`.

## Tonalidades y escalas

- `musicKeyScale(tonalidad, &escala)` da las siete notas desde la tónica en
  octava 4, como siempre hizo `export`. Cada grado usa la letra siguiente, así
  que las alteraciones salen de la tonalidad y nunca se cambian por su
  enarmónica: G mayor tiene F#5, F mayor tiene Bb4, Cb mayor va de Cb4 a Bb4 y
  G# mayor tiene F##5. Las alturas son las de `musicScale`.
- `musicScaleNote(&escala, i, &nota)` es el acceso `(scale of k)[i]`, de base
  cero; fuera de 0–6 devuelve `MUSIC_OUT_OF_RANGE`.
- Relaciones tonales. La tonalidad derivada conserva `strict` y se pueden
  anidar (`relative minor of dominant of k`):
  - `musicDominantKey`: la tónica una quinta justa arriba, mismo modo
    (C mayor → G mayor, A menor → E menor).
  - `musicSubdominantKey`: una cuarta justa arriba, mismo modo
    (C mayor → F mayor).
  - `musicRelativeMinorKey`: la tonalidad menor con las mismas notas. En
    mayor es el sexto grado (C mayor → A menor, Eb mayor → C menor). En los
    demás modos es el grado desde el cual los intervalos son los del modo menor
    (D dórico → A menor, B locrio → A menor). Una tonalidad menor es su propia
    relativa.

El generador arma la escala que exporta con `musicKeyScale`.

## Relación con el AST

`MusicMode` y `MusicRule` repiten los valores de `Mode` y `CheckRule` del AST.
La semántica y el generador convierten con un cast, y `Semantic.c` tiene
`_Static_assert` que impiden compilar si los dos enums dejan de coincidir.

## Tests

- `src/test/c/music-validation.c` y `music-voice-oom.c`: los de Tadeo, ahora
  sobre `Music.h`.
- `src/test/c/music-model.c`: creación, copia profunda, liberación repetida y
  fallos de memoria de progresiones y voicings. Se compila con
  AddressSanitizer, así que una fuga en un camino de error hace fallar la
  suite.
- `src/test/c/music-note.c`: lectura de notas, tónicas e intervalos,
  clases de altura, octava escrita, transposición con su grafía (también con
  dobles alteraciones y cruces de octava) y los límites C0 y B8.
- `src/test/c/music-key.c`: escalas de los siete modos con su grafía,
  tonalidades con alteraciones, acceso por grado y sus límites, y las tres
  relaciones tonales (también anidadas y conservando `strict`).

`test.sh` compila todos los tests de la biblioteca con `$MUSIC_SOURCES`.
