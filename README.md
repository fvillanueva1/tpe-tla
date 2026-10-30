# Cadence

Grupo **G-150**: Felipe Villanueva, Eduardo Tormakh y Tadeo Gorganchian.

Cadence es un lenguaje de dominio específico para describir armonía tonal a
partir de tonalidades, grados, acordes y progresiones. El compilador, escrito
en C con Flex y Bison, derivará las notas concretas, validará reglas musicales
y generará artefactos reproducibles.

## Estado del proyecto

El proyecto se encuentra iniciando **Stage III: Backend**. Está implementado en C con
Flex y Bison: reconoce programas Cadence y construye su árbol de sintaxis
abstracta (AST), con destructores para liberar los nodos y sus hijos.
La especificación está disponible como [PDF de Stage I](<doc/Cadence%20-%20Stage%20I.pdf>);
las [decisiones compartidas](doc/stage-ii/decisiones.md) actualizan su sintaxis.

Se reconocen declaraciones y asignaciones, notas e intervalos, tonalidades,
escalas, acordes, progresiones, vectores e índices, propiedades, operadores,
transformaciones, control de flujo, procedimientos, `voice ... as SATB`,
`check`, `export` a MIDI/sheet y `log` con interpolación por nombre.

El frontend construye el AST y el analizador semantico verifica nombres, alcances y tipos, ademas de algunos valores musicales conocidos. La evaluacion musical general y la realizacion SATB en ejecucion siguen pendientes. La generacion MIDI soporta por ahora unicamente una tonalidad literal seguida de la exportacion de su escala, directamente o mediante una declaracion de escala; las demas salidas siguen pendientes. El detalle y las limitaciones estan en [analisis semantico](doc/stage-ii/semantica.md).

## Documentación

- [Especificación final de Stage I](<doc/Cadence%20-%20Stage%20I.pdf>)
- [Decisiones de diseño de Stage II](doc/stage-ii/decisiones.md)
- [Tokens y nodos del AST](doc/stage-ii/tokens.md)
- [Cobertura y clasificación de los tests](doc/stage-ii/tests.md)
- [Consignas y material de la cátedra](doc/consignas/)

La carpeta `doc/` contiene los entregables y, en `doc/consignas/`, el material
de referencia de la cátedra.

## Desarrollo

