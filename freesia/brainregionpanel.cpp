#include "brainregionpanel.h"

#include <QDebug>
#include <QFile>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QStandardItemModel>
#include <QTreeView>
#include <QVBoxLayout>
#include <QVariantMap>

#include "brainregionmodel.h"
#include "common.h"

BrainRegionPanel::BrainRegionPanel() {
  QVBoxLayout *lyt = new QVBoxLayout;
  lyt->setContentsMargins(0, 0, 0, 0);
  setLayout(lyt);
  lyt->setSpacing(2);

  m_pTableView = new QTreeView;
  m_pModel = new QStandardItemModel;
  m_pTableView->setModel(m_pModel);
  m_pTableView->setHeaderHidden(true);

  m_pTableView->header()->setStretchLastSection(false);
  m_pTableView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
  m_pTableView->setExpandsOnDoubleClick(false);

  lyt->addWidget(m_pTableView, 1);

  QHBoxLayout *inputLyt = new QHBoxLayout;
  inputLyt->setContentsMargins(0, 0, 0, 0);
  m_searchEditor = new QLineEdit;
  QPushButton *btnSearch = new QPushButton("Search");
  m_searchEditor->setPlaceholderText("Enter region name here");
  inputLyt->addWidget(m_searchEditor);
  inputLyt->addWidget(btnSearch);
  lyt->addLayout(inputLyt);

  loadItems();
  connect(BrainRegionModel::i(), &BrainRegionModel::modelSelected, this,
          &BrainRegionPanel::loadItems);

  connect(btnSearch, &QPushButton::clicked, this,
          &BrainRegionPanel::searchRegion);
  connect(m_searchEditor, &QLineEdit::returnPressed, this,
          &BrainRegionPanel::searchRegion);

  connect(m_pTableView, &QTreeView::clicked, this,
          &BrainRegionPanel::selectRow);
  connect(Common::i(), &Common::regionSelected2, this,
          &BrainRegionPanel::selectRegionByColor);
}

QStandardItem *BrainRegionPanel::getItem(const QString &text,
                                         const QString &tooltipText) {
  QStandardItem *item = new QStandardItem(text);
  item->setToolTip(tooltipText);
  item->setEditable(false);
  item->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);

  return item;
}
QStandardItem *BrainRegionPanel::getItem(RegionTree *node,
                                         QStandardItem *parent) {
  QStandardItem *pItem =
      getItem(node->acronym + " (" + node->name + ")", node->name);
  if (nullptr == parent) {
    m_pModel->appendRow(pItem);
  } else {
    parent->appendRow(pItem);
  }

  node->modelIndex = pItem->index();

  QListIterator<RegionTree *> iter(node->subRegions);
  while (iter.hasNext()) {
    getItem(iter.next(), pItem);
  }

  m_models2RegionTree.insert(pItem, node);

  return pItem;
}

void BrainRegionPanel::searchRegion() {
  QString name = m_searchEditor->text().toLower();
  if (name.isEmpty()) {
    return;
  }

  if (name != m_searchText || m_searchIndex >= m_allRegions.length()) {
    m_searchIndex = 0;
  }
  m_searchText = name;

  for (; m_searchIndex < m_allRegions.length(); m_searchIndex++) {
    RegionTree *t = m_allRegions[m_searchIndex];
    if (t->acronym.toLower().indexOf(name) >= 0 ||
        t->name.toLower().indexOf(name) >= 0) {
      selectRegionByColor(t->color);
      selectRow(t->modelIndex);
      break;
    }
  }

  m_searchIndex++;
}

void BrainRegionPanel::selectRegionByColor(int color) {
  RegionTree *node = m_color2RegionTree.value(color);
  if (nullptr == node) {
    return;
  }
  QModelIndex index = node->modelIndex;

  m_pTableView->collapseAll();
  QModelIndex parentIndex = index.parent();
  while (parentIndex.isValid()) {
    m_pTableView->expand(parentIndex);
    parentIndex = parentIndex.parent();
  }

  m_pTableView->selectionModel()->select(index,
                                         QItemSelectionModel::SelectCurrent);
  m_pTableView->scrollTo(index);
}

void BrainRegionPanel::selectRow(QModelIndex index) {
  void *v = (void *)m_pModel->itemFromIndex(index);

  RegionTree *node = m_models2RegionTree.value(v, nullptr);
  if (nullptr != node) {
    emit Common::i()->regionSelected(node->color);
  }
}

void BrainRegionPanel::loadItems() {
  const QString filePath = BrainRegionModel::i()->getSelectedModelPath();
  QVariantMap params;
  if (!Common::loadJson(filePath, params)) {
    return;
  }

  qDeleteAll(m_allRegions);
  m_allRegions.clear();
  m_rootRegions.clear();
  m_models2RegionTree.clear();
  m_id2RegionTree.clear();
  qDeleteAll(m_items);
  m_items.clear();
  m_pModel->removeRows(0, m_pModel->rowCount());

  QMap<int, QVariantMap> items;
  QMapIterator<QString, QVariant> iter(params);
  while (iter.hasNext()) {
    iter.next();
    QVariantMap item = iter.value().toMap();
    int order = item["graph_order"].toInt();
    if (order == 0) {
      continue;
    }
    if (items.contains(order)) {
      qWarning() << "item with order" << order << "existed";
      continue;
    }
    items.insert(order, item);
  }

  QMapIterator<int, QVariantMap> iter1(items);
  while (iter1.hasNext()) {
    iter1.next();

    QVariantMap item = iter1.value();
    int sId = item["id"].toInt(), color = item["label_color"].toInt();
    bool bOK;
    int parentId = item["parent_structure_id"].toInt(&bOK);
    if (!bOK) {
      parentId = -1;
    }
    QString acronym = item["acronym"].toString(),
            name = item["name"].toString();

    RegionTree *node = new RegionTree(sId, parentId, color, acronym, name);
    m_allRegions.append(node);
    m_id2RegionTree.insert(sId, node);
    m_color2RegionTree.insert(color, node);
  }
  QListIterator<RegionTree *> iter2(m_allRegions);
  while (iter2.hasNext()) {
    RegionTree *node = iter2.next();
    int parentId = node->parentId;
    if (!m_id2RegionTree.contains(parentId)) {
      m_rootRegions.append(node);
      continue;
    }
    RegionTree *parentNode = m_id2RegionTree[parentId];
    parentNode->subRegions.append(node);
  }

  QListIterator<RegionTree *> iter3(m_rootRegions);
  while (iter3.hasNext()) {
    getItem(iter3.next(), nullptr);
  }
}
