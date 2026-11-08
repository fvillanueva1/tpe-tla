# Salida de Cadence (`export`)

`src/main/c/backend/output/` separa la exportación en dos pasos:

1. El runtime convierte el valor exportado en una **partitura**: una lista de
   eventos de nota (`OutputScore`).
2. Un **escritor** convierte la partitura en un archivo MIDI o LilyPond. Los
   escritores no conocen el lenguaje; solo escriben eventos.

Todo está declarado en `Output.h` (contrato 3 entre Felipe y Eduardo).

## Eventos

- `OutputEvent`: una nota (`MusicNote`, con su grafía para LilyPond), el tick
  de inicio, la duración en ticks y el canal.
- Una negra dura `OUTPUT_TICKS_PER_QUARTER` = 480 ticks, el mismo valor que
  tiene el encabezado del MIDI de escalas. Una redonda dura 1920 y una
  semicorchea 120 (`outputDurationTicks`).
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

Cada función agrega el valor a partir de `end`, así varios valores quedan uno
después del otro:

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

## Escritores

`outputWriteMidi` y `outputWriteLilyPond` escriben primero a un archivo
temporal junto al destino y lo renombran solo cuando todo se escribió, así un
error nunca deja un archivo a medias. Nunca sobrescriben: si el destino existe
devuelven `OUTPUT_FILE_EXISTS`. Por ahora devuelven `OUTPUT_NOT_IMPLEMENTED`;
se implementan en los commits siguientes. Mientras tanto la exportación de
escalas sigue en `exportLiteralScale` (`Generator.c`).

## Errores

`OutputStatus`: `OUTPUT_OK`, `OUTPUT_INVALID_ARGUMENT`, `OUTPUT_NO_MEMORY`,
`OUTPUT_FILE_EXISTS`, `OUTPUT_WRITE_ERROR` y `OUTPUT_NOT_IMPLEMENTED`. El
runtime decide el mensaje y el código de salida (1 para errores de escritura,
2 para memoria).

## Tests

`src/test/c/output-score.c` arma partituras con eventos conocidos y comprueba
alturas, inicios, duraciones y canales de notas, escalas, acordes,
progresiones y voicings; que un fallo a mitad de un valor deje la partitura
intacta (también sin memoria, con `--wrap=realloc`) y los límites de cada
campo. Se compila con AddressSanitizer.
