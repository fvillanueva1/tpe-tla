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
| `Chord.c` | Grados, acordes por grado y por notas, fundamental, calidad y clases |
| `Property.c` | Propiedades, nombres para `log` y pertenencia (`in`, `strict`) |
| `Progression.c` | Progresiones por grados y por acordes, `+` y repetición |
| `Transform.c` | Transposición de acordes, progresiones y tonalidades, inversión, arpegio y modulación |
| `Voicing.c` | `voice` con regla de omisión, voicing explícito y `check` sobre un voicing |
| `MusicValidation.c` | Escalas, pertenencia a tonalidad, tesitura, cruce, paralelas y `check` |
| `VoiceLeading.c` | Realización SATB por programación dinámica |

`MusicValidation.c` y `VoiceLeading.c` vienen de `backend/semantic-analysis/`,
donde se escribieron para la semántica; se movieron sin cambiar su comportamiento. Su
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
  Un acorde construido por grado recuerda el grado (`size` distinto de 0 y
  `degree` de 0 a 6) y su tonalidad (`hasKey`, `key`); así `modulate` puede
  reconstruirlo sobre el mismo grado de otra tonalidad. Un acorde escrito por
  notas tiene `size` 0 y `hasKey` falso.
- **`MusicProgression`:** arreglo dinámico de `MusicStep` (acorde y duración).
- **`MusicVoicing`:** `int[n][4]` en orden soprano, contralto, tenor y bajo, y
  una duración por acorde.
