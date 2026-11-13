# Runtime de Cadence

El compilador interpreta el AST: no genera otro programa. Después de
`analyzeSemantics`, `executeGenerator` llama a `executeProgram` en
`src/main/c/backend/runtime/`. Todos los programas usan esa ruta, incluida la
exportación de escalas, que conserva los bytes MIDI anteriores.
`--check` ejecuta solo frontend y semántica: no imprime `log` ni escribe archivos.

## Valores y alcances

`Value.h` / `Value.c` representan todos los tipos declarables: enteros,
booleanos, strings, duraciones, notas, intervalos, tonalidades, escalas,
acordes, progresiones y voicings, y sus vectores. Nota, intervalo y tonalidad
usan estructuras pequeñas de `Music.h`. Escalas y acordes poseen un puntero a
su estructura en el heap; progresiones y voicings poseen sus arreglos.
Así `sizeof(Value)` es 32 bytes en el contenedor de pruebas de 64 bits, en lugar
de 184. Las copias de escalas y acordes también reservan su propio payload. Calidad y modo son tipos internos
de expresión; los grados literales se evalúan como enteros de 1 a 7.

Las copias son profundas, también para progresiones y voicings dentro de
vectores. Se liberan los temporales, argumentos y alcances al salir, ante un
error y ante un `return`. Una asignación reemplaza el valor completo. Las
listas de acordes adquieren su tipo por contexto: vector de acordes,
progresión o voicing explícito. También se resuelven las listas vacías en
inicializadores, asignaciones, argumentos, retornos y exportaciones.

`RuntimeScope` guarda variables y un puntero al alcance padre. La búsqueda
va del alcance más interno al global, con las mismas reglas de ocultamiento
que la semántica. Cada bloque, cada iteración y cada llamada crea un alcance.
La semántica garantiza nombres, tipos, aridades y retornos; el runtime
comprueba valores.

## Semántica de ejecución

- **Operandos:** se evalúan una vez, de izquierda a derecha.
- **Aritmética:** enteros de 32 bits, con comprobación de desborde. La división
  trunca hacia cero: `(0 - 7) / 2` da `-3`.
- **Lógica:** `and` y `or` evalúan en cortocircuito.
- **Índices:** de base cero en vectores, escalas y progresiones. Indexar una
  progresión devuelve el acorde; no modifica la duración del paso original.
- **`for variable = desde to hasta`:** límites inclusivos evaluados una vez.
  Si `desde > hasta` no hay iteraciones. Asignar la variable dentro del cuerpo
  no cambia el contador del bucle.
- **Procedimientos:** se registran al llegar a su `define`. Los argumentos se
  evalúan en el alcance del llamador y se pasan por valor, vectores incluidos.
  El cuerpo ve y puede modificar globales; no hereda las variables locales del
  llamador. La profundidad máxima es `RUNTIME_MAX_CALL_DEPTH` (1000).
  Además se limita el uso de pila de C a la mitad de `RLIMIT_STACK`, tomando
  como máximo un límite de 8 MB. Según el tamaño del cuerpo y la instrumentación
  de AddressSanitizer, la guarda de pila puede alcanzarse antes de 1000 llamadas
  con muchos bloques anidados. En el contenedor de pruebas (GCC 14.2.0,
  AArch64, `-O3 -fsanitize=address`, pila de 8 MB), la cuenta descendente desde
  999 termina correctamente, también con dos `if` anidados en cada llamada:
  son 1000 llamadas activas, contando el caso base. Antes de reducir `Value`,
  los máximos eran 661 y 300 respectivamente. Con `-fstack-usage`, `_execute`
  baja de 3792 a 960 bytes, `_evaluateUnchecked` de 1680 a 720 y `_call` de
  848 a 464. Los helpers de exportación, `for` e índices evitan que GCC
  reserve sus temporales en todos los frames recursivos.
- **Comparaciones:** notas por altura MIDI (`C#4 == Db4`), intervalos por
  semitonos (`aug4 == d5`), enteros por valor; modo y calidad admiten igualdad
  y desigualdad. No se compara la representación binaria de las estructuras.
- **Strings y `log`:** los escapes `\"`, `\\`, `\{`, `\}`, `\n`, `\t` se
  decodifican al evaluar. Cada `{variable}` toma el valor actual. Un string
  guardado no cambia al reasignar después una variable interpolada. La lectura
  interpolada aplica el mismo contexto `strict` que una expresión de variable,
  también dentro de vectores, progresiones y voicings. `log`
  agrega un salto de línea y comprueba la escritura de stdout.

Las notas se imprimen con su grafía; los intervalos, modos y calidades, por
su nombre. Una tonalidad se imprime como `C major strict`, sin octava.
Los acordes usan llaves; las escalas, progresiones, voicings y vectores,
corchetes. En progresiones y voicings una duración distinta de negra se muestra
con `as duración`. Los voicings conservan alturas y se imprimen con sostenidos.

