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
