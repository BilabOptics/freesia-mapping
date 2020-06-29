#include "menubar.h"
#include "common.h"

#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>

MenuBar::MenuBar(){
    initFileMenu();initEditMenu();initViewMenu();
    initHelpMenu();
}

void MenuBar::initFileMenu(){
    QMenu *menu=addMenu("File");Common *c=Common::i();

    menu->addAction("Load project",c,&Common::loadProject,QKeySequence(Qt::CTRL|Qt::Key_O));
    menu->addAction("Save project",[c](){emit c->saveProject(false);},QKeySequence(Qt::CTRL|Qt::Key_S));
    menu->addAction("Save project As...",[c](){emit c->saveProject(true);});

    menu->addAction("Import images",c,&Common::importDirectory);
    menu->addAction("Import spots",c,&Common::importSpotsDirectory);
    menu->addAction("Export cell counting result",c,&Common::exportCellCounting);
    menu->addAction("Export pixel counting result",c,&Common::exportPixelCounting);
    menu->addAction("Merge counting result",c,&Common::mergeCellCounting);
}

void MenuBar::initEditMenu(){
    QMenu *menu=addMenu("Edit");Common *c=Common::i();

    menu->addAction("Build warp field",[c](){emit c->buildWarpField(false);},QKeySequence(Qt::CTRL|Qt::Key_B));
//    menu->addAction("Build all warp fields",[c](){emit c->buildWarpField(true);});
    menu->addAction("Remove all markers in current group",c,&Common::removeCurrentMarkers);
}

void MenuBar::initViewMenu(){
    QMenu *menu=addMenu("View");Common *c=Common::i();
    menu->addAction("Toggle warp preview",c,&Common::toggleWarpPreview,QKeySequence(Qt::ALT|Qt::Key_1));
    menu->addAction("Show/hide spots",c,&Common::toggleShowSpots,QKeySequence(Qt::ALT|Qt::Key_2));
}

void MenuBar::initHelpMenu(){
    QMenu *menu=addMenu("Help");
    menu->addAction("Software homepage",[](){
        QDesktopServices::openUrl(QUrl("https://github.com/BilabOptics/freesia-mapping"));
    });
}
