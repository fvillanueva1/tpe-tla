# Analisis semantico de Cadence

El compilador ejecuta `analyzeSemantics` despues del parser y no invoca el generador cuando falla. Los nombres se resuelven desde el alcance mas interno: los bloques permiten ocultar nombres exteriores, pero rechazan redeclaraciones en el mismo alcance. La declaracion es visible despues de su inicializador; los parametros y la variable `for` pertenecen al alcance de su cuerpo.

Los procedimientos se definen solo a nivel global y se pueden llamar antes de su definicion. Se comprueban firma, argumentos, retornos y que todo camino de una funcion con retorno produzca un valor; los bucles no garantizan retorno. Las variables globales visibles dentro del cuerpo son las declaradas antes de la definicion.

## Tipos

Las expresiones tienen tipos explicitos, incluidos grado, duracion y calidad internos. Las listas homogeneas de acordes se aceptan como progresion o vector de acordes segun contexto; una lista vacia necesita contexto. `in` acepta nota en escala/acorde o lista de grados en tonalidad. Las propiedades e indices requieren tipos compatibles. Los operadores aritmeticos admiten enteros, transposicion por intervalo, concatenacion de progresiones y repeticion de progresion por entero. La comparacion se admite entre enteros, notas o intervalos del mismo tipo; igualdad y desigualdad tambien admiten calidades y modos.

Se comprueban tipos de condiciones, argumentos, retornos, construcciones musicales y `check`. `export` admite nota, escala, acorde, progresion o voicing; tempo entero de 1 a 1000 BPM y los instrumentos nombrados en la tabla de `validInstrument`, cuyo orden es el de General MIDI; `piano` identifica el primer programa. Tempo e instrumento pueden omitirse. Se validan rangos de transposicion de nota literal y de inversion de un acorde literal cuando sus operandos son literales.

## Limites e integracion

Los valores calculados mediante variables, procedimientos o ciclos no se conocen en esta fase: el runtime debera verificar rangos, incluyendo tempo, inversion y transposicion. La validacion `strict`, realizacion SATB, tesituras y contrapunto necesitan el modelo musical y se mantienen pendientes, junto con sus casos en `src/test/c/pending/`. El generador aun no emite artefactos: no hay integracion de escala a archivo en este estado. El AST no conserva posiciones para informar numeros de linea.

El rechazo semantico usa codigo de salida 1 y un mensaje `Semantic error:`. La suite verifica ese diagnostico para los casos semanticos, evitando que un error sintactico o un fallo del proceso cuente como rechazo correcto.
