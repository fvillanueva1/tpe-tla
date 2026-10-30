# Análisis semántico — avance inicial

El compilador recorre el AST después del análisis sintáctico y detiene la generación si falla la validación de nombres. Las variables son visibles después de su declaración; cada bloque crea un alcance y puede ocultar un nombre exterior, pero no redeclarar otro del mismo alcance. Los parámetros pertenecen al alcance del cuerpo de su procedimiento y la variable de `for` al de su cuerpo. Se comprueban también los nombres interpolados en strings.

Esta primera fase todavía no comprueba tipos, firmas de procedimientos ni las reglas musicales; aceptar un programa no garantiza su validez semántica completa. Las ubicaciones de origen no están disponibles en los nodos del AST, por lo que los errores aún no indican línea.

Se validan adem?s condiciones booleanas conocidas, l?mites enteros de `for`, operaciones l?gicas y comparaciones de enteros, notas e intervalos.

Se comprueba que el tempo, cuando aparece, sea entero y que `check` reciba un `voicing`, siempre que se conozca el tipo de la expresi?n.
