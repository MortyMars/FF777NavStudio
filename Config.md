# Mode opératoire pour compiler FF777NavStudio

## 1. Compilation sous macOS

#### Prérequis
- **CMake ≥ 3.20**
- **Qt 6.10+** (modules Core, Widgets, Xml, Sql), téléchargé depuis [qt.io](https://www.qt.io/download) (emplacement par défaut '/Applications/Qt/...' sur macOS) ou installé via Homebrew : 'brew install qt'
- Un compilateur C++17 (clang/gcc/MSVC)

#### Compiler et lancer dans le terminal

Le script à utiliser pour macOS est '**build.sh**'.

Dans le Terminal, à la racine du projet, taper :
```bash
# Compiler (config CMake + build + bundle .app sur macOS)
./build.sh

# Compiler et Lancer l'application, en une seule opération
./buildAndRun.sh
```
Qt est **auto-détecté**, son framework est intégré à l'application finale. 
Pour forcer une version précise de Qt :
```bash
QT_PATH=/Applications/Qt/6.11.0/macos ./build.sh
```
Nettoyer la compilation :
```bash
./build.sh clean
```
Une fois compilé, le projet devient **indépendant de Qt Creator** et se lance même sur une machine où Qt n'est pas installé.

#### Autres commandes de configurations utiles

```bash
# Build Debug et choix de l'architecture
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_OSX_ARCHITECTURES="$(uname -m)"
cmake --build build --parallel

# Build universel macOS (les deux architectures)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"
```

## 2. Compilation sous Windows

#### Prérequis

- **Qt 6.10+** avec le composant **MinGW 64-bit** (MinGW 13.x 64 bits) qui fournit le compilateur
- Si Qt est déjà installé, le mettre éventuellement à jour et vérifier que le pack **MinGW 13.x 64-bit** est inclus
- Optionnel mais recommandé : **Developer and Designer Tools → CMake** (sinon un CMake système dans le PATH est requis)

Ces composants s'installent via le [Qt Online Installer](https://www.qt.io/download-qt-installer) ou le '**Maintenance Tool**' si Qt est déjà installé.

#### Compilation dans PowerShell

Le script à utiliser pour Windows est **build.ps1**.

Dans PowerShell, à la racine du projet, taper :
```powershell
.\build.ps1
```

Le script détecte automatiquement l'installation Qt et compile le projet. 
L'exécutable final `FF777NavStudio` se trouve à la racine du dossier `build2`.

#### Emplacement d'installation non standard

Par défaut, le script recherche Qt sous `C:\Qt` (emplacement proposé par défaut par l'installeur). **Si Qt est installé ailleurs** (par exemple `D:\dev\Qt`, ou une installation partagée en entreprise), la détection automatique échouera avec un message indiquant que Qt n'a pas été trouvé.
Dans ce cas, indiquer le chemin exact du kit `mingw_64` via la variable d'environnement `QT_PATH` avant de lancer le script :

```powershell
$env:QT_PATH = "D:\dev\Qt\6.11.0\mingw_64" (pour reprendre l'exemple)
.\build.ps1
```
Cette variable pointe directement vers le dossier du kit (celui qui contient `bin\`, `lib\`, etc.), pas vers la racine `Qt\` ni vers le dossier de version.

> Note : `$env:QT_PATH` n'est valable que pour la session PowerShell en cours. Pour le rendre permanent, l'ajouter aux variables d'environnement utilisateur (Paramètres système → Variables d'environnement) ou placer la ligne `$env:QT_PATH = "..."` dans le profil PowerShell.

#### Cas particulier : kit Qt sans dossier `Tools` associé

Le script déduit l'emplacement du compilateur MinGW et de CMake/Ninja à partir de `QT_PATH` en remontant deux niveaux de dossiers (ex. `...\6.11.0\mingw_64` → `...\Tools`). Cela fonctionne automatiquement pour toute installation faite via l'installeur Qt, où `Tools\` se trouve à côté du dossier de version — même si l'ensemble est déplacé ou installé hors de `C:\Qt`.

En revanche, si votre kit `mingw_64` provient d'une installation "manuelle" ou reconstituée (dossier copié isolément, sans son `Tools\` d'origine), le script ne trouvera pas le compilateur ni CMake au bon endroit. Dans ce cas :
- s'assurer que `gcc.exe` (MinGW 64-bit, idéalement la même version que celle utilisée pour compiler Qt) et `cmake.exe` sont déjà accessibles dans le `PATH` système ;
- le script les utilisera alors directement, sans passer par `Tools\`.

Si ni le dossier `Tools\` attendu ni un compilateur/CMake dans le `PATH` ne sont trouvés, le script s'arrête avec un message d'erreur explicite plutôt que d'échouer plus loin avec une erreur CMake peu claire.


## 3. Outils générés par la compilation

La compilation génère les exécutables GUI et CLI de l'application :
| Cible               | Description                                                            |
|---------------------|------------------------------------------------------------------------|
| FF777NavStudio      | Application GUI   (Application principale)                             |
| FF777NavStudioCli   | Outil en ligne de commande : 'validate <dossier>' / 'export <src> <dst>|

## 4. Alternative de compilation (macOS et Windows)

La compilation du programme exigeant la présence (ou l'installation) de Qt Creator sur la machine macOs ou Windows, il est évidemment possible de compiler et d'exécuter l'application directement dans l'IDE.
Dans ce cas, contrairement à la compilation dans le Terminal ou Power Shell (cf. scripts plus haut), l'application obtenue ne sera pas indépendante de Qt et ne pourra fonctionner que sur une machine où ce dernier est déjà installé.


## 5. Structure du projet

Le projet est structuré selon l'arborescence suivante :
```
assets/         Icone application
icones/         Autres icones
src/            Dossier des sources
  cli/          Point d'entrée appli cli
  convertion/   Mise en forme des données
  extract/      Extraction des fichiers texte
  model/        Entités et données métier
  parser/       Décodage/encodage de la base nav1.db
  persistence/  Sauvegarde des données
  reader/       Lecture des fichiers de navigation
  repository/   Réindex des données des projets
  tools/        Pipelines de conversion
  ui/           Interfaces
  userdata/     Saisies utilisateur
  validator/    Validation des données
  worldindex/   Lecture des index de rattachement
  writer/       Écriture des fichiers de navigation
build/          Dossier de build du projet
```
## 6. Base de données générée par l'application

L'application stocke les données saisies pour les différents projets créés, dans une base de données locale '**projects.sqlite**' positionnée en :
- sous macOS   : '~/Library/Application Support/FF777NavStudio'
- sous Windows : '%USERPROFILE%\AppData\Roaming\FF777NavStudio'

