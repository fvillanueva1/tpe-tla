# Runtime de Cadence

El compilador interpreta el AST: no genera otro programa. Después de
`analyzeSemantics`, `executeGenerator` prueba primero la exportación literal de
escalas (`exportLiteralScale`) y, si no aplica, ejecuta el programa con el
runtime de `src/main/c/backend/runtime/`.

## Módulos

- `Value.h` / `Value.c`: el valor de ejecución y el entorno. Un `Value` lleva
  su `DataType` (o el de sus elementos, si es vector) y una unión con el entero,
  el booleano, la duración, el string ya decodificado o el arreglo de elementos.
  Los tipos musicales se agregan cuando `backend/music/Music.h` defina su
  representación. Las copias son profundas.
- `RuntimeScope`: lista de variables con puntero al alcance padre. La búsqueda
  va del alcance más interno al global, con las mismas reglas de ocultamiento
  que la semántica (`doc/stage-ii/semantica.md`).
  El alcance global se crea al empezar la ejecución y se libera al terminar,
  también ante un error.
- `Runtime.h` / `Runtime.c`: el evaluador. La semántica ya garantiza nombres,
  tipos, aridades y retornos; el runtime solo comprueba valores.

## Construcciones que ejecuta

Enteros, booleanos, strings, duraciones y sus vectores, declaraciones,
asignaciones, aritmética, lógica, comparaciones, índices y `log`.

Antes de ejecutar, `runtimeSupports` recorre todo el programa. Si aparece una
construcción que el runtime todavía no ejecuta (notas, tonalidades, acordes,
`voice`, `check`, `export` de valores calculados), el compilador falla con
*"Code generation is not implemented for this program"* sin ejecutar nada, así
no deja efectos parciales. Esa lista se amplía a medida que la biblioteca
musical esté disponible.

## Semántica de ejecución

- **Aritmética:** enteros de 32 bits. La división trunca hacia cero
  (`(0 - 7) / 2` es `-3`).
- **Lógica:** `and` y `or` evalúan en cortocircuito: el operando derecho solo
  se evalúa si decide el resultado.
- **Vectores:** índices de base cero. Una asignación reemplaza el vector entero.
- **`log`:** imprime el mensaje en la salida estándar seguido de un salto de
  línea. Decodifica los escapes (`\"`, `\\`, `\{`, `\}`, `\n`, `\t`) e
  interpola cada `{variable}` con su valor actual. Los vectores se muestran como
  `[1, 2]`, los booleanos como `true` / `false` y las duraciones por su nombre.
- **Strings:** un literal string se interpola al evaluarse; cambiar después
  una variable interpolada no cambia el string ya guardado.

## Errores de ejecución

Un error imprime `Runtime error: ...` en la salida de error y termina con
código 1. Lo que `log` imprimió antes del error ya está en la salida estándar.
Se detectan:

| Error | Mensaje |
|-------|---------|
| División por cero | `division by zero` |
| Desborde de enteros (`+ - * /`, incluido `-2147483648 / -1`) | `integer overflow in A op B` |
| Índice fuera de rango (base cero) | `index N is out of range for a vector of length L` |
| Memoria insuficiente | `out of memory` (código 2) |

## Pendiente

- `if`, `while` y `for`.
- Procedimientos y límite de recursión.
- Expresiones musicales, `voice`, `check` y `strict` con valores calculados,
  sobre `backend/music/Music.h`.
- `export` general: el runtime acumulará los eventos y los escribirá recién al
  terminar sin errores, para no dejar archivos si el programa falla después de
  un `export`.

## Tests

`test.sh` ejecuta de verdad cada caso de `src/test/c/accept/` que tenga un
`src/test/c/output/<caso>.out`, compara la salida estándar byte a byte y
comprueba que no se escriba ningún archivo. Los casos sin `.out` siguen con
`--check`. Los rechazos `src/test/c/reject/runtime-*.cad` exigen el diagnóstico
`Runtime error:` (y ningún `Semantic error:`), además del mensaje concreto.
