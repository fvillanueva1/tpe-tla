# Runtime de Cadence

El runtime vive en `src/main/c/backend/runtime/`. Por ahora define los
valores de ejecución y su entorno; el evaluador se agrega después.

## Módulos

- `Value.h` / `Value.c`: el valor de ejecución y el entorno. Un `Value` lleva
  su `DataType` (o el de sus elementos, si es vector) y una unión con el entero,
  el booleano, la duración, el string ya decodificado o el arreglo de elementos.
  Los tipos musicales se agregan cuando `backend/music/Music.h` defina su
  representación. Las copias son profundas.
- `RuntimeScope`: lista de variables con puntero al alcance padre. La búsqueda
  va del alcance más interno al global, con las mismas reglas de ocultamiento
  que la semántica (`doc/stage-ii/semantica.md`).
