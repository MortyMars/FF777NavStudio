#!/usr/bin/env bash
# ----------------------------------------------------------------------------
# build.sh — Compile FF777NavStudio sans Qt Creator.
#
# Usage :
#   ./build.sh                # config + compilation + bundle .app
#   ./build.sh clean          # supprime le dossier de build
#   QT_PATH=/chemin/vers/qt ./build.sh   # force un Qt précis
#
# Le résultat se trouve dans build/FF777NavStudio.app (macOS) ou
# build/FF777NavStudio (Linux/Windows).
# ----------------------------------------------------------------------------
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${PROJECT_DIR}/build2"
JOBS="$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)"

# --- Détection de Qt ---------------------------------------------------------
detect_qt() {
    if [[ -n "${QT_PATH:-}" ]]; then
        QT_PREFIX="$QT_PATH"
    elif [[ -n "${QT_INSTALL_PREFIX:-}" ]]; then
        QT_PREFIX="$QT_INSTALL_PREFIX"
    else
        # 1. Recherche dynamique et tri par version (la plus haute en premier)
        local app_qt_cands
        app_qt_cands=$(ls -d /Applications/Qt/6.*/macos 2>/dev/null | sort -V -r || true)
        
        # 2. Liste globale ordonnée des candidats (priorité aux versions officielles, puis Homebrew)
        IFS=$'\n' read -r -d '' -a candidates <<EOF || true
$app_qt_cands
/opt/homebrew/opt/qt
/usr/local/opt/qt
EOF

        for cand in "${candidates[@]}"; do
            # Nettoyage des lignes vides potentielles
            [[ -z "$cand" ]] && continue
            
            if [[ -d "${cand}/lib/cmake/Qt6" ]]; then
                QT_PREFIX="$cand"
                break
            fi
        done
    fi

    if [[ -z "${QT_PREFIX:-}" ]]; then
        echo "ERREUR : Qt 6 introuvable." >&2
        echo "Installez Qt (https://qt.io) ou configurez QT_PATH." >&2
        exit 1
    fi
    CMAKE_PREFIX_ARGS=( -DCMAKE_PREFIX_PATH="${QT_PREFIX}" )
    echo ">> Qt utilisée : ${QT_PREFIX}"
}



configure_and_build() {
    cmake -S "${PROJECT_DIR}" -B "${BUILD_DIR}" \
        -DCMAKE_BUILD_TYPE=Release \
        "${CMAKE_PREFIX_ARGS[@]}" \
        -DCMAKE_OSX_ARCHITECTURES="$(uname -m)"
    cmake --build "${BUILD_DIR}" --parallel "${JOBS}"
}

deploy_macos() {
    local macdeployqt="${QT_PREFIX}/bin/macdeployqt"
    if [[ -x "${macdeployqt}" && -d "${BUILD_DIR}/FF777NavStudio.app" ]]; then
        echo ">> Création du bundle autonome FF777NavStudio.app ..."
        # 1. On lance macdeployqt normalement (il va râler sur Postgres/ODBC mais faire le reste du travail)
        "${macdeployqt}" "${BUILD_DIR}/FF777NavStudio.app" -always-overwrite -verbose=1
        
        # 2. Nettoyage chirurgical APRÈS déploiement : on supprime les pilotes manquants/inutiles
        #    mais on préserve libqsqlite.dylib !
        echo ">> Nettoyage chirurgical des pilotes SQL inutilisés (Postgres, ODBC, Mimer)..."
        local sql_plugins_dir="${BUILD_DIR}/FF777NavStudio.app/Contents/PlugIns/sqldrivers"
        
        if [[ -d "${sql_plugins_dir}" ]]; then
            rm -f "${sql_plugins_dir}/libqsqlodbc.dylib"
            rm -f "${sql_plugins_dir}/libqsqlpsql.dylib"
            rm -f "${sql_plugins_dir}/libqsqlmimer.dylib"
            # Optionnel : si mysql n'est pas utilisé, vous pouvez aussi le retirer
            rm -f "${sql_plugins_dir}/libqsqlmysql.dylib" 
        fi
        echo ">> Nettoyage SQL terminé. Pilote SQLite préservé."
    fi
}

# ---------------------------------------------------------
if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
    sed -n '2,12p' "$0"
    exit 0
fi

if [[ "${1:-}" == "clean" ]]; then
    rm -rf "${BUILD_DIR}"
    echo ">> ${BUILD_DIR} supprimé."
    exit 0
fi

detect_qt
configure_and_build

if [[ "$(uname -s)" == "Darwin" ]]; then
    deploy_macos
    echo ""
    echo ">> Terminé. Lancez l'application :  open ${BUILD_DIR}/FF777NavStudio.app"
else
    echo ""
    echo ">> Terminé. Lancez l'application :  ${BUILD_DIR}/FF777NavStudio"
fi