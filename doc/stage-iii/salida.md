# Salida de Cadence (`export`)

`src/main/c/backend/output/` separa la exportación en dos pasos:

1. El runtime convierte el valor exportado en una **partitura**: una lista de
   eventos de nota (`OutputScore`).
2. Un **escritor** convierte la partitura en un archivo MIDI o LilyPond. Los
   escritores no conocen el lenguaje; solo escriben eventos.

Todo está declarado en `Output.h` (contrato 3 entre Felipe y Eduardo).

| Archivo | Contenido |
|---------|-----------|
| `Output.h` | Contrato: partitura, conversión de valores, codificadores y escritura |
| `Score.c` | Partitura y conversión de nota, escala, acorde, progresión y voicing |
| `Midi.c` | Codificador MIDI |
| `LilyPond.c` | Codificador LilyPond |
| `File.c` | Escritura segura y por lotes |
| `Buffer.c`, `OutputInternal.h` | Buffer en memoria y validación compartidos (no son parte del contrato) |

## Eventos

- `OutputEvent`: una nota (`MusicNote`, con su grafía para LilyPond), el tick
  de inicio, la duración en ticks y el canal.
- Una negra dura `OUTPUT_TICKS_PER_QUARTER` = 480 ticks. Una redonda dura
  1920 y una semicorchea 120 (`outputDurationTicks`).
- Canales: una línea simple usa el canal 0 (`OUTPUT_MELODY_CHANNEL`); un
  voicing usa un canal por voz, de soprano (0) a bajo (3).
- `OutputScore` guarda los eventos, el tempo (1 a 60000000 BPM), el
  instrumento (programa General MIDI 0–127) y `end`, el tick donde empieza el
  próximo valor. Si `export` no indica tempo o instrumento se usan 120 BPM y
  `piano` (`OUTPUT_DEFAULT_TEMPO`, `OUTPUT_DEFAULT_INSTRUMENT`).

## Duración por defecto

Una nota, un acorde o un acorde de una progresión duran una negra
(`MUSIC_DEFAULT_DURATION` en `Music.h`), igual que cada nota de la escala que
ya exportaba `exportLiteralScale`. Registrado en `doc/stage-ii/decisiones.md`.

## Del valor a la partitura

Cada función agrega el valor a partir de `end`:

| Función | Valor | Eventos |
|---------|-------|---------|
| `outputAddNote` | `note` | Una negra |
| `outputAddScale` | `scale` | Las siete notas seguidas, una negra cada una |
| `outputAddChord` | `chord` | Todas las notas a la vez, una negra |
| `outputAddProgression` | `progression` | Cada acorde con su duración, uno después del otro |
| `outputAddVoicing` | `voicing` | Cada acorde SATB con su duración, la voz *i* en el canal *i* |

Un voicing solo guarda alturas, así que sus notas se deletrean con sostenidos
(`musicNoteFromPitch`).

Si algo falla (memoria, una duración inválida, una altura fuera de rango), la
función devuelve el error y deja la partitura como estaba: no quedan acordes a
medias. `outputScoreAdd` agrega un evento suelto y rechaza duración cero,
canales fuera de 0–15, alturas fuera de C0–B8 y un final que no entra en 32
bits.

## MIDI

`outputEncodeMidi` escribe un MIDI estándar de **formato 0**: una sola pista
con

1. el tempo (microsegundos por negra, campo de 24 bits: hace falta al menos
   4 BPM, el mismo mínimo que valida la semántica);
2. un cambio de programa (instrumento) por cada canal usado;
3. los eventos ordenados por tick; en el mismo tick, primero terminan las
   notas que terminan y después empiezan las nuevas. Velocidad 64.

**Voces SATB (punto 5 del feedback del Stage I):** cada voz va en su propio
canal (soprano 0, contralto 1, tenor 2, bajo 3) dentro de la misma pista, con
el mismo instrumento. Se eligió formato 0 porque una sola pista basta para
cuatro canales y porque así la escala exporta exactamente el mismo archivo que
antes (`scale-midi.c` lo comprueba byte a byte).

## LilyPond (`sheet`)

`outputEncodeLilyPond` escribe un `.ly` (versión 2.24) con un pentagrama por
canal usado:

- Para un voicing, cuatro pentagramas con nombre (Soprano, Alto, Tenor, Bass)
  y claves de sol, sol, sol octava baja (`treble_8`) y fa. Para una línea
  simple, un pentagrama sin nombre con clave de sol o de fa según la altura
  promedio.
- Las notas usan su grafía en la nomenclatura por defecto de LilyPond: `cis`,
  `bes`, `fisis`; la octava escrita se marca con `'` y `,` (`c'` es C4). Por
  eso `Cb4` se escribe `ces'`, no `b`.
- Las notas que empiezan juntas forman un acorde (`<c' e' g'>4`), los
  silencios entre eventos son `r` y las duraciones largas se ligan
  (`c'1~ c'4`). Las duraciones tienen que ser múltiplos de una semicorchea.
- El tempo va como `\tempo 4 = N` en el primer pentagrama.

Si dos eventos de un mismo canal se superponen sin empezar y terminar juntos
devuelve `OUTPUT_INVALID_ARGUMENT`; las funciones `outputAdd*` nunca lo
producen.

## Escritura de archivos

`outputWriteFiles(archivos, n, &falló)` escribe **todos los archivos o
ninguno**, que es lo que el runtime necesita al terminar un programa con varios
`export`:

1. Antes de escribir nada comprueba que ningún destino exista ni esté repetido
   en la llamada (`OUTPUT_FILE_EXISTS`).
2. Escribe cada archivo a un temporal junto a su destino (`destino.XXXXXX`, con
   `mkstemp`). Si algo falla, borra todos los temporales.
3. Recién entonces publica cada temporal con su nombre final usando `link()`,
   que falla si el destino apareció mientras tanto, así nunca se sobrescribe.
   En sistemas de archivos sin enlaces duros devuelve `OUTPUT_WRITE_ERROR`:
   no usa `rename()`, que podría reemplazar un destino creado por otro proceso.
   Si falla la publicación de uno, se borran los ya publicados y los
   temporales restantes.

`falló` recibe el índice del archivo que falló. Los archivos quedan con los
permisos normales (`0666` menos la `umask`). `outputWriteMidi` y
`outputWriteLilyPond` escriben un solo archivo con la misma garantía.

El generador ya usa este módulo para la exportación de escalas literales
(`exportLiteralScale`): el archivo que produce es el mismo de antes, pero ahora
se escribe a un temporal y se publica sin sobrescribir.

## Errores

`OutputStatus`: `OUTPUT_OK`, `OUTPUT_INVALID_ARGUMENT`, `OUTPUT_NO_MEMORY`,
`OUTPUT_FILE_EXISTS` y `OUTPUT_WRITE_ERROR`. El runtime decide el mensaje y el
código de salida (1 para errores de escritura, 2 para memoria).

## Tests

- `src/test/c/output-score.c`: partituras con eventos conocidos; alturas,
  inicios, duraciones y canales de notas, escalas, acordes, progresiones y
  voicings; un fallo a mitad de un valor deja la partitura intacta (también
  sin memoria, con `--wrap=realloc`) y los límites de cada campo.
- `src/test/c/output-file.c`: el MIDI de un acorde byte a byte, los canales y
  cambios de programa de un voicing, el tempo mínimo; el LilyPond de una
  escala, de acordes con silencios, ligaduras y grafías, y de un voicing; y la
  escritura: contenido y permisos, no sobrescribir, todo o nada (un destino
  imposible, repetido o existente, un archivo que no se puede codificar) sin
  dejar temporales. También rechaza formatos desconocidos sin liberar memoria
  sin inicializar y simula fallos de `link()` para comprobar la limpieza del
  lote y que un destino creado durante la publicación conserve su contenido.
- `scale-midi.c` y la integración de `test.sh` comprueban que la escala se
  sigue exportando igual y que una segunda exportación al mismo archivo falla
  sin cambiarlo.

Todos se compilan con AddressSanitizer.
