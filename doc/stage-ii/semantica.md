# Analisis semantico de Cadence

El compilador ejecuta `analyzeSemantics` despues del parser y no invoca el generador cuando falla. Los nombres se resuelven desde el alcance mas interno: los bloques permiten ocultar nombres exteriores, pero rechazan redeclaraciones en el mismo alcance. La declaracion es visible despues de su inicializador; los parametros y la variable `for` pertenecen al alcance de su cuerpo.

Los procedimientos se definen solo a nivel global y solo se pueden llamar despues de su definicion o desde su propio cuerpo; no hay recursion mutua. Asi un cuerpo nunca lee una global que todavia no se declaro al momento de la llamada. Se comprueban firma, argumentos, retornos y que todo camino de una funcion con retorno produzca un valor; los bucles no garantizan retorno. Las variables globales visibles dentro del cuerpo son las declaradas antes de la definicion.

## Tipos

Las expresiones tienen tipos explicitos, incluidos grado, duracion y calidad internos. Las listas homogeneas de acordes se aceptan como progresion, vector de acordes o voicing explicito segun contexto; en un voicing explicito cada acorde da las notas de soprano, contralto, tenor y bajo, y los acordes literales deben tener cuatro notas; una lista vacia necesita contexto. `in` acepta nota en escala/acorde o lista de grados en tonalidad. Las propiedades e indices requieren tipos compatibles. Los operadores aritmeticos admiten enteros, transposicion por intervalo, concatenacion de progresiones y repeticion de progresion por entero. La comparacion se admite entre enteros, notas o intervalos del mismo tipo; igualdad y desigualdad tambien admiten calidades y modos.

Se comprueban tipos de condiciones, argumentos, retornos, construcciones musicales y `check`. `export` admite nota, escala, acorde, progresion o voicing; tempo entero positivo (de 4 a 60000000 BPM para MIDI, por su campo de tempo de 24 bits en microsegundos por negra) y los instrumentos nombrados en la tabla de `semanticInstrumentProgram`, cuyo orden es el de General MIDI; `piano` identifica el primer programa. `semanticInstrumentProgram` devuelve numeros MIDI de 0 a 127 o -1 para un identificador invalido, de modo que el generador use la misma tabla. Tempo e instrumento pueden omitirse. Se evaluan expresiones aritmeticas constantes y valores escalares literales conocidos por declaraciones o asignaciones lineales para rechazar division por cero, grados fuera de 1 a 7, indices fuera de rango en listas literales o escalas, inversiones fuera del tamano de acordes literales, repeticiones negativas y tempos fuera de rango. Los indices son de base cero. Una nota literal usada como valor requiere octava y altura entre C0 y B8; las tonicas de tonalidad se escriben sin octava. Tambien se valida la transposicion de notas e intervalos literales o guardados en variables cuyo valor actual es conocido. El desborde de enteros no se evalua estaticamente y requiere comprobacion durante la ejecucion.

## Limites e integracion

Los valores calculados mediante variables, procedimientos o ciclos no se conocen en esta fase: el runtime debera verificar rangos, incluyendo tempo, inversion y transposicion. La validacion `strict` de valores dinamicos, la realizacion SATB para expresiones calculadas sigue pendiente de la evaluacion musical. Los escenarios de cruce y tesitura 58 y 59 permanecen en `src/test/c/pending/`. El generador ya admite una tonalidad literal y la exportacion MIDI de su escala, directamente o mediante una declaracion de escala intermedia. El resto de la generacion sigue pendiente. El AST no conserva posiciones para informar numeros de linea.

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

Una declaracion de tonalidad escalar establece el contexto tonal del alcance a partir de esa declaracion; la declaracion mas reciente prevalece y los bloques internos pueden definir otro contexto. Si es `strict` y se conoce su tonalidad literal, las notas literales y las variables cuyo valor actual literal es conocido deben pertenecer a ella. El analizador actualiza valores escalares en asignaciones lineales y los olvida ante ramas, ciclos y llamadas para evitar suposiciones sobre el flujo. Una nueva tonalidad no estricta reemplaza el contexto. El analizador actualiza el contexto si la tonalidad vigente recibe un valor literal conocido; una asignacion a otra tonalidad no lo modifica. Una llamada o una estructura de control vuelve desconocido el contexto; tampoco presume el contexto global al analizar un procedimiento. Las demas notas y tonalidades calculadas requieren validacion en ejecucion. Esta regla explicita resuelve la ausencia de una sintaxis que asocie un acorde explicito con una tonalidad.

## Realizacion SATB sobre acordes conocidos

`musicRealizeSATB` recibe una mascara de clases de altura por acorde y produce una distribucion SATB con las tesituras documentadas, sin cruce y que contiene todas las clases de cada acorde. Considera todas las distribuciones posibles y elige la secuencia con menor suma de desplazamientos absolutos entre voces mediante programacion dinamica. En empate, prefiere la distribucion inicial mas cercana al centro de las tesituras; si aun hay empate, usa el recorrido ascendente. La salida solo se escribe si toda la secuencia se pudo resolver.

Una mascara vacia o que no admite distribucion devuelve `MUSIC_VOICE_INVALID_CHORD`; los errores de memoria devuelven `MUSIC_VOICE_NO_MEMORY`. Actualmente solo pueden distribuirse acordes con hasta cuatro clases distintas, pues no esta definida una regla de omision para acordes de novena. El analizador la utiliza para `voice` con progresiones literales conocidas.

## Comprobacion estatica de `voice` y `check`

El analizador realiza y verifica progresiones literales de acordes por notas en el alcance global. Se admiten variables de nota, acorde, progresion y voicing cuando su inicializador es literal y no se asigna al mismo nombre en ninguna parte del programa. `check` aplica solamente las reglas solicitadas sobre esa realizacion; las tesituras se comprueban siempre. Tambien se admiten progresiones por grados y acordes de triada o septima construidos a partir de tonalidades y grados literales o variables inmutables conocidas. Casos con mas de cuatro clases por acorde, valores mutables o expresiones dentro de bloques y procedimientos quedan para la evaluacion en ejecucion. Los escenarios 56 y 57 usan acordes que fuerzan las paralelas; 58 y 59 siguen pendientes porque la realizacion escogida ya evita cruces y tesituras invalidas; se probaran con voicings explicitos cuando el runtime los evalue. Por ahora el analizador solo verifica que los acordes literales de un voicing explicito tengan cuatro notas.

Los errores SATB identifican la regla y el acorde (numeracion desde uno).

Para reconstruir valores compuestos usados en SATB se exige ademas que sus variables no reciban asignaciones en ningun bloque o procedimiento; esto evita reevaluar un acorde con valores distintos a los que tenia cuando se creo.

Los errores de tipos informan el tipo esperado y el recibido, o el operador y los dos operandos incompatibles.

Por defecto el compilador devuelve error si el generador todavia no soporta el programa; `--check` ejecuta solo el frontend y la semantica y no crea archivos. La suite usa ese modo para casos de aceptacion semantica y prueba la generacion por separado.

Los indices constantes tambien se validan contra la longitud de vectores inicializados con listas literales cuyo nombre no recibe asignaciones en el programa. Si el vector puede cambiar, su longitud se comprueba en ejecucion.

Los documentos de catedra sobre scopes, sistema de tipos y tabla de simbolos estan disponibles en `doc/consignas/`. El analizador resuelve nombres desde el alcance mas interno y verifica tipos sobre el AST, de acuerdo con esos materiales.

## Entrega a modelo musical y runtime

`analyzeSemantics(Program *)` valida el AST y devuelve `SUCCEEDED`, `FAILED` o `OUT_OF_MEMORY`; el punto de entrada no llama al generador si falla. Los simbolos y alcances son temporales y se liberan al terminar, por lo que el backend debe gestionar sus propios valores y entornos de ejecucion. `semanticInstrumentProgram` comparte el mapeo de instrumentos.

Felipe debe completar las operaciones musicales y definir la realizacion de acordes con mas de cuatro clases; los casos 58 y 59 no prueban los errores que nombran y necesitan ejemplos concluyentes. Eduardo debe implementar la ejecucion y comprobar valores no resueltos estaticamente: limites de indices y grados, division por cero y desborde, transposicion, inversion, repeticion, tempo, `strict`, realizacion de `voice` y reglas de `check`. Tambien debe propagar errores de ejecucion y de escritura. La aceptacion con `--check` no significa que el generador ya soporte el programa.

Las validaciones musicales conocidas no cubren todas las expresiones: `voice` y `check` se evaluan estaticamente solo en los casos descritos arriba; los demas quedan para el runtime. El AST no conserva ubicaciones para diagnosticos por linea.
