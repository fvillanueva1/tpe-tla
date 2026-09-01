# Cadence

Cadence es un lenguaje de dominio específico para describir armonía tonal a
partir de tonalidades, grados, acordes y progresiones. El compilador, escrito
en C con Flex y Bison, derivará las notas concretas, validará reglas musicales
y generará artefactos reproducibles.

## Estado del proyecto

El proyecto se encuentra en **Stage I: Diseño**. La especificación final está
disponible como [PDF de entrega](<doc/Cadence%20-%20Stage%20I.pdf>).

## Documentación

- [Especificación final de Stage I](<doc/Cadence%20-%20Stage%20I.pdf>)
- [Consignas y material de la cátedra](docs/consignas/)

La carpeta `doc/` contiene los entregables. `docs/` contiene material de
referencia.

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
