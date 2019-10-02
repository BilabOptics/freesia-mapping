#ifndef BRAINREGIONPANEL_H
#define BRAINREGIONPANEL_H

//This panel is contained in ControlPanel

#include <QWidget>

#include <QWidget>
#include <QModelIndex>
#include <QStringList>

QT_BEGIN_NAMESPACE
class QTreeView;
class QStandardItemModel;
class QStandardItem;
class QLineEdit;
QT_END_NAMESPACE

struct RegionTree{
    int id,parentId,color;QString acronym,name;QList<RegionTree*> subRegions;
    QModelIndex modelIndex;
    RegionTree(int _id,int _parentId,int _color,QString _acronym,QString _name)
        :id(_id),parentId(_parentId),color(_color),acronym(_acronym),name(_name){}
};

class BrainRegionPanel : public QWidget
{
    Q_OBJECT

    QTreeView *m_pTableView;QStandardItemModel *m_pModel;
    QList<QStandardItem*> m_items;

    QList<RegionTree*> m_allRegions,m_rootRegions;QMap<void*,RegionTree*> m_models2RegionTree;
    QMap<int,RegionTree*> m_id2RegionTree,m_color2RegionTree;

    QLineEdit *m_searchEditor;QString m_searchText;int m_searchIndex;

    QStandardItem* getItem(RegionTree*,QStandardItem *parent);
    QStandardItem* getItem(const QString &text,const QString &tooltipText);
public:
    explicit BrainRegionPanel();

signals:

private slots:
    void selectRegionByColor(int color);
    void selectRow(QModelIndex);
    void searchRegion();

    void loadItems();
};

#endif // BRAINREGIONPANEL_H
