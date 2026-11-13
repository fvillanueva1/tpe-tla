#! /bin/bash
# Corre programas Cadence de demo/. Sin argumentos corre todos los .cad.
# Cada programa escribe en salida/<nombre>/, que se vacía antes de correrlo
# porque el compilador nunca sobrescribe archivos.
set -uo pipefail
cd "$(dirname "$0")"
[ -x ../.build/Flex-Bison-Compiler ] || bash ../src/main/bash/build.sh
[ $# -gt 0 ] || set -- *.cad
for program in "$@"; do
    name=$(basename "$program" .cad)
    rm -rf "salida/$name" && mkdir -p "salida/$name"
    echo "=== $name"
    (cd "salida/$name" && ../../../.build/Flex-Bison-Compiler < "../../$program")
    echo "--- código $?; archivos: $(ls "salida/$name" | tr '\n' ' ')"
done
