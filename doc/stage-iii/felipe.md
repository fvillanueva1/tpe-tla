# Felipe — Biblioteca musical, salida MIDI/sheet e informe

## Por qué este reparto

El Stage III pide dos fases: análisis semántico y generación. La semántica ya está completa en los PR #13–#19, hechos por Tadeo. Esos PR también incluyen las validaciones musicales (escalas, `strict`, tesitura, cruce y paralelas), la realización SATB de acordes conocidos y el primer MIDI de escalas. Por eso lo que queda se reparte casi todo entre Felipe y Eduardo:
- **Eduardo:** el runtime, que es la otra fase.
- **Felipe:** la biblioteca musical, la salida MIDI y `sheet`, y la coordinación del informe.
- **Tadeo:** revisión, ajustes de semántica al integrar y sus secciones del informe.

## Punto de partida (común a los tres)

- **Hecho:** frontend completo (Stage II) y análisis semántico: nombres, alcances, tipos, procedimientos, `log`, `export`, rangos de valores constantes, `strict` y SATB para valores conocidos. Las funciones musicales que se escribieron para la semántica estaban en `backend/semantic-analysis/MusicValidation.c` y `VoiceLeading.c` (ahora en `backend/music/`): escalas, pertenencia a tonalidad, realización SATB por masas de clases, tesitura, cruce y paralelas.
- **Salida:** el generador solo exporta a MIDI la escala de una tonalidad literal (`exportLiteralScale` en `Generator.c`). Cualquier otro programa devuelve error *"Code generation is not implemented"*. `--check` corre solo frontend y semántica; la suite acepta con `--check` hasta que exista el runtime.
- **Decidido:**
  - Solo se puede llamar a un procedimiento después de su `define` o desde su propio cuerpo.
  - Un `voicing` puede ser una lista explícita de acordes de 4 notas, en orden S, A, T, B.
  - `sheet` se exporta en LilyPond (`.ly`), como en los ejemplos del Stage I.
  - Está documentado en `doc/stage-ii/decisiones.md`.
- **Arquitectura de hecho:** el compilador interpreta el AST y escribe los artefactos (`.mid`, `.ly`). No se genera otro programa.

## Contratos a fijar el día 1, entre los tres (media hora)

1. **Modelo musical** (Felipe, `backend/music/Music.h`): nota = altura MIDI más su grafía, acorde = arreglo de alturas, progresión = acordes más duraciones, voicing = `int[n][4]`.
2. **Valores del runtime** (Eduardo, `backend/runtime/Value.h`): unión con un tipo por cada `DataType` que usa los structs de Felipe, más un entorno de variables.
3. **Eventos de salida** (Felipe, `backend/output/Output.h`): lista de `{pitch, start, duration, channel}` en ticks, más tempo e instrumento. El runtime arma la lista y la salida solo la escribe.

## Tu parte A: el módulo `backend/music/`, en C puro, sin dependencia del AST

Cada función devuelve un resultado o un código de error, sin imprimir nada. Eduardo la llama desde el runtime y decide el mensaje. Para cada operación: un test con un resultado conocido y otro con un límite o error, en el estilo de `src/test/c/music-validation.c`.

### 1. Base (primero: desbloquea a Eduardo)
- [x] Definir `Music.h` (contrato 1). Mover ahí `MusicValidation.c` y `VoiceLeading.c` y ajustar los includes de `Semantic.c`, sin cambiar su comportamiento: la suite tiene que seguir verde.
- [x] Notas y alteraciones, grafía, rango admitido (el mismo que valida la semántica) e intervalos (`aug4`, etc.).
- [x] Transposición de nota con error fuera de rango.

### 2. Tonalidades, escalas y acordes
- [x] Escalas de los siete modos (`musicScale` ya existe; revisar la grafía) y acceso por grado. También las relaciones tonales (`dominant of`, `subdominant of`, `relative minor of`).
- [x] Acordes explícitos y por grado: tríadas, séptimas y novenas, con la calidad según el modo. Grado inválido es error.
- [x] Propiedades que expone el lenguaje (ver `accept/properties.cad`).
- [x] `strict`: pertenencia de notas y acordes a la tonalidad, con valores calculados (`musicInKey` ya existe).

### 3. Progresiones y transformaciones
- [x] Progresiones por grados y por acordes, concatenación (`+`) y repetición.
- [x] Transposición de acorde y progresión, inversión (con su rango), arpegio con duración y modulación (dominante y relativa) preservando la función armónica.
- [x] Acordar con Eduardo la duración por defecto de un acorde en una progresión. Registrarla en `decisiones.md`. (Negra, como la exportación de escalas; avisarle a Eduardo.)

### 4. SATB y `check`
- [x] Generalizar `musicRealizeSATB` para acordes calculados. Definir la regla de omisión para acordes de más de cuatro clases (por ejemplo, omitir la quinta y después la séptima) y documentarla.
- [x] Qué pasa si no hay realización válida: error de runtime con mensaje.
- [x] `check` sobre voicings calculados (`musicCheckVoicing` ya existe).
- [x] **Decidido:** se agrega el voicing explícito, una lista de acordes por notas en orden S, A, T, B: `voicing v = [{C5, G4, E4, C3}, ...];`. Hace falta porque `voice` nunca produce cruces ni notas fuera de tesitura, así que sin esto los casos 58 y 59 del Stage I no se pueden escribir. La gramática ya lo acepta. La semántica ya acepta una lista de acordes como `voicing` (hecho por Tadeo). Vos definís la conversión a `int[n][4]`: cada acorde debe tener exactamente 4 notas y si no, es error. La semántica ya lo chequea en los acordes literales; los calculados quedan para el runtime. `check` y tesitura se aplican sobre esa conversión sin redistribuir.
- [x] Rehacer `pending/58-voice-crossing.cad` y `59-tessitura.cad` con voicings explícitos que provoquen el error que nombran, y moverlos a `reject/`.

## Tu parte B: salida (`backend/output/`)

Ya existe un prototipo: `exportLiteralScale` en `Generator.c` escribe un MIDI válido de una escala. Tomalo como base y probalo con `src/test/c/scale-midi.c`.

### 5. Contrato y MIDI general (temprano: Eduardo lo necesita para `export`)
- [x] Definir `Output.h` (contrato 3) y pasárselo a Eduardo. (Ver `doc/stage-iii/salida.md`.)
- [x] Escritor MIDI con la lista de eventos: notas simultáneas (acordes), secuencias, cuatro canales para SATB, tempo y cambio de instrumento (`semanticInstrumentProgram`).
- [x] Errores de escritura: no sobrescribir, no dejar archivos a medio escribir (escribir a temporal y renombrar), código de salida 1.
- [x] Probarlo con eventos fijos, sin runtime. Extender `scale-midi.c` para comprobar alturas, tiempos y canales de acordes y progresiones. (Los acordes, progresiones y voicings se comprueban en `output-file.c` y `output-score.c`; `scale-midi.c` sigue comprobando la escala.)
- [ ] Cuando el runtime produzca eventos, reemplazar `exportLiteralScale` por este escritor junto con Eduardo. (Hecho a medias: `exportLiteralScale` ya escribe con `outputWriteMidi`; falta que el runtime de Eduardo arme los eventos de cualquier `export` y llame a `outputWriteFiles` al final.)

### 6. `sheet` en LilyPond
- [x] Escribir `.ly` a partir de los mismos eventos, usando la grafía de las notas (por eso conviene que lo hagas vos). Para un voicing, cuatro voces.
- [x] Probar el contenido de un caso chico.

## Tu parte C: informe final (coordinación)

> **Cambio de reparto:** el informe no lo hace Felipe. Su alcance llega hasta la parte B.

- [ ] Armar el documento con las secciones de la consigna. Aplicar el feedback del Stage I: fecha, índice, numeración de páginas, texto justificado, ejemplos en figuras con leyenda y numeración separada de casos.
- [ ] Actualizar los ejemplos del Stage I a la sintaxis actual. El 6.4 usa `v` como variable y no compila porque `v` es un grado reservado; el 6.2 usa `log "... {1}", grado`.
- [ ] Escribir: 1. Introducción, 2. Modelo computacional (2.1 Dominio, 2.2 Lenguaje), la parte musical y de salida de 3.2 Backend, 5. Conclusiones, 6. Referencias y 7. Bibliografía. Integrar lo de Tadeo y Eduardo.
- [ ] Al final: hash completo de `development` y anuncio al QRF.

## Dependencias
- La biblioteca musical y los escritores se prueban sin runtime, con tests en C.
- Eduardo depende de `Music.h` (paso 1) y de `Output.h` (paso 5). Entregá los dos headers temprano aunque falten implementaciones.
