# Analisis semantico: estado actual

El compilador valida nombres y alcances tras el parser, y detiene la generacion si encuentra un error. Las declaraciones son visibles despues de su inicializacion. Los bloques permiten ocultar nombres exteriores y rechazan redeclaraciones en el mismo alcance; los parametros y la variable de for pertenecen al alcance de su cuerpo.

Se comprueban parcialmente tipos de declaraciones, asignaciones, aritmetica, condiciones, comparaciones, check y tempo. Las expresiones cuyo tipo aun no se calcula quedan pendientes de validacion.

Los procedimientos se registran a nivel global y pueden llamarse antes o despues de su definicion. Se rechazan definiciones duplicadas o anidadas, llamadas a nombres inexistentes, cantidad incorrecta de argumentos y tipos incompatibles conocidos. Un return debe estar dentro de un procedimiento y coincidir con su tipo de retorno cuando se conoce el tipo de la expresion. No se garantiza aun que todos los caminos devuelvan un valor.

Faltan reglas de tipos y validaciones musicales. El AST no conserva ubicaciones de origen para diagnosticos con linea. Aceptar un programa no garantiza aun su validez semantica completa.
