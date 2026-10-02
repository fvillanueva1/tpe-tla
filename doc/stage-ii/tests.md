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
