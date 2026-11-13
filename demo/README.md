# Demo de Cadence

Programas cortos para mostrar el lenguaje de punta a punta: cada uno se
compila, se ejecuta y deja sus archivos MIDI y LilyPond para escucharlos o
leerlos. No forman parte de la suite de tests.

## Cómo correrla

Desde la raíz del repositorio, con Docker en ejecución:

```bash
docker compose run --rm compiler bash -lc 'bash demo/demo.sh'                     # todos los programas
docker compose run --rm compiler bash -lc 'bash demo/demo.sh 03-coral-a-mano.cad'  # uno solo
```

Cada programa escribe en `demo/salida/<programa>/`. El script vacía esa
carpeta antes de correrlo porque el compilador nunca sobrescribe un archivo
existente. `salida/` no se versiona.

## Programas

| Programa | Qué muestra | Salida |
|----------|-------------|--------|
| `01-coro.cad` | Progresión I–vi–IV–V armonizada a cuatro voces con `voice`, transposición, arpegio y `for` | `coro.mid`, `coro.ly`, `en-re.mid`, `arpegio.mid` |
| `02-modos.cad` | La escala de Re en modo mayor, dórico, frigio y lidio, cada una con otro instrumento | Un `.mid` por modo y `re-frigio.ly` |
| `03-coral-a-mano.cad` | Cadencia I–IV–V–I escrita voz por voz que pasa `check` con todas las reglas | `coral.mid`, `coral.ly` |
| `04-modulacion.cad` | Un procedimiento `cadencia(key)` que recorre Do mayor, Sol mayor, Fa mayor y La menor | `modulacion.mid` |
| `05-arpegios.cad` | Acordes de séptima y novena por grado, arpegiados en semicorcheas | `arpegios.mid`, `arpegios.ly` |
| `06-error-strict.cad` | Error: un `G#` en Do mayor estricto | `chord contains a note outside the strict key` |
| `07-error-paralelas.cad` | Error: quintas paralelas entre soprano y bajo | `parallel fifths at chord 2`, sin archivos |

## Escuchar y ver las salidas

Ver [Abrir las salidas](../doc/stage-iii/salida.md#abrir-las-salidas). En
resumen: los `.mid` se abren con MuseScore (libre y gratuito), que los
reproduce, los muestra como partitura y los exporta a MP3; los `.ly` se
convierten en PDF con LilyPond.
