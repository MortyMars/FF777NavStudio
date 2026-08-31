# CMakeLists.txt - Projet FF777NavStudio

# Déclarations standard
cmake_minimum_required(VERSION 3.20)
project(FF777NavStudio VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ----------------------------------------------------------------------------
# Auto-détection de Qt : sans Qt Creator, on cherche Qt 6 dans les emplacements
# habituels (installations officielles, Homebrew) si CMAKE_PREFIX_PATH n'a pas
# déjà été fourni en ligne de commande.
# ----------------------------------------------------------------------------
if(NOT DEFINED CMAKE_PREFIX_PATH)
    set(AUTO_QT_CANDIDATES
        "$ENV{QT_INSTALL_PREFIX}"
    )
    if(APPLE)
        list(APPEND AUTO_QT_CANDIDATES
            "/Applications/Qt/6.11.2/macos"
            "/Applications/Qt/6.8.0/macos"
            "/Applications/Qt/6.5.3/macos"
            "/opt/homebrew/opt/qt"
            "/usr/local/opt/qt"
        )
    elseif(UNIX)
        list(APPEND AUTO_QT_CANDIDATES
            "/usr/lib/x86_64-linux-gnu/cmake/Qt6"
            "/usr/lib/cmake/Qt6"
            "/opt/Qt/6.8.0/gcc_64"
        )
    endif()

    foreach(candidate IN LISTS AUTO_QT_CANDIDATES)
        if(candidate AND EXISTS "${candidate}")
            message(STATUS "Qt auto-détecté : ${candidate}")
            set(CMAKE_PREFIX_PATH "${candidate}")
            break()
        endif()
    endforeach()
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Xml Sql Concurrent)

qt_standard_project_setup()

# qt_standard_project_setup() active AUTOMOC/AUTOUIC mais pas AUTORCC :
# sans cette ligne les .qrc (documents Markdown embarqués) ne sont pas compilés.
set(CMAKE_AUTORCC ON)


# ----------------------------------------------------------------------------
# Bibliothèque Core : modèle, writer, reader, validator. Partagée entre l'app
# GUI et le CLI — aucune dépendance à Qt6::Widgets ici, uniquement Qt6::Core.
# ----------------------------------------------------------------------------
add_library(FF777NavStudioCore STATIC
    src/model/Entities.h
    src/repository/ProjectRepository.h
    src/writer/TextFormat.h
    src/writer/NavDataWriter.h
    src/writer/NavDataWriter.cpp
    src/reader/TextParse.h
    src/reader/NavDataReader.h
    src/reader/NavDataReader.cpp
    src/validator/Validator.h
    src/validator/Validator.cpp
    src/userdata/UserEntities.h
    src/userdata/UserProject.h
    src/conversion/ConversionResult.h
    src/conversion/IdentResolver.h
    src/conversion/IdentResolver.cpp
    src/conversion/UserToModelConverter.h
    src/conversion/UserToModelConverter.cpp
    src/conversion/Regenerator.h
    src/conversion/Regenerator.cpp
    src/worldindex/WorldIndexReader.h
    src/worldindex/WorldIndexReader.cpp
    src/persistence/ProjectStore.h
    src/persistence/ProjectStore.cpp
    src/tools/Nav1DbPipeline.h
    src/tools/Nav1DbPipeline.cpp
    src/extract/NavDataBase.h
    src/extract/NavDataBase.cpp
    src/extract/ExtractedProjectBuilder.h
    src/extract/ExtractedProjectBuilder.cpp

    # Décodage/encodage nav1.db : ParserFFDB (classes intégrées)
    src/parser/FileConverter.h
    src/parser/FileConverter.cpp
    src/parser/NavDataFile.h
    src/parser/NavDataFile.cpp
    src/parser/Stream.h
    src/parser/Stream.cpp
    src/parser/StreamFile.h
    src/parser/StreamFile.cpp
    src/parser/Types.h
    src/parser/xxhash.h
)

target_include_directories(FF777NavStudioCore
    PUBLIC
        src
        src/model
        src/repository
        src/writer
        src/reader
        src/validator
        src/userdata
        src/conversion
        src/worldindex
        src/persistence
        src/util
        src/tools
        src/parser
        src/extract
)

target_link_libraries(FF777NavStudioCore PUBLIC
    Qt6::Core
    Qt6::Sql
)


# ----------------------------------------------------------------------------
# Application GUI
# ----------------------------------------------------------------------------
add_executable(FF777NavStudio
    src/main.cpp
    src/ui/mainwindow.h
    src/ui/mainwindow.cpp
    src/ui/mainwindow.ui
    src/ui/GenericTableModel.h
    src/ui/GenericTableModel.cpp
    src/ui/EditorFieldHelpers.h
    src/ui/TableColumnHelpers.h
    src/ui/PointEditorWidget.h
    src/ui/PointEditorWidget.cpp
    src/ui/WaypointEditorWidget.h
    src/ui/WaypointEditorWidget.cpp
    src/ui/AirportEditorWidget.h
    src/ui/AirportEditorWidget.cpp
    src/ui/RunwayEditorWidget.h
    src/ui/RunwayEditorWidget.cpp
    src/ui/NavaidEditorWidget.h
    src/ui/NavaidEditorWidget.cpp
    src/ui/LegEditorWidget.h
    src/ui/LegEditorWidget.cpp
    src/ui/LegSequenceEditorWidget.h
    src/ui/LegSequenceEditorWidget.cpp
    src/ui/ApproachEditorWidget.h
    src/ui/ApproachEditorWidget.cpp
    src/ui/ApproachTransitionEditorWidget.h
    src/ui/ApproachTransitionEditorWidget.cpp
    src/ui/PointTableModel.h
    src/ui/PointTableModel.cpp
    src/ui/ProcedureEditorWidget.h
    src/ui/ProcedureEditorWidget.cpp
    src/ui/ProcedureTransitionEditorWidget.h
    src/ui/ProcedureTransitionEditorWidget.cpp
    src/ui/RunwayProcedureTransitionEditorWidget.h
    src/ui/RunwayProcedureTransitionEditorWidget.cpp
    src/ui/UnitConverterWidget.h
    src/ui/UnitConverterWidget.cpp
    src/ui/docs.qrc
    src/ui/AirportExtractDialog.h
    src/ui/AirportExtractDialog.cpp
)

target_include_directories(FF777NavStudio
    PRIVATE
        src/ui
)

target_link_libraries(FF777NavStudio PRIVATE
    FF777NavStudioCore
    Qt6::Widgets Qt6::Xml Qt6::Concurrent
)

# ----------------------------------------------------------------------------
# Bundle macOS : application autonome, double-cliquable depuis le Finder.
# ----------------------------------------------------------------------------
if(APPLE)
    set_target_properties(FF777NavStudio PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "FF777NavStudio"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.ff777.navstudio"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_ICON_FILE "FF777NavStudio.icns"
    )
    target_sources(FF777NavStudio PRIVATE
        "${CMAKE_SOURCE_DIR}/assets/FF777NavStudio.icns"
    )
    set_source_files_properties("${CMAKE_SOURCE_DIR}/assets/FF777NavStudio.icns"
        PROPERTIES MACOSX_PACKAGE_LOCATION Resources
    )
endif()


# ----------------------------------------------------------------------------
# CLI : validate <dossier> / export <dossier_source> <dossier_destination>
# ----------------------------------------------------------------------------
add_executable(FF777NavStudioCli
    src/cli/main.cpp
)

target_link_libraries(FF777NavStudioCli PRIVATE
    FF777NavStudioCore
)



