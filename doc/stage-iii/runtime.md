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
  Cada bloque, cada iteración de `while` y `for` y cada llamada crean un alcance
  y lo liberan al salir, también ante un error o un `return`.
- `Runtime.h` / `Runtime.c`: el evaluador. La semántica ya garantiza nombres,
  tipos, aridades y retornos; el runtime solo comprueba valores.

## Construcciones que ejecuta

Enteros, booleanos, strings, duraciones y sus vectores, declaraciones,
asignaciones, aritmética, lógica, comparaciones, índices, `if` / `else if` /
`else`, `while`, `for`, procedimientos y `log`.

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
- **`for variable = desde to hasta`:** ambos límites son inclusivos y se
  evalúan una vez. Si `desde > hasta` no hay iteraciones. Cada iteración liga
  la variable al valor del contador; asignarla dentro del cuerpo no cambia la
  cantidad de iteraciones.
- **Procedimientos:** se registran al llegar a su `define`. Los argumentos se
  evalúan en el alcance del llamador y se copian, vectores incluidos (paso por
  valor). El cuerpo corre en un alcance cuyo padre es el global, así que ve y
  puede modificar las globales. La profundidad máxima de llamadas anidadas es
  `RUNTIME_MAX_CALL_DEPTH` (1000).
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
| Recursión demasiado profunda | `procedure 'p' exceeds the maximum call depth of 1000` |
| Memoria insuficiente | `out of memory` (código 2) |

## Pendiente

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
`test.sh` cambia el código de salida de AddressSanitizer a 86 y rechaza
cualquier reporte del sanitizador, así un error de memoria o una fuga no pasa
por un rechazo esperado (código 1).
