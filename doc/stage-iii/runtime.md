# Runtime de Cadence

El compilador interpreta el AST: no genera otro programa. Después de
`analyzeSemantics`, `executeGenerator` prueba primero la exportación literal de
escalas (`exportLiteralScale`) y, si no aplica, ejecuta el programa con el
runtime de `src/main/c/backend/runtime/`.

## Módulos

- `Value.h` / `Value.c`: el valor de ejecución y el entorno. Un `Value` lleva
  su `DataType` (o el de sus elementos, si es vector) y una unión con el entero,
  el booleano, la duración, el string ya decodificado o el arreglo de elementos.
  Incluye las estructuras de `Music.h` para notas, intervalos, tonalidades,
  escalas, acordes, progresiones y voicings. Las copias son profundas, también
  para los arreglos de progresiones y voicings dentro de vectores.
  Calidad y modo son tipos internos de expresión. Las notas conservan su
  grafía al imprimir; acordes usan llaves y escalas, progresiones y voicings
  usan corchetes. Una duración distinta de negra se muestra con `as duración`.
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
  `RUNTIME_MAX_CALL_DEPTH` (1000). Como el intérprete es recursivo, también mide
  la pila de C usada: si supera la mitad del límite del sistema (`RLIMIT_STACK`,
  como máximo 8 MB), corta con un error de ejecución en lugar de terminar con
  un desborde de pila. Cada bloque y cada expresión anidada del cuerpo suman
  marcos de C a cada llamada, así que el límite de pila suele llegar antes que
  el de 1000: con el binario compilado con AddressSanitizer, un cuerpo con dos
  `if` anidados corta alrededor de las 945 llamadas y uno con seis, alrededor
  de las 530. En ambos casos el programa termina con un diagnóstico.
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
| Anidamiento que agota la pila de C (bloques y expresiones dentro de muchas llamadas) | `the program nests too deeply for the available stack` |
| No se puede escribir la salida de `log` | `cannot write the log output` |
| Memoria insuficiente | `out of memory` (código 2)* |

\* El runtime devuelve el código 2 cuando falla una reserva, pero el binario se
enlaza siempre con AddressSanitizer, que suele abortar el proceso (o el sistema
lo termina) antes de que `malloc` devuelva `NULL`. Para ejercitar esa ruta hay
que forzar el fallo de las reservas (por ejemplo, con `-Wl,--wrap=malloc`), no
agotar la memoria real.

## Pendiente

- Expresiones musicales, `voice`, `check` y `strict` con valores calculados,
  sobre `backend/music/Music.h`.
- `export` general: el runtime acumulará los eventos y los escribirá recién al
  terminar sin errores, para no dejar archivos si el programa falla después de
  un `export`.

## Tests

`test.sh` ejecuta de verdad cada caso de `src/test/c/accept/` que tenga un
`src/test/c/output/<caso>.out`, compara la salida estándar byte a byte y
comprueba que no se escriba ningún archivo. Un `.out` sin caso de aceptación
con el mismo nombre hace fallar la suite, así un error de nombre no deja el
caso en `--check` sin aviso. Ningún rechazo puede dejar archivos en su
directorio de trabajo. Los casos sin `.out` siguen con
`--check`. Los rechazos `src/test/c/reject/runtime-*.cad` exigen el diagnóstico
`Runtime error:` (y ningún `Semantic error:`), además del mensaje concreto.
Para la garantía de no ejecutar nada, `test.sh` corre programas que empiezan
con `log "antes";` y tienen una nota dentro de un `if`, un `while`, un `for`,
el cuerpo de un procedimiento, un parámetro, el tipo de retorno o un
argumento: exige la salida estándar vacía y el mensaje de generación no
implementada.
`test.sh` cambia el código de salida de AddressSanitizer a 86 y rechaza
cualquier reporte del sanitizador, así un error de memoria o una fuga no pasa
por un rechazo esperado (código 1).

`runtime-value.c` verifica copias independientes de progresiones dentro de
vectores y de los dos arreglos de un voicing, liberación repetida e impresión
con duraciones. Se ejecuta con AddressSanitizer.

La prueba de recursión infinita acepta el límite de 1000 llamadas o la guarda
de pila: el tamaño de los valores y la instrumentación de ASan determinan cuál
se alcanza primero. La prueba con bloques anidados exige la guarda de pila.

El evaluador musical ya resuelve literales de nota, intervalo y tonalidad,
relaciones tonales, escalas, acordes por notas o grados, propiedades,
comparaciones por altura/semitonos e índices de escalas. Los grados enteros
son de 1 a 7; los índices siguen siendo de base cero. La integración con el
generador permanece detrás del recorrido de soporte hasta completar la salida.

Las listas de acordes se convierten a progresión al inicializar, asignar,
pasar un argumento o devolver ese tipo. También se convierten al concatenar,
repetir, transponer o modular. El evaluador ejecuta `in`, las transformaciones
y el acceso por índice a progresiones; la semántica ahora admite ese acceso.
Se conservan tonalidad, grafía y duración según el contrato de `Music.h`.
Todos los operandos se evalúan una vez, de izquierda a derecha, y se liberan
aun cuando el segundo operando falla.

`strict` se comprueba con valores reales al evaluar cada acorde, incluidos los
de progresiones, vectores y voicings. Rige la última tonalidad escalar ligada
en el alcance más interno; los parámetros de tonalidad se ligan en orden de
la firma. Una tonalidad no estricta tapa el contexto exterior. Al asignar la
tonalidad vigente cambia su valor; asignar otra anterior no cambia cuál rige.
Los bloques restauran el contexto exterior al salir y los procedimientos
heredan el contexto global, sin tomar tonalidades locales del llamador.
Una violación informa `chord contains a note outside the strict key`.

`voice` convierte la progresión y delega la distribución en
`musicVoiceProgression`. Un voicing explícito toma las cuatro notas de cada
acorde en orden S,A,T,B sin redistribuir. Se comprueba su tesitura al construirlo
o recibirlo. `check` aplica las reglas solicitadas al valor actual e informa
la primera violación, numerando acordes desde uno: `parallel fifths at chord 2`,
`parallel octaves`, `voice crossing` o `tessitura`. Una lista vacía es válida.
Los errores de realización o de cantidad de voces son errores de ejecución.

Cada `export` evalúa el valor y el tempo y guarda una partitura independiente,
con la ruta ya decodificada. Cambios posteriores de variables no alteran esa
partitura. Las exportaciones se acumulan incluso dentro de ciclos o llamadas.
Solo al terminar sin errores se invoca una vez `outputWriteFiles` con el lote
completo. Un fallo previo libera las partituras sin escribir nada; un fallo
de escritura revierte el lote y se informa como `cannot write 'ruta'`.
Destinos repetidos o existentes fallan sin sobrescribir. MIDI exige de 4 a
60000000 BPM; `sheet` admite cualquier entero positivo.
