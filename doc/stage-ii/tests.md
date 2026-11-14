# Casos de prueba de Stage II

Cada archivo prueba una construcción pequeña. `test.sh` verifica el código de
salida del compilador y, para todas las aceptaciones, también la
salida estándar y los archivos exportados (ver `doc/stage-iii/runtime.md`). La extensión `.cad` identifica programas Cadence.

Se incluye desde el primer commit el rechazo de un programa vacío para que
las dos carpetas que recorre el runner existan y contengan tests reales.
Cada modo se prueba en su propio archivo.

Los prefijos numéricos corresponden a los casos de Stage I. Los tests sin
prefijo amplían la cobertura de las decisiones de Stage II. El primer grupo
cubre los casos 31–38 y 41–43, además de modos, strict, escala, novenas,
grados disminuidos, propiedades, comentarios, operadores, índices y `aug4`.

El segundo grupo completa los casos 30, 39–40 y 44–48: bloques, procedimientos,
voces, check y salida. También cubre instrumentos, strings e interpolación.
El caso 45 comprueba la sintaxis de check, no demuestra validez musical.

## Rechazos

`reject/` cubre los casos 49–63 del Stage I y errores adicionales de léxico,
sintaxis, semántica y ejecución. Los casos 56–59 obligan a quintas u octavas
paralelas o usan un voicing explícito para provocar el cruce y la tesitura.
`pending/` quedó vacía.

## Actualizacion de Stage III

La suite incluye rechazos semanticos activos y exige el mensaje `Semantic error:` para ellos, ademas del codigo 1. Los casos 54 a 57 y 60 a 63, instrumento y tempo invalidos ya son activos. Los casos 58 y 59 pasaron a `reject/` con voicings explícitos, que fijan las voces: 58 exige "voice crossing at chord 1" y 59 "tessitura at chord 1".

Los casos de rangos cubren literales y expresiones constantes; `semantic-dynamic-values.cad` verifica que las expresiones dependientes de variables sigan siendo aceptadas estaticamente.

Los rechazos de tipos tambien cubren parametros, limites de for, voice, check, duracion de arpegio, inversion, modulacion, relaciones tonales y vectores heterogeneos.

Los casos de `duration` verifican declaracion, arpegio, vectores, parametros y retornos, y el rechazo de valores incompatibles.

`music-validation.c` prueba directamente pertenencia tonal, tesituras, cruce y movimiento paralelo sobre alturas MIDI; no reemplaza los casos Cadence pendientes de integrar.

La validacion de secuencias SATB comprueba los resultados y el indice de error, las reglas opcionales, la tesitura obligatoria y las entradas invalidas.

La prueba musical tambien comprueba escalas de los siete modos, cruce de octava y limites de salida.

La suite ejecuta cada aceptacion en un directorio temporal y comprueba bytes MIDI para escalas de Do y Sol, incluyendo instrumento, tempo, duraciones, errores de escritura y proteccion de archivos existentes.

El caso 55 comprueba `strict` con una tonalidad y un acorde literales. Casos adicionales cubren un acorde diatonico, otro modo, reemplazo de contexto y valores que requieren evaluacion en ejecucion.

`strict` tambien verifica notas variables de valor literal inmutable; si hay alguna asignacion al mismo nombre en el programa, su valor se considera desconocido y queda para ejecucion.

Las pruebas de realizacion SATB comprueban cobertura de notas, tesituras, ausencia de cruce, minimo de movimiento conocido, y acordes imposibles.

Los casos 56 y 57 usan progresiones que fuerzan las paralelas bajo la realizacion de minimo movimiento. Se comprueba tambien una progresion referida por variables inmutables y la seleccion de reglas de `check`.

Los casos 56 y 57 verifican tambien el nombre de la regla y el numero del acorde en el diagnostico.

Se verifica la conduccion y el contrapunto de progresiones por grados conocidas, ademas de acordes explicitos.

Los casos SATB cubren ademas acordes por grado y septimas con tonalidades y grados inmutables conocidos.

Grados, tempos, inversiones y transposiciones tambien se prueban con variables inmutables conocidas; las variables asignadas en el programa permanecen desconocidas para estos controles estaticos.

Las pruebas cubren valores escalares asignados en linea recta y olvidados despues de una rama, para evitar rechazos basados en un camino que podria no ejecutarse.

El contexto estricto se prueba con reasignacion a una tonalidad conocida, alias y asignaciones a una tonalidad distinta.

Un alias tonal conserva su valor anterior cuando se reasigna la variable de origen; el chequeo estricto valida esa copia.

Los rechazos de asignacion y operador verifican ademas los tipos incluidos en el diagnostico.

La suite ejecuta todas las aceptaciones en directorios temporales: compara stdout con `src/test/c/output/<caso>.out` (vacío si falta) y los archivos con `src/test/c/artifacts/<caso>/`. Los rechazos también se ejecutan en directorios temporales y no pueden dejar archivos. `--check` se comprueba aparte y no tiene efectos de ejecución. Los fixtures `.ly` se fijan en LF en `.gitattributes` y ningún fixture usa caracteres inválidos en Windows (`runtime-export-values` prueba los escapes de la ruta con `nota\{final\}.mid`), para que el repositorio se pueda clonar y la comparación byte a byte pase desde cualquier sistema.

La integracion MIDI comprueba tambien una escala declarada y exportada por nombre.

Los indices constantes sobre vectores literales inmutables incluyen pruebas de limite y de conservacion de vectores que pueden cambiar.

Las llamadas antes de la definicion se rechazan, tambien desde otro procedimiento o cuando el cuerpo usa una global posterior; la recursion directa se acepta. El voicing explicito se acepta con acordes de cuatro notas y se rechaza con notas sueltas o acordes de otro tamano (`type-check-list`).

## Runtime musical completo

La suite tiene 106 aceptaciones y 155 rechazos. `runtime-musical-*` comprueba
valores, comparaciones, propiedades, transformaciones, modulación y copias por
valor de vectores de progresiones y voicings. `runtime-strict-scope` comprueba
contexto tonal y restauración al salir de bloques. Los rechazos `runtime-*`
exigen diagnósticos específicos para grados e índices fuera de rango,
transposiciones, inversiones, repetición negativa, `strict` calculado, SATB,
modulación sin clave, tempo y escritura. El antiguo caso de aceptación
`semantic-loop-unknown` es `runtime-loop-transpose-range`: se acepta
estáticamente, pero su ejecución sale del rango de alturas.

Los fixtures MIDI/LilyPond se construyen desde eventos esperados mediante
`generate-runtime-goldens.py`, sin ejecutar el compilador. Cubren notas,
escalas, acordes, progresiones, arpegios, cuatro canales SATB, tempo,
instrumento, vacíos, rutas con escapes y capturas previas a reasignaciones.
No se regeneran durante la suite. Una salida inesperada hace fallar el caso.

Los programas de `demo/` son ejemplos para mostrar el lenguaje y no forman
parte de la suite: `test.sh` no los recorre y sus salidas no se versionan.

`runtime-value.c` comprueba copia profunda y liberación. Los errores posteriores a `export`
conservan los logs previos y no escriben archivos. La suite rechaza reportes
de ASan incluso si el programa ya devolvió un código de rechazo esperado.
