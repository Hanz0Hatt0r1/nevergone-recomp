#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
: "${NEVERGONE_LIB_DIR:?Set NEVERGONE_LIB_DIR to the original ARM32 libraries directory}"
export NEVERGONE_LIB_DIR
export NEVERGONE_DEVICE_LIB_DIR="${NEVERGONE_DEVICE_LIB_DIR:-$PWD/device-libs}"
export NEVERGONE_DEVICE_RUNTIME_DIR="${NEVERGONE_DEVICE_RUNTIME_DIR:-$PWD/device-runtime}"
relr_dirs=(--directory "$NEVERGONE_LIB_DIR" --directory "$NEVERGONE_DEVICE_LIB_DIR")
use_runtime=true
for arg in "$@"; do
    case "$arg" in
        --sdk23-runtime) use_runtime=false ;;
        --device-runtime) use_runtime=true ;;
    esac
done
if "$use_runtime"; then relr_dirs+=(--directory "$NEVERGONE_DEVICE_RUNTIME_DIR"); fi
python3 relr_plan.py "${relr_dirs[@]}" --output target/relr-plan.json
mvn -q -DskipTests package org.apache.maven.plugins:maven-dependency-plugin:3.8.1:build-classpath -Dmdep.outputFile=target/classpath.txt
exec java -cp "target/classes:$(cat target/classpath.txt)" nevergone.NevergoneHarness "$@"
