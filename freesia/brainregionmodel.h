#ifndef BRAINREGIONMODEL_H
#define BRAINREGIONMODEL_H

#include <QObject>
#include <QStringList>
#include <QMap>

#include <opencv2/core.hpp>

class QPainterPath;

struct ModelInfo{
    QString name,fullName,imagePath,regionPath;
    float voxelSize;cv::Point3i dimension;
};
struct RegionNode{
    int id,parentId,color;QString acronym,name,path;
    QList<RegionNode*> subRegions;RegionNode *parentNode;
    QList<int> voxelIndexes;int totalVoxelNumber;

    RegionNode(int _id,int _parentId,QString _acronym,QString _name,int _color)
        :id(_id),parentId(_parentId),acronym(_acronym),name(_name),color(_color),parentNode(nullptr),totalVoxelNumber(0){}
    ~RegionNode(){qDeleteAll(subRegions);}
};

class BrainRegionModel : public QObject
{
    Q_OBJECT

    const QString m_modelFolderPath;
    QList<ModelInfo> m_models;QStringList m_modelNames;
    int m_selectModelIndex;

    cv::Point3i m_size;uint16_t *m_voxels;
    float m_voxelSize;

    QList<RegionNode*> m_rootRegions;QMap<int,RegionNode*> m_color2Regions,m_id2Regions;
    bool loadRegions(const QString &filePath);

    BrainRegionModel();
public:
    static BrainRegionModel *i(){static BrainRegionModel m;return &m;}

    QString getSelectedModelPath();bool loadSelectedModel();
    void getModelSize(cv::Point3i &dimension,cv::Point3i &voxelSize);

    uint16_t *getVoxels(cv::Point3i &size, float &voxelSize);
    char *getRegionVoxelByColor(int color);

    bool dumpCellCounting(size_t colorCounts[65536],const QString &filePath);
signals:
    void modelSelected();
    void modelUpdated();
};

#endif // BRAINREGIONMODEL_H