Se necesita [Docker](https://www.docker.com/) con Compose. La imagen del repo
incluye GCC, Flex, Bison, CMake y Make; no es necesario instalarlos en el host.
Desde la raíz del repositorio:

```bash
chmod u+x src/main/bash/*.sh
docker compose build compiler
docker compose run --rm compiler bash -lc 'src/main/bash/build.sh && src/main/bash/test.sh'
```

La compilación genera `.build/Flex-Bison-Compiler`, el parser y el scanner.
Bison debe terminar sin conflictos (`%expect 0`); GCC habilita AddressSanitizer.
Los archivos generados no se versionan. El ejecutable es de Linux, por lo que
en macOS también se ejecuta dentro del contenedor.

Para ejecutar un programa:

```bash
docker compose run --rm compiler bash -lc 'src/main/bash/run.sh src/test/c/accept/30-export-scale.cad'
```

También se puede abrir una consola con `docker compose run --rm compiler bash`
y ejecutar allí los scripts de compilación, ejecución y tests.

## Tests y pendientes

Cada test es un programa pequeño enfocado en una construcción. Los prefijos
numéricos mantienen la correspondencia con Stage I; los demás prueban decisiones
como `==`, `aug4`, instrumentos e interpolación.

| Carpeta | Contenido | Resultado esperado |
|---------|-----------|-------------------|
| `src/test/c/accept/` | Programas validos segun el analizador | Codigo 0 con `--check` |
| `src/test/c/reject/` | Errores lexicos, sintacticos y semanticos | Codigo 1 |
| `src/test/c/pending/` | Escenarios musicales pendientes | No se ejecutan en la suite |

`test.sh` ejecuta `accept/` y `reject/` y exige el diagnostico semantico en los casos correspondientes. Comprueba el contenido MIDI de la primera exportacion de escala y las funciones de validacion musical; la evaluacion general sigue pendiente. Los casos 55 a 57 validan `strict` y paralelas con valores conocidos; los casos 58 y 59 siguen pendientes.

Los archivos 56–59 son **escenarios por concretar**, no pruebas que ya
garanticen quintas u octavas paralelas, cruce de voces o errores de tesitura.
Enumerar notas no fija su asignación a SATB; el algoritmo posterior puede
elegir otras inversiones y octavas. Se deberá definir una entrada o realización
que obligue a cada error. No se declara tesitura en la sintaxis: los rangos
vocales serán fijos. Ver [tests.md](doc/stage-ii/tests.md).

En los commits iniciales se prepararon tests de nuevas construcciones en
`src/test/c/planned/`. Cada implementación los activó moviéndolos a `accept/`
o `reject/`. Esa carpeta era temporal y ya no tiene archivos en el estado final;
es distinta de `pending/`, que se conserva para Stage III. Cada uno de los ocho
commits de Eduardo compila y pasa la suite activa de ese momento.

## Revisión final del frontend

Con toda la gramática integrada se hicieron las dos revisiones de cierre de
Stage II, sobre la imagen Docker del repo (Bison 3.8.2):

- **Conflictos de Bison:** `bison -Wcounterexamples` no emite ningún aviso y la
  gramática conserva `%expect 0`. Con `-Wall` solo aparecen siete avisos
  `-Wprecedence` (`BY`, `AS`, `TO`, `OF`, `NOT`, `DOT` y `OPEN_BRACKET` se
  declaran con `%left`/`%right`/`%nonassoc` aunque no son asociativos). Son
  cosméticos: no hay conflictos ni reglas inútiles, y no se modificaron.
- **Destructores y memoria:** `destroyExpression` y `destroyStatement` cubren
  todos los valores de `ExpressionType` y `StatementType`, y cada nodo auxiliar
  (`Voice`, `Check`, `Export`, `Log`, `StringPart`, listas y procedimientos)
  libera a sus hijos. Los 80 programas de `accept/` y `reject/` se ejecutaron
  con AddressSanitizer y detección de fugas sin errores. La imagen no incluye
  `valgrind`; AddressSanitizer cubre el mismo chequeo.

## Ejemplo

```text
key tonalidad = C major;
progression tema = [I, IV, V, I] in tonalidad;
voicing voces = voice tema as SATB;
check voces for parallel_fifths, parallel_octaves, voice_crossing;
log "Exportando {voces}";
export voces as midi to "coral.mid" at 90 bpm with piano;
```

Este programa se acepta y produce un AST; todavía no escribe `coral.mid`.
Se usa `voces` porque `v` es un grado reservado. La comparación se escribe
`==`, la cuarta aumentada `aug4` y los grados disminuidos con sufijo `dim`.

## Feedback y siguientes etapas

La licencia incluye a los tres integrantes, la documentación vive en `doc/`
y `CLAUDE.md` importa `AGENTS.md`. Los cambios de lenguaje acordados tras
Stage I están registrados en `decisiones.md`: interpolación por nombre,
comparación entre enteros e instrumento opcional como identificador común.

Cada exportación admite un instrumento opcional; no hay sintaxis de pistas.
La tabla MIDI y los tipos por operador tienen una implementacion inicial. Las validaciones musicales
se implementarán en Stage III. Los escapes se conservan sin decodificar en
el AST. Los cambios de presentación del informe (fecha, índice, justificación,
paginación y figuras) corresponden al informe final: no se reentrega el PDF de
Stage I y Stage II no incorpora un nuevo informe.

Las entregas se integran en `development` mediante PRs revisados por otro
integrante. La entrega a la cátedra debe identificar el grupo, los integrantes
y el hash completo del commit de `development` después de integrar la revisión.

## Primera exportacion MIDI

Un programa con dos sentencias, `key tonalidad = C major;` y `export scale of tonalidad as midi to "escala.mid" at 120 bpm;`, produce una escala MIDI de siete notas en octava 4. El tempo literal y el instrumento son opcionales; se usan 120 BPM y piano por defecto. La ruta no debe existir: el compilador devuelve error sin sobrescribirla. Por ahora no se admiten rutas con escapes ni expresiones calculadas de tempo en esta salida. Los programas cuya generacion todavia no esta implementada devuelven error y no producen salida. Para comprobar solo sintaxis y semantica, usar `src/main/bash/run.sh archivo.cad --check`; este modo no genera archivos.