## Evaluación musical

El evaluador delega en `Music.h`: literales de nota, intervalo, tonalidad y
acorde; escalas y relaciones tonales; tríadas, séptimas y novenas por grado;
progresiones de grados; concatenación, repetición, transposición, inversión,
arpegios y modulación. También resuelve pertenencia `in`, propiedades
(`.root`, `.quality`, `.tonic`, `.mode`, `.octave`, `.length`) e índices.
Las reglas de grafía, fundamental, duración y modulación están en
`doc/stage-iii/musica.md` y `doc/stage-ii/decisiones.md`.

Los grados enteros van de 1 a 7. Las inversiones van de cero a la cantidad de
notas. Repetir cero veces produce una progresión vacía; repetir una cantidad
negativa falla. Una transposición no puede salir de C0–B8. Un acorde tiene
como máximo `MUSIC_MAX_CHORD_NOTES` (16) notas, incluidas las repetidas.
Un acorde por notas no adquiere tonalidad implícita: para modularlo necesita
compartir progresión con un acorde que conozca su tonalidad.

### Contexto `strict`

Rige la última tonalidad escalar ligada en el alcance más interno. Los
parámetros de tonalidad se ligan en orden de la firma. Una tonalidad no
estricta tapa el contexto exterior. Asignar la tonalidad vigente cambia su
valor; asignar otra anterior no cambia cuál rige. Al salir de un bloque vuelve
el contexto exterior, con las asignaciones que el bloque haya realizado.

Un procedimiento hereda el contexto global existente al llamarlo, sin tomar
el de los bloques locales del llamador. Sus parámetros o declaraciones de
tonalidad pueden reemplazarlo. Estas reglas completan el contexto que el
analizador deja desconocido ante control de flujo y llamadas.

Los argumentos se evalúan y validan en el contexto `strict` del llamador,
de izquierda a derecha. Ligar un parámetro `key ... strict` establece el
contexto para las lecturas y expresiones del cuerpo; no revalida los otros
argumentos al ligarlos ni cambia el contexto donde se evalúan. Por ejemplo,
sin tonalidad estricta en el llamador:

```cadence
define aceptar(key contexto, chord acorde) { log "aceptado"; }
aceptar(C major strict, {C#4, F4, G#4});
```

Imprime `aceptado`: el cuerpo no lee el acorde. Agregar `chord copia = acorde;`
dentro del cuerpo falla con `chord contains a note outside the strict key`.
Si el llamador tiene un contexto estricto, este valida los argumentos incluso
cuando el parámetro de tonalidad del procedimiento sea no estricto.

Se comprueba cada acorde evaluado, incluidos resultados calculados, lecturas
de variables, argumentos, retornos, progresiones, vectores y voicings.
Todas sus notas deben pertenecer a la tonalidad estricta vigente por clase de
altura. Una nota suelta no activa esta comprobación. La violación informa
`chord contains a note outside the strict key`.

### `voice` y `check`

`voice` convierte el valor a progresión y llama a `musicVoiceProgression`.
La biblioteca aplica la omisión de notas y busca la distribución SATB.
Una lista usada como voicing explícito toma cuatro notas por acorde en orden
S,A,T,B, sin redistribuir, y comprueba su tesitura al construirla o recibirla.

`check` aplica las reglas pedidas al valor actual y siempre comprueba tesitura.
Informa la primera violación, con acordes numerados desde uno: por ejemplo,
`parallel fifths at chord 2`. Un voicing vacío es válido.

## Exportaciones por lote

Cada `export` evalúa su valor y tempo y guarda una partitura `OutputScore`
independiente con la ruta ya decodificada. Cambios posteriores de variables
no alteran esa partitura. Se exportan notas, escalas, acordes, progresiones,
voicings y arpegios; una lista de acordes se interpreta como progresión.

La duración por defecto es una negra; cada nota de una escala dura una negra.
Los pasos de progresiones y arpegios conservan sus duraciones. El tempo por
defecto es 120 BPM y el instrumento es `piano`. MIDI exige 4–60000000 BPM;
`sheet` admite cualquier entero positivo. Los instrumentos usan la tabla de
`semanticInstrumentProgram`.

Las exportaciones se acumulan también dentro de bloques, ciclos y llamadas.
Solo al terminar sin errores se llama una vez a `outputWriteFiles` con el lote
completo. Un error previo libera las partituras sin crear archivos. Un fallo
de escritura revierte el lote. Destinos repetidos o existentes fallan sin
sobrescribir, incluso si aparecieron durante la publicación. Los logs ya
impresos permanecen en stdout. El contrato de escritura está en
`doc/stage-iii/salida.md`.

