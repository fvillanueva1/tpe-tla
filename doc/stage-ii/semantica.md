# Analisis semantico de Cadence

El compilador ejecuta `analyzeSemantics` despues del parser y no invoca el generador cuando falla. Los nombres se resuelven desde el alcance mas interno: los bloques permiten ocultar nombres exteriores, pero rechazan redeclaraciones en el mismo alcance. La declaracion es visible despues de su inicializador; los parametros y la variable `for` pertenecen al alcance de su cuerpo.

Los procedimientos se definen solo a nivel global y se pueden llamar antes de su definicion. Se comprueban firma, argumentos, retornos y que todo camino de una funcion con retorno produzca un valor; los bucles no garantizan retorno. Las variables globales visibles dentro del cuerpo son las declaradas antes de la definicion.

## Tipos

Las expresiones tienen tipos explicitos, incluidos grado, duracion y calidad internos. Las listas homogeneas de acordes se aceptan como progresion o vector de acordes segun contexto; una lista vacia necesita contexto. `in` acepta nota en escala/acorde o lista de grados en tonalidad. Las propiedades e indices requieren tipos compatibles. Los operadores aritmeticos admiten enteros, transposicion por intervalo, concatenacion de progresiones y repeticion de progresion por entero. La comparacion se admite entre enteros, notas o intervalos del mismo tipo; igualdad y desigualdad tambien admiten calidades y modos.

Se comprueban tipos de condiciones, argumentos, retornos, construcciones musicales y `check`. `export` admite nota, escala, acorde, progresion o voicing; tempo entero positivo (de 4 a 60000000 BPM para MIDI, por su campo de tempo de 24 bits en microsegundos por negra) y los instrumentos nombrados en la tabla de `semanticInstrumentProgram`, cuyo orden es el de General MIDI; `piano` identifica el primer programa. `semanticInstrumentProgram` devuelve numeros MIDI de 0 a 127 o -1 para un identificador invalido, de modo que el generador use la misma tabla. Tempo e instrumento pueden omitirse. Se evaluan expresiones aritmeticas constantes sin variables para rechazar division por cero, grados fuera de 1 a 7, indices fuera de rango en listas literales o escalas, inversiones fuera del tamano de acordes literales, repeticiones negativas y tempos fuera de rango. Los indices son de base cero. Una nota literal usada como valor requiere octava y altura entre C0 y B8; las tonicas de tonalidad se escriben sin octava. Tambien se valida la transposicion de nota e intervalo literales. El desborde de enteros no se evalua estaticamente y requiere comprobacion durante la ejecucion.

## Limites e integracion

Los valores calculados mediante variables, procedimientos o ciclos no se conocen en esta fase: el runtime debera verificar rangos, incluyendo tempo, inversion y transposicion. La validacion `strict` de valores dinamicos, la realizacion SATB para expresiones calculadas sigue pendiente de la evaluacion musical. Los escenarios de cruce y tesitura 58 y 59 permanecen en `src/test/c/pending/`. El generador ya admite un programa de dos sentencias con una tonalidad literal y la exportacion MIDI de su escala. El resto de la generacion sigue pendiente. El AST no conserva posiciones para informar numeros de linea.

El rechazo semantico usa codigo de salida 1 y un mensaje `Semantic error:`. La suite verifica ese diagnostico para los casos semanticos, evitando que un error sintactico o un fallo del proceso cuente como rechazo correcto.

La compilacion con avisos estrictos de `Semantic.c` no produce avisos propios; `Environment.h` conserva un aviso preexistente por un calificador en el tipo de retorno.

El tipo `duration` permite declarar duraciones, vectores de duraciones y parametros o retornos de procedimientos; `arpeggiate` exige una duracion.

Si no se puede reservar memoria para un simbolo, el analizador devuelve `OUT_OF_MEMORY` (codigo 2), distinto de un rechazo semantico (codigo 1).

## Validaciones musicales para integracion

`MusicValidation.h` ofrece funciones puras sobre alturas MIDI (C4 = 60) para pertenencia a tonalidad/modo, tesitura, cruce de voces y quintas u octavas paralelas. Los vectores SATB se ordenan soprano, contralto, tenor y bajo. Los rangos inclusivos elegidos son soprano C4-A5, contralto F3-D5, tenor C3-A4 y bajo E2-E4. Voces en unisono no se consideran cruzadas. Para paralelas, ambas voces deben moverse en el mismo sentido; se consideran intervalos compuestos equivalentes y las octavas requieren distancia minima de una octava en ambos acordes.

Estas funciones se prueban con datos conocidos. El analizador utiliza la pertenencia tonal para `strict` conocido y verifica SATB y `check` para progresiones literales conocidas. Los escenarios `pending/58` y `59` siguen sin activarse. `musicInKey` recibe notas MIDI de C0 a B8, tonica como clase de altura de 0 a 11 y un modo; admite alteraciones enarmonicas por clase de altura.

`musicCheckVoicing` valida una secuencia completa: siempre comprueba tesitura y aplica las reglas indicadas por la mascara `CheckRule`. Devuelve el primer tipo de violacion y, si se solicita, el indice del acorde donde aparece (para paralelas, el segundo acorde). Una secuencia vacia es valida; mascaras desconocidas y datos nulos no vacios son errores de entrada. Las funciones se invocan desde el analizador para progresiones literales conocidas.

`musicScale` calcula las siete alturas MIDI ascendentes de una tonalidad y modo, con una tonica que incluye octava. Devuelve fallo si la escala excede C0-B8 o la entrada es invalida; no altera la salida en caso de error. La validacion de pertenencia reutiliza esas alturas. El generador inicial usa la tonica en octava 4, notas negras y tempo 120 por defecto.

## Contexto de tonalidad estricta

Una declaracion de tonalidad escalar establece el contexto tonal del alcance a partir de esa declaracion; la declaracion mas reciente prevalece y los bloques internos pueden definir otro contexto. Si es `strict` y se conoce su tonalidad literal, las notas literales y las variables inicializadas con una nota literal sin asignaciones posteriores en todo el programa deben pertenecer a ella. La busqueda de asignaciones incluye todos los bloques y procedimientos, incluso con sombreado, para no asumir valores mutables. Una nueva tonalidad no estricta reemplaza el contexto. El analizador deja de asumir un contexto conocido despues de una asignacion a tonalidad, una llamada o una estructura de control; tampoco presume el contexto global al analizar un procedimiento. Las demas notas y tonalidades calculadas requieren validacion en ejecucion. Esta regla explicita resuelve la ausencia de una sintaxis que asocie un acorde explicito con una tonalidad.

## Realizacion SATB sobre acordes conocidos

`musicRealizeSATB` recibe una mascara de clases de altura por acorde y produce una distribucion SATB con las tesituras documentadas, sin cruce y que contiene todas las clases de cada acorde. Considera todas las distribuciones posibles y elige la secuencia con menor suma de desplazamientos absolutos entre voces mediante programacion dinamica. En empate, elige la primera distribucion segun el recorrido ascendente. La salida solo se escribe si toda la secuencia se pudo resolver.

Una mascara vacia o que no admite distribucion devuelve `MUSIC_VOICE_INVALID_CHORD`; los errores de memoria devuelven `MUSIC_VOICE_NO_MEMORY`. Actualmente solo pueden distribuirse acordes con hasta cuatro clases distintas, pues no esta definida una regla de omision para acordes de novena. El analizador la utiliza para `voice` con progresiones literales conocidas.

## Comprobacion estatica de `voice` y `check`

El analizador realiza y verifica progresiones literales de acordes por notas en el alcance global. Se admiten variables de nota, acorde, progresion y voicing cuando su inicializador es literal y no se asigna al mismo nombre en ninguna parte del programa. `check` aplica solamente las reglas solicitadas sobre esa realizacion; las tesituras se comprueban siempre. Casos con mas de cuatro clases por acorde, grados, valores mutables o expresiones dentro de bloques y procedimientos quedan para la evaluacion en ejecucion. Los escenarios 56 y 57 usan acordes que fuerzan las paralelas; 58 y 59 siguen pendientes porque la realizacion escogida ya evita cruces y tesituras invalidas.

Los errores SATB identifican la regla y el acorde (numeracion desde uno).