- **`MusicDuration`:** `whole` a `sixteenth`. El runtime conserva su propio
  `Duration` (`Value.h`) con los mismos valores en el mismo orden y lo
  convierte con un cast al llamar a la biblioteca.

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
la semántica conservan sus códigos propios (`MusicVoiceResult` y
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

## Acordes

- **Grados:** `musicParseDegree` lee `I`–`VII`, `i`–`vii` y las minúsculas
  con `dim` y devuelve 0–6. La mayúscula, la minúscula y `dim` son solo para
  leer: la calidad sale siempre de la tonalidad, así que `ii`, `II` e `iidim`
  son el mismo grado (en C mayor los tres dan D–F–A, menor). Así lo tenía
  también la semántica.
- **Por grado:** `musicDegreeChord(tonalidad, grado, tamaño, &acorde)` apila
  terceras de la escala desde el grado: `MUSIC_TRIAD` (3 notas),
  `MUSIC_SEVENTH` (4) o `MUSIC_NINTH` (5). Las notas suben desde la nota de la
  escala (octava 4 o 5) y pasan a la octava siguiente al superar el séptimo
  grado: `seventh on V of C major` = G4 B4 D5 F5, `ninth on I of C major` =
  C4 E4 G4 B4 D5. La fundamental es la primera nota. Un grado fuera de 0–6
  devuelve `MUSIC_OUT_OF_RANGE`; el lenguaje cuenta los grados enteros de 1 a
  7, así que el que llama resta 1.
- **Por notas:** `musicChordFromNotes` conserva el orden y las notas
  repetidas. Vacío es `MUSIC_INVALID_ARGUMENT` y más de 16 notas,
  `MUSIC_TOO_MANY_NOTES`. Como el acorde puede estar invertido, la
  fundamental se busca: es la nota más grave que tiene arriba (en cualquier
  octava) una tercera y una quinta justa; si ninguna la tiene, la más grave
  con tercera y alguna quinta (disminuida o aumentada); si tampoco, la nota
  más grave. `{E4, G4, C5}` tiene fundamental C5; `{D4, F4, B4}`, B4.
- **Calidad** (`c.quality`): la de la tríada sobre la fundamental: mayor
  (3M + 5J), menor (3m + 5J), disminuida (3m + 5d), aumentada (3M + 5A) u
  otra. Un acorde de séptima o novena tiene la calidad de su tríada.
- `musicChordMask` da las clases de altura como máscara de 12 bits, la entrada
  de `musicRealizeSATB`, y `musicChordClassCount` cuántas clases distintas hay.

La semántica arma las máscaras de los acordes por grado conocidos con estas
funciones, en lugar de su copia propia del apilado de terceras.

## Propiedades

| Propiedad | Función | Resultado |
|-----------|---------|-----------|
| `n.octave` | `musicNoteOctave` | Octava escrita (`Cb5` → 5) |
| `c.root` | `musicChordRoot` | La fundamental (ver acordes) |
| `c.quality` | `musicChordQuality` | Calidad de la tríada; `other` si el acorde está vacío |
| `k.tonic` | `musicKeyTonic` | La tónica en octava 4, la misma de `scale of k` (`Bb` → Bb4) |
| `k.mode` | `musicKeyMode` | El modo |
| `p.length` | campo `count` | Cantidad de acordes de la progresión |

`musicModeName`, `musicQualityName` y `musicDurationName` dan el texto que
imprime `log` (`major`, `diminished`, `eighth`); devuelven `NULL` para un
valor desconocido.

## Pertenencia y `strict`

La pertenencia se decide por clase de altura, igual que `==` compara alturas:

- `musicNoteInScale` (`n in scale of k`): `E7` está en la escala de C mayor
  aunque la escala esté escrita en octava 4, y `Fb4` cuenta como `E4`.
- `musicNoteInChord` (`n in c`): `C6` está en `{C4, E4, G4}`.
- `musicNoteInKey` y `musicChordInKey` son la comprobación de `strict` para
  valores calculados. Un acorde está en la tonalidad si todas sus notas lo
  están; un acorde vacío no. Los acordes por grado siempre pertenecen a su
  tonalidad (el menor es el natural: el V de A menor es E–G–B).

La semántica valida `strict` sobre notas literales con `musicNoteInKey`.

## Progresiones

Cada función arma una progresión nueva en `result`, que libera el que llama, y
la deja vacía si falla. `result` tiene que ser una variable distinta de las
entradas y lo que tuviera antes no se libera: para `p = p + q` el runtime arma
en un temporal, destruye `p` y lo reemplaza.

| Función | Construcción | Errores |
|---------|--------------|---------|
| `musicDegreeProgression` | `[I, V, I] in k`: una tríada por grado (0–6), una negra cada una | `MUSIC_OUT_OF_RANGE` por un grado inválido |
| `musicChordProgression` | `[c1, c2]`: cada acorde una negra | Acorde vacío |
| `musicConcatenate` | `p1 + p2` | Memoria |
| `musicAppendChord` | `p + c`: el acorde se agrega con una negra | Acorde vacío, memoria |
| `musicRepeat` | `p * n` | `n < 0` es `MUSIC_OUT_OF_RANGE`; `n = 0` da una progresión vacía |

Los errores de memoria devuelven `MUSIC_NO_MEMORY`.

## Transformaciones

- **Transposición** (`c + M3`, `p - m2`): `musicTransposeChord` y
  `musicTransposeProgression` mueven cada nota con su grafía. Un acorde por
  grado conserva el grado y su tonalidad se mueve con él (`V` de C mayor + P5
  es `V` de G mayor). `musicTransposeKey` mueve la tónica de una tonalidad.
  Una nota fuera de C0–B8 da `MUSIC_OUT_OF_RANGE`.
- **Inversión** (`invert c by n`): `n` veces, la nota más grave sube las
  octavas necesarias para quedar por encima de la más aguda. El resultado se
  escribe de la nota más grave a la más aguda: `{C4, E4, G4}` por 1 da
  `{E4, G4, C5}`, por 2 `{G4, C5, E5}` y por 3 `{C5, E5, G5}`. `n` va de 0 a la
  cantidad de notas, el mismo rango que valida la semántica con literales;
  fuera de eso, o si una nota sale de C0–B8, `MUSIC_OUT_OF_RANGE`. La
  fundamental y la calidad no cambian; el acorde olvida el grado (ya no es la
  tríada apilada) pero conserva la tonalidad.
- **Arpegio** (`arpeggiate c as d`): una progresión con las notas de la más
  grave a la más aguda, una por paso, cada una con la duración `d`.
- **Modulación** (`modulate p to k`): conserva la función armónica.
  - Un acorde por grado se reconstruye sobre el mismo grado de `k`:
    `[I, V]` de C mayor a A menor da `[i, v]` de A menor (A–C–E, E–G–B).
  - Un acorde escrito por notas se transporta la distancia entre la tónica de
    su tonalidad y la de `k`, en la dirección más cercana (a lo sumo un
    tritono): de C mayor a G mayor, `{C4, E4, G4}` baja una cuarta a
    `{G3, B3, D4}`. Si el acorde no tiene tonalidad, usa la del primer acorde de
    la progresión que la tenga.
  - Si ningún acorde tiene tonalidad (por ejemplo `[{C4, E4, G4}]`), devuelve
    `MUSIC_MISSING_KEY`: no hay de dónde modular.
  - Las duraciones no cambian.
- `musicIntervalBetween(desde, hasta, ...)` da el intervalo con grafía que
  lleva una nota a otra. Lo usa la modulación.

## SATB y `check`

- **`voice p as SATB`** (`musicVoiceProgression`): reparte cada acorde en
  cuatro voces con `musicRealizeSATB` (tesituras fijas, sin cruces,
  moviendo las voces lo menos posible). **Regla de omisión:** un acorde con más
  de cuatro clases de altura pierde primero la quinta (justa, disminuida o
  aumentada) y después la séptima. Una novena queda con fundamental, tercera,
  séptima y novena. Si aun así tiene más de cuatro clases (un cluster),
  `MUSIC_TOO_MANY_NOTES`. Si un acorde no entra en las tesituras,
  `MUSIC_NO_REALIZATION`. Las duraciones de la progresión pasan al voicing.
- **Voicing explícito** (`musicExplicitVoicing`): cada acorde da soprano,
  contralto, tenor y bajo en ese orden, sin redistribuir, una negra cada uno.
  Un acorde sin exactamente cuatro notas es `MUSIC_NOT_FOUR_VOICES`.
- **`check v for ...`** (`musicCheckVoicingValue`): `musicCheckVoicing` sobre
  un voicing. Siempre comprueba la tesitura y además las reglas pedidas;
  devuelve la primera violación y el índice del acorde.

La semántica verifica en compilación los voicings explícitos de notas
literales (directos o en una variable inmutable) con estas funciones. Así los
casos 58 (cruce) y 59 (tesitura) pasaron de `pending/` a `reject/`.

## Relación con el AST

`MusicMode` y `MusicRule` repiten los valores de `Mode` y `CheckRule` del AST.
La semántica y el generador convierten con un cast, y `Semantic.c` tiene
`_Static_assert` que impiden compilar si los dos enums dejan de coincidir.

## Tests

- `src/test/c/music-validation.c` y `music-voice-oom.c`: los que venían de la semántica, ahora
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
- `src/test/c/music-chord.c`: lectura de grados, tríadas, séptimas y novenas
  en varios modos con su calidad, acordes por notas invertidos, repetidos o
  incompletos con su fundamental, máscaras y los límites (grado, tamaño,
  cantidad de notas).
- `src/test/c/music-property.c`: cada propiedad (también con acordes
  invertidos y vacíos), los nombres para `log`, la pertenencia a escala y
  acorde por clase de altura y `strict` sobre notas y acordes calculados.
- `src/test/c/music-progression.c`: progresiones, concatenación, repetición,
  transposición, inversión, arpegio, modulación (por grado, por notas y sin
  tonalidad), `voice` con su regla de omisión, voicings explícitos, `check` y
  fallos de memoria (`--wrap=calloc` y `--wrap=malloc`).

`test.sh` compila todos los tests de la biblioteca con `$MUSIC_SOURCES`.
