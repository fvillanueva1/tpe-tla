# Seguimiento del feedback del Stage I

Cada punto del feedback de la cátedra, en el orden en que llegó, con su
estado y quién lo cierra. Los puntos de formato se aplican al informe final
del Stage III: el PDF del Stage I no se reentrega.

| # | Punto | Estado | Dónde / quién |
|---|-------|--------|---------------|
| 1 | La licencia solo nombra a Felipe | Resuelto | `LICENSE.md` (commit `9b3d515`) |
| 2 | El informe no tiene fecha ni tabla de contenidos | Pendiente | Informe final (Felipe) |
| 3 | `log` no está explicado; conviene interpolar por nombre | Resuelto en código; falta el informe | Interpolación por nombre en la gramática (PR #11) y ejecución en el runtime (`doc/stage-iii/runtime.md`). Explicarlo en 2.2 Lenguaje (Felipe) y 3.2 Backend, runtime (Eduardo) |
| 4 | ¿Los relacionales aplican solo a notas e intervalos? | Decidido y documentado | Tabla por tipo en `doc/stage-ii/decisiones.md`. Enteros ya se ejecutan; notas e intervalos, con la biblioteca musical (Eduardo, paso 3). Explicarlo en 2.2 Lenguaje |
| 5 | MIDI: falta la asignación de instrumentos y el manejo de pistas | Instrumentos resueltos; pistas pendientes | `with instrumento` (PR #10) con la tabla General MIDI de `semanticInstrumentProgram`. No hay sintaxis de pistas: es una decisión de diseño. Falta fijar y documentar cómo se reparten las voces en pistas o canales al definir `Output.h` (Felipe, con el armado de eventos de Eduardo) |
| 6 | Dos carpetas de documentación | Resuelto | Todo en `doc/` (consignas en `doc/consignas/`), commit `9b3d515` |
| 7 | `CLAUDE.md` debe importar `AGENTS.md` | Resuelto | `CLAUDE.md` contiene `@AGENTS.md` (commit `9b3d515`) |
| 8 | Texto sin justificar | Pendiente | Informe final (Felipe) |
| 9 | Sin números de página | Pendiente | Informe final (Felipe) |
| 10 | Numeración de §3.2, construcciones, casos de aceptación y de rechazo mezclada | Pendiente | Informe final (Felipe): una numeración independiente para cada lista |
| 11 | Los ejemplos de código deben ser figuras con leyenda | Pendiente | Informe final (Felipe). Además, actualizar los ejemplos a la sintaxis actual: el 6.2 usa `log "... {1}", grado` y el 6.4 usa `v`, que es un grado reservado |

## Lista de control del informe final

Antes de entregar el Stage III, el informe debe cumplir:

- [ ] Fecha en la portada.
- [ ] Tabla de contenidos con las secciones de la consigna.
- [ ] Texto justificado.
- [ ] Números de página.
- [ ] Numeración independiente para secciones, construcciones, casos de
      aceptación y casos de rechazo.
- [ ] Cada ejemplo de código en una figura enmarcada, con número y leyenda.
- [ ] Ejemplos compilables con la sintaxis actual (verificados con el
      compilador).
- [ ] Explicación de `log` (interpolación, escapes, salida) y de los
      operadores relacionales por tipo.
- [ ] Explicación de instrumentos y de la decisión sobre pistas MIDI.
