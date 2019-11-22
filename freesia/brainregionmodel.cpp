#include "brainregionmodel.h"
#include "common.h"

#include <QApplication>
#include <QVariantList>
#include <QJsonDocument>
#include <QJsonArray>
#include <QPainterPath>
#include <QColor>
#include <QDebug>
#include <QFile>
#include <QTextStream>

using namespace cv;

BrainRegionModel::BrainRegionModel():m_modelFolderPath(QApplication::applicationDirPath()+"/data/"),
    m_selectModelIndex(0),m_voxels(nullptr){

    Common *c=Common::i();QVariantMap info;
    if(!Common::loadJson(m_modelFolderPath+"freesia-atlas.json",info)){emit c->showMessage("Unable to load brain model information");return;}

    foreach(QVariant v,info["atlas"].toList()){
        QVariantMap v1=v.toMap();ModelInfo info;info.name=v1["name"].toString();
        info.fullName=v1["full_name"].toString();info.voxelSize=v1["voxel_size"].toFloat();
        info.imagePath=m_modelFolderPath+v1["annotation_path"].toString();
        info.regionPath=m_modelFolderPath+v1["structures_path"].toString();
        QStringList v1s=v1["image_dimension"].toString().split(" ");if(v1s.length()!=3){continue;}
        info.dimension=cv::Point3i(v1s[0].toInt(),v1s[1].toInt(),v1s[2].toInt());

        m_modelNames.append(info.name);m_models.append(info);
    }
}

void BrainRegionModel::getModelSize(Point3i &dimension, Point3i &voxelSize){
    dimension=m_size;voxelSize=Point3i(m_voxelSize,m_voxelSize,m_voxelSize);
}

QString BrainRegionModel::getSelectedModelPath(){return m_selectModelIndex<0?"":m_models[m_selectModelIndex].regionPath;}

bool BrainRegionModel::loadRegions(const QString &filePath){
    QVariantMap params;if(!Common::loadJson(filePath,params)){return false;}
    qDeleteAll(m_rootRegions);m_rootRegions.clear();m_color2Regions.clear();m_id2Regions.clear();

    QMapIterator<QString,QVariant> iter(params);
    while(iter.hasNext()){
        iter.next();QVariantMap item=iter.value().toMap();//if(item["graph_order"].toInt()==0){continue;}
        int sId=item["id"].toInt();QString acronym=item["acronym"].toString(),name=item["safe_name"].toString();
        bool bOK;int parentId=item["parent_structure_id"].toInt(&bOK);if(!bOK){parentId=-1;}
        int color=item["label_color"].toInt();if(color<=0){continue;}
        RegionNode *node=new RegionNode(sId,parentId,acronym,name,color);
        m_color2Regions[color]=node;m_id2Regions[sId]=node;
    }
    foreach(RegionNode *node,m_color2Regions){
        node->parentNode=m_id2Regions.value(node->parentId,nullptr);
        if(nullptr!=node->parentNode){node->parentNode->subRegions.append(node);}
        else{m_rootRegions.append(node);}
    }
}

bool BrainRegionModel::loadSelectedModel(){
    Common *c=Common::i();
    if(m_selectModelIndex<0||m_selectModelIndex>=m_models.length()){emit c->showMessage("No model was selected",3000);return false;}
    emit c->showMessage("Loading brain atlas ...");

    ModelInfo modelInfo=m_models[m_selectModelIndex];m_voxelSize=modelInfo.voxelSize;

    FILE *fp=fopen(modelInfo.imagePath.toStdString().c_str(),"rb");
    if(nullptr==fp){emit c->showMessage("Unable to load model "+modelInfo.name,5000);return false;}
    m_size=modelInfo.dimension;

    free(m_voxels);size_t num=modelInfo.dimension.x*modelInfo.dimension.y*modelInfo.dimension.z,length=num*2;
    m_voxels=(uint16_t*)malloc(length);fread((char*)m_voxels,length,1,fp);fclose(fp);

    loadRegions(modelInfo.regionPath);

    int i=0,count=0;
    for(uint16_t *v=m_voxels,*vEnd=v+num;v!=vEnd;v++,i++){
        uint16_t color=*v;if(color==0){continue;}
        RegionNode *ptr=m_color2Regions.value(color,nullptr);
        if(nullptr!=ptr){ptr->voxelIndexes.append(i);count++;}
    }

    foreach(RegionNode *node,m_color2Regions){
        int n=node->voxelIndexes.length();QStringList pathNames;
        for(RegionNode *p=node;nullptr!=p;p=p->parentNode){p->totalVoxelNumber+=n;pathNames.prepend(p->acronym);}
        node->path=pathNames.join("/");node->level=pathNames.length();
    }

    emit c->showMessage("Done",500);return true;
}

uint16_t *BrainRegionModel::getVoxels(Point3i &size, float &voxelSize){size=m_size;voxelSize=m_voxelSize;return m_voxels;}

inline void getRegionSubColors(RegionNode *node,QList<int> &colors){
    colors.append(node->color);foreach(RegionNode *p,node->subRegions){getRegionSubColors(p,colors);}
}
char *BrainRegionModel::getRegionVoxelByColor(int color){
    RegionNode *node=m_color2Regions.value(color,nullptr);if(nullptr==node){return nullptr;}
    QList<int> colors;getRegionSubColors(node,colors);
    int length=m_size.x*m_size.y*m_size.z;char *data=(char*)malloc(length);memset(data,0,length);
    foreach(int color,colors){foreach(int index,m_color2Regions[color]->voxelIndexes){*(data+index)=255;}}
    return data;
}

bool BrainRegionModel::dumpCellCounting(size_t colorCounts[], const QString &filePath){
    QFile file(filePath);if(!file.open(QIODevice::WriteOnly|QIODevice::Text)){return false;}

    foreach(RegionNode *node,m_color2Regions){
        size_t number=colorCounts[node->color];if(number==0){continue;}
        while(nullptr!=node->parentNode){
            colorCounts[node->parentNode->color]+=number;node=node->parentNode;
        }
    }
    double voxelSize3d=pow(m_voxelSize,3);

    QTextStream out(&file);out<<"id,acronym,count,density,volume,path,level,name\n";
    foreach(RegionNode *node,m_color2Regions){
        size_t n=colorCounts[node->color];int n1=node->totalVoxelNumber;if(n1<=0){continue;}
        double volume=n1*voxelSize3d,density=n/volume;
        out<<node->id<<","<<node->acronym<<","<<colorCounts[node->color]<<","<<density
          <<","<<volume<<","<<node->path<<","<<node->level<<","<<node->name<<"\n";
    }

    out.flush();file.close();return true;
}
