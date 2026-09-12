#include "mainwindow.h"
#include "ThemeManager.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // Applique le thème mémorisé AVANT la création de la fenêtre : tous les
    // widgets sont ainsi construits avec le bon style et la bonne palette.
    navstud::ui::ThemeManager::apply(navstud::ui::ThemeManager::loadSavedTheme());

    MainWindow w;
    w.show();
    return QApplication::exec();
}
