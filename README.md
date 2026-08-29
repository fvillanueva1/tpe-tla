# Cadence

Cadence es un lenguaje de dominio específico para describir armonía tonal a
partir de tonalidades, grados, acordes y progresiones. El compilador, escrito
en C con Flex y Bison, derivará las notas concretas, validará reglas musicales
y generará artefactos reproducibles.

## Estado del proyecto

El proyecto se encuentra en **Stage I: Diseño**. La primera versión de la
especificación está disponible como [PDF de entrega](<doc/Cadence%20-%20Especificaci%C3%B3n%20Stage%20I.pdf>).
Es un documento de trabajo: se revisará antes de la entrega definitiva.

La fuente editable de la especificación está en
[docs/stage-i/cadence-especificacion.md](docs/stage-i/cadence-especificacion.md).

## Documentación

- [Especificación Stage I](docs/stage-i/cadence-especificacion.md)
- [Consignas y material de la cátedra](docs/consignas/)
- [PDF de la primera iteración](<doc/Cadence%20-%20Especificaci%C3%B3n%20Stage%20I.pdf>)

La carpeta `doc/` contiene exclusivamente los entregables. `docs/` contiene
fuentes de trabajo y material de referencia.

## Desarrollo

El proyecto base se ejecuta dentro de Docker Compose. Se necesita
[Docker](https://www.docker.com/).

```bash
docker compose run --rm compiler
```

Desde el contenedor se pueden usar los scripts provistos:

```bash
src/main/bash/build.sh
src/main/bash/run.sh <programa>
src/main/bash/test.sh
```

Las entregas se realizan sobre la rama `development`.
