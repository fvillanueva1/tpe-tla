# Biblioteca musical de Cadence

`src/main/c/backend/music/` es C puro y no depende del AST. Cada función
devuelve un resultado o un código de error y no imprime nada: el que la llama
(la semántica o el runtime) decide el mensaje.

| Archivo | Contenido |
|---------|-----------|
| `Music.h` | Modelo musical y todas las declaraciones públicas |
| `Music.c` | Memoria de progresiones y voicings (crear, copiar, liberar) |
| `MusicValidation.c` | Escalas, pertenencia a tonalidad, tesitura, cruce, paralelas y `check` |
| `VoiceLeading.c` | Realización SATB por programación dinámica |

`MusicValidation.c` y `VoiceLeading.c` vienen de `backend/semantic-analysis/`,
donde los escribió Tadeo; se movieron sin cambiar su comportamiento. Su
descripción detallada sigue en `doc/stage-ii/semantica.md`.

## Modelo (contrato con el runtime)

- **Altura:** número MIDI, C4 = 60. El lenguaje admite de C0 (12) a B8 (119),
  `MUSIC_LOWEST_PITCH` y `MUSIC_HIGHEST_PITCH`.
- **`MusicSpelling`:** cómo se escribe una nota: letra `'A'`–`'G'` y
  alteración `-1` (bemol), `0` o `+1` (sostenido).
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

`test.sh` compila todos los tests de la biblioteca con `$MUSIC_SOURCES`.
