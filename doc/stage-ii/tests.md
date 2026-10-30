# Casos de prueba de Stage II

Cada archivo prueba una construcción pequeña; `test.sh` solo verifica el código
de salida del compilador. La extensión `.cad` identifica programas Cadence.

Se incluye desde el primer commit el rechazo de un programa vacío para que
las dos carpetas que recorre el runner existan y contengan tests reales.
Cada modo se prueba en su propio archivo.

Los prefijos numéricos corresponden a los casos de Stage I. Los tests sin
prefijo amplían la cobertura de las decisiones de Stage II. El primer grupo
cubre los casos 31–38 y 41–43, además de modos, strict, escala, novenas,
grados disminuidos, propiedades, comentarios, operadores, índices y `aug4`.

El segundo grupo completa los casos 30, 39–40 y 44–48: bloques, procedimientos,
voces, check y salida. También cubre instrumentos, strings e interpolación.
Los casos de voz, check, exportación y strings se preparan en `planned/`,
separados por feature y resultado esperado. No los recorre `test.sh` todavía;
cada commit de implementación mueve sus casos a `accept/` o `reject/`.
Esta carpeta temporal no contiene errores semánticos y debe desaparecer al
terminar Stage II. `pending/` se reserva para análisis semántico de Stage III.
El caso 45 comprueba la sintaxis de check, no demuestra validez musical.

## Rechazos y pendientes

`reject/` cubre 49–53 y errores adicionales de sintaxis. `pending/` contiene
54–63 y otros errores que requieren tipos, tabla MIDI, rangos o símbolos.
Los rechazos de voces, check, exportación y log también se preparan en
`planned/` y se activan con sus reglas: un rechazo no debe pasar simplemente
porque el parser todavía no reconozca la construcción que pretende probar.
`test.sh` no recorre `pending/`. Solo se moverán esos archivos a `reject/`
cuando Stage III implemente y verifique el diagnóstico correspondiente.

Los casos 56–59 son candidatos, no regresiones semánticas concluyentes:
la sintaxis actual no permite asignar notas directamente a voces SATB, y
`voice` puede elegir inversiones y octavas diferentes. Stage III deberá
fijar la realización o seleccionar entradas que obliguen al error antes
de activar esos cuatro casos. No se inventa sintaxis de tesitura.

## Cobertura respecto de Stage I

| Casos | Situación |
|-------|-----------|
| 30–48 | Programas activos que prueban la sintaxis; no verifican resultados musicales |
| 49–53 | Rechazos activos del frontend; el 49 tiene dos variantes |
| 54–55 y 60–63 | Casos semánticos excluidos hasta Stage III |
| 56–59 | Escenarios por concretar con el algoritmo de realización y los rangos vocales |

Hay un archivo por cada número 30–63, pero **tener el archivo no demuestra
cobertura efectiva de los cuatro errores musicales pendientes**. Tampoco un
código de salida 0 demuestra que el AST se haya evaluado o que un archivo MIDI
se haya generado. En Stage II solo se construye y libera el árbol.

## Verificación por commit

Cada snapshot se compiló en una copia aislada con la imagen Docker del repo,
GCC, Flex, Bison y los scripts originales. No hubo warnings ni conflictos.
Todos los rechazos activos terminaron con código 1, sin errores de AddressSanitizer.

| Commit de Eduardo | Aceptaciones activas | Rechazos activos |
|-------------------|----------------------|------------------|
| 1: armonía y operadores | 28 | 1 |
| 2: control y tests preparados | 39 | 1 |
| 3: reject y pending | 39 | 13 |
| 4: voice y activación de sus tests | 40 | 14 |
| 5: check y activación de sus tests | 41 | 16 |
| 6: export y activación de sus tests | 46 | 19 |
| 7: log y activación de sus tests | 53 | 27 |
| 8: documentación | 53 | 27 |

Al finalizar no quedan archivos en `planned/`. `pending/` contiene 13 archivos:
nueve casos semánticos y cuatro escenarios musicales por concretar.

## Actualizacion de Stage III

La suite incluye rechazos semanticos activos y exige el mensaje `Semantic error:` para ellos, ademas del codigo 1. Los casos 54 a 57 y 60 a 63, instrumento y tempo invalidos ya son activos. Los casos 58 y 59 permanecen en `pending/`: necesitan escenarios concluyentes o una forma de fijar las voces. Las tablas anteriores describen los snapshots historicos de Stage II.

Los casos de rangos cubren literales y expresiones constantes; `semantic-dynamic-values.cad` verifica que las expresiones dependientes de variables sigan siendo aceptadas estaticamente.

Los rechazos de tipos tambien cubren parametros, limites de for, voice, check, duracion de arpegio, inversion, modulacion, relaciones tonales y vectores heterogeneos.

Los casos de `duration` verifican declaracion, arpegio, vectores, parametros y retornos, y el rechazo de valores incompatibles.

La prueba `semantic-oom.c` fuerza un fallo de reserva para verificar el codigo `OUT_OF_MEMORY` y se ejecuta desde `test.sh`.

`music-validation.c` prueba directamente pertenencia tonal, tesituras, cruce y movimiento paralelo sobre alturas MIDI; no reemplaza los casos Cadence pendientes de integrar.

La validacion de secuencias SATB comprueba los resultados y el indice de error, las reglas opcionales, la tesitura obligatoria y las entradas invalidas.

La prueba musical tambien comprueba escalas de los siete modos, cruce de octava y limites de salida.

La suite ejecuta cada aceptacion en un directorio temporal y comprueba bytes MIDI para escalas de Do y Sol, incluyendo instrumento, tempo, duraciones, errores de escritura y proteccion de archivos existentes.

El caso 55 comprueba `strict` con una tonalidad y un acorde literales. Casos adicionales cubren un acorde diatonico, otro modo, reemplazo de contexto y valores que requieren evaluacion en ejecucion.

`strict` tambien verifica notas variables de valor literal inmutable; si hay alguna asignacion al mismo nombre en el programa, su valor se considera desconocido y queda para ejecucion.

Las pruebas de realizacion SATB comprueban cobertura de notas, tesituras, ausencia de cruce, minimo de movimiento conocido, acordes imposibles y fallos de reserva.

Los casos 56 y 57 usan progresiones que fuerzan las paralelas bajo la realizacion de minimo movimiento. Se comprueba tambien una progresion referida por variables inmutables y la seleccion de reglas de `check`.

Los casos 56 y 57 verifican tambien el nombre de la regla y el numero del acorde en el diagnostico.
