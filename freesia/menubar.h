#ifndef MENUBAR_H
#define MENUBAR_H

#include <QMenuBar>

class MenuBar : public QMenuBar
{
    Q_OBJECT

    void initFileMenu();void initEditMenu();void initViewMenu();
    void initHelpMenu();
public:
    explicit MenuBar();

signals:

public slots:
};

#endif // MENUBAR_H