## Errores de ejecución

Un error imprime `Runtime error: ...` en stderr y termina con código 1. Los
mensajes terminan con punto. Memoria insuficiente devuelve código 2.

| Error | Mensaje sin el prefijo ni el punto final |
|-------|-----------------------------------------|
| División por cero | `division by zero` |
| Desborde de enteros | `integer overflow in A op B` |
| Índice de vector | `index N is out of range for a vector of length L` |
| Índice de escala | `index N is out of range for a scale of length 7` |
| Índice de progresión | `index N is out of range for a progression of length L` |
| Grado entero | `chord degree N must be between 1 and 7` |
| Transposición | `transposition is out of range` |
| Inversión, incluido salir del rango de alturas | `inversion is out of range` |
| Repetición negativa | `repetition count cannot be negative (N)` |
| Contexto estricto | `chord contains a note outside the strict key` |
| Modulación sin origen | `cannot modulate a progression without a known key` |
| Demasiadas notas | `chord has too many notes (maximum 16)` o `voice has too many notes` |
| Realización imposible | `voice has no SATB realization within the tessituras` |
| Voicing explícito de otro tamaño | `explicit voicing chords need four notes` |
| Validación SATB | `parallel fifths`, `parallel octaves`, `voice crossing` o `tessitura`, seguido de `at chord N` |
| Tempo | `tempo N is outside the export format's range` |
| Escritura | `cannot write 'ruta'`; agrega `(destination already exists or is repeated)` si corresponde |
| Más de 1000 llamadas | `procedure 'p' exceeds the maximum call depth of 1000` |
| Pila de C | `the program nests too deeply for the available stack` |
| Escritura de stdout | `cannot write the log output` |
| Memoria | `out of memory` (código 2) |

Cada `MusicStatus` se propaga: memoria conserva el código 2 y los valores
inválidos incluyen la operación que falló. Los errores que la semántica ya
conoce se rechazan antes de ejecutar. Para probar memoria se fuerzan fallos
de reserva; no se intenta agotar la memoria real con ASan.

## Tests

`test.sh` ejecuta las 104 aceptaciones en directorios temporales. Compara
stdout con `src/test/c/output/<caso>.out` (vacío si falta) y los archivos con
`src/test/c/artifacts/<caso>/`. Falla ante archivos inesperados o fixtures sin
caso. `generate-runtime-goldens.py` construye las referencias desde eventos
esperados sin ejecutar Cadence; la suite nunca las regenera automáticamente.

Los 151 rechazos exigen código 1 y ningún archivo. Los `runtime-*` exigen
`Runtime error:`, sin `Semantic error:`, y el mensaje concreto. Los rechazos
musicales encolan una exportación antes del error para comprobar el descarte.
Los ocho casos `runtime-strict-interpolation-*` comprueban strings y `log`,
con acordes, vectores de acordes y vectores de progresiones o voicings; un
acorde cromático invalida la lectura y descarta también la exportación pendiente.
La aceptación y el rechazo `runtime-strict-key-parameter` distinguen el paso
de un acorde cromático a un parámetro `key` estricto de su lectura en el cuerpo.
`runtime-loop-transpose-range`, antes `semantic-loop-unknown`, comprueba que
un `while (false)` deja B8 sin modificar y falla al transponer después.

Las pruebas cubren propiedades y comparaciones musicales, copias y retornos
por valor, todas las transformaciones, índices, contexto estricto, SATB,
partituras MIDI/LilyPond, tempo e instrumento, rutas con escapes y capturas
anteriores a una asignación. La integración de escalas conserva la comparación
MIDI byte a byte, errores de escritura y protección de destinos existentes.

También se prueba que `--check` no imprime ni exporta y que un error posterior
a `export`, dentro de bloques o procedimientos, no deja archivos y conserva
`log "antes"` en stdout. La recursión infinita exige el diagnóstico exacto del
límite de 1000 llamadas. `runtime-recursion-depth` exige que la cuenta desde
999 imprima cero, tanto directa como con dos `if` anidados. La prueba con
veinte bloques anidados exige la guarda de pila.

`runtime-value.c` limita el tamaño de `Value` y comprueba copias independientes
de acordes, escalas, progresiones dentro de vectores y los dos arreglos de un
voicing, además de liberación repetida.
`runtime-oom.c` fuerza por turno cada reserva directa de `malloc`, `calloc` y
`realloc` de una ejecución con procedimientos, copias musicales, validaciones
y nueve exportaciones. Exige código 2 y ningún archivo en cada fallo.

El binario y las pruebas de memoria usan AddressSanitizer. `test.sh` fija su
código de error en 86 y rechaza cualquier reporte: ni un fallo de memoria ni
una fuga pueden contarse como un rechazo esperado.
