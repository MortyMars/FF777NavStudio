#!/usr/bin/env bash
# run.sh — Compile puis lance FF777NavStudio sans Qt Creator.
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
BIN_DIR="${PROJECT_DIR}/build2"

"${PROJECT_DIR}/build.sh"

if [[ -d "${BIN_DIR}/FF777NavStudio.app" ]]; then
    open "${BIN_DIR}/FF777NavStudio.app"
else
    "${BIN_DIR}/FF777NavStudio"
fi