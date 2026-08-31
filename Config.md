# FF777NavStudio
Studio d'édition de données de navigation pour le Boeing 777 (FlightFactor) basé sur Qt 6. 


## Prérequis
- **CMake ≥ 3.20**
- **Qt 6.10+** (modules Core, Widgets, Xml, Sql)
  - Téléchargé depuis [qt.io](https://www.qt.io/download) (emplacement par défaut '/Applications/Qt/...' sur macOS) ou installé via Homebrew : 'brew install qt'
- Un compilateur C++17 (clang/gcc/MSVC)


## Compiler et lancer dans le terminal
```bash
# Compiler (config CMake + build + bundle .app sur macOS)
./build.sh

# Compiler et Lancer l'application
./buildAndRun.sh
```
Qt est **auto-détecté** pour être intégré (le framework) à l'application finale. 
Pour forcer une version précise de Qt :
```bash
QT_PATH=/Applications/Qt/6.11.0/macos ./build.sh
```
Nettoyer la compilation :
```bash
./build.sh clean
```

Une fois compilé, le projet devient **indépendant de Qt Creator** et se lance sur une machine où Qt n'est pas installé.


## Autres configurations utiles

```bash
# Build Debug et choix de l'architecture
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_OSX_ARCHITECTURES="$(uname -m)"
cmake --build build --parallel

# Build universel macOS (les deux architectures)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"
```


## Outils générés

| Cible               | Description                                          |
|---------------------|------------------------------------------------------|
| FF777NavStudio    | Application GUI                                        |
| FF777NavStudioCli | Outil en ligne de commande : 'validate <dossier>' / 'export <src> <dst>' |


## Alternative de compilation
Si vous disposez d'une installation de Qt Creator, l'application peut évidemment être compilé et exécuté directement dans l'IDE.
Dans ce cas, contrairement à la compilation dans le terminal (scripts vus plus haut), l'application obtenue ne sera pas indépendante de Qt et ne pourra tourner que sur une machine où ce dernier est déjà installé


## Structure du projet
```
src/
  cli/        Point d'entrée CLI
  model/      Entités du domaine
  reader/     Lecture des fichiers de navigation
  writer/     Écriture des fichiers de navigation
  validator/  Validation des données
  ui/         Application Qt Widgets (éditeurs, tableaux)
  parser/     Décodage/encodage des bases nav1.db (FlightFactor)
  tools/      Pipelines de conversion
build/        Artefacts de compilation (créé par build.sh)
```