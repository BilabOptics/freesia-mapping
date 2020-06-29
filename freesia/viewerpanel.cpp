#include "viewerpanel.h"
#include "contrastadjuster.h"
#include "brainregionmodel.h"
#include "volumeviewer.h"
#include "sectionviewer.h"
#include "importexportdialog.h"
#include "../../lab-works/flsm/tstorm-control/processing/imagewriter.h"

#include "common.h"

#include <QElapsedTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDebug>
#include <QFileDialog>
#include <QFileInfo>
#include <QComboBox>
#include <QGridLayout>
#include <QDirIterator>
#include <QStackedWidget>
#include <QLineF>

#include <opencv2/opencv.hpp>

extern "C"{
#include <nn.h>
}

using namespace cv;

struct Group{
    TransformParameters param,paramSaved;
    cv::Mat warpField;
};

ViewerPanel::ViewerPanel():m_bRunning(true),m_regionNextColor(-1),m_bImageUpdated(false),m_bProjectPathValid(false),m_bViewerFullscreen(false),
    m_projectPathPtr(nullptr),m_params3d(new TransformParameters),m_params3dSaved(new TransformParameters),m_bShowSpots(true),
    m_importParams(nullptr),m_projectPath2Save(nullptr),m_nextWarpType(-1),m_currentGroup(nullptr),
    m_exportPath(nullptr),m_exportPixelPath(nullptr),m_mergePath(nullptr),m_spotsPath2Import(nullptr){

    QVBoxLayout *lyt=new QVBoxLayout;setLayout(lyt);lyt->setContentsMargins(0,0,0,0);lyt->setSpacing(0);
    Common *c=Common::i();

    QHBoxLayout *headerLyt=new QHBoxLayout;lyt->addLayout(headerLyt);lyt->addSpacing(1);
    headerLyt->setContentsMargins(0,0,0,0);headerLyt->setSpacing(2);
    QComboBox *adjusterBox=new QComboBox;adjusterBox->addItems(QStringList()<<"Section"<<"Volume");

    ContrastAdjuster *adjusterGroup=new ContrastAdjuster(65535,1000),*adjusterVolume=new ContrastAdjuster(65535,1000);
    connect(adjusterGroup,&ContrastAdjuster::updateViewerThreshold,c,&Common::sliceContrastChanged);
    connect(adjusterVolume,&ContrastAdjuster::updateViewerThreshold,c,&Common::volumeContrastChanged);

    QStackedWidget *adjusterContainer=new QStackedWidget;adjusterContainer->addWidget(adjusterGroup);adjusterContainer->addWidget(adjusterVolume);
    headerLyt->addWidget(adjusterBox);headerLyt->addWidget(adjusterContainer);
    connect(adjusterBox,SIGNAL(currentIndexChanged(int)),adjusterContainer,SLOT(setCurrentIndex(int)));

    m_volumeViewer=new VolumeViewer;
    for(int i=0;i<3;i++){m_sectionViewers[i]=new SectionViewer(i);}

    QGridLayout *lyt1=new QGridLayout;lyt1->setContentsMargins(0,0,0,0);lyt1->setSpacing(1);lyt->addLayout(lyt1,1);
    lyt1->addWidget(m_sectionViewers[2],0,0);lyt1->addWidget(m_volumeViewer,0,1);
    lyt1->addWidget(m_sectionViewers[0],1,0);lyt1->addWidget(m_sectionViewers[1],1,1);

    connect(c,&Common::toggleFullscreen,[this](int type){
        if(type<0||type>3){return;}
        bool bVisible=m_bViewerFullscreen;QWidget *p=(type==3?(QWidget*)m_volumeViewer:m_sectionViewers[type]);p->hide();
        foreach(QWidget *w,QList<QWidget*>()<<m_sectionViewers[0]<<m_sectionViewers[1]<<m_sectionViewers[2]<<m_volumeViewer){w->setVisible(bVisible);}
        p->show();m_bViewerFullscreen=!m_bViewerFullscreen;
    });
    connect(c,&Common::toggleWarpPreview,[this,c](){
        bool v=!c->p_bWarpPreview.load();c->p_bWarpPreview=v;m_bImageUpdated=true;
        emit c->setProjectStatus(v?"Preview":"");
    });
    connect(c,&Common::toggleShowSpots,[this,c](){
        bool v=!m_bShowSpots.load();m_bShowSpots=v;m_bImageUpdated=true;
        emit c->showMessage("Spots are "+QString(v?"visible":"hidden"),1000);
    });

    connect(c,&Common::importDirectory,this,&ViewerPanel::onImportDirectory);
    connect(c,&Common::importSpotsDirectory,this,&ViewerPanel::onImportSpots);
    connect(c,&Common::exportCellCounting,this,&ViewerPanel::onExportCellCounting);
    connect(c,&Common::exportPixelCounting,this,&ViewerPanel::onExportPixelCounting);
    connect(c,&Common::mergeCellCounting,this,&ViewerPanel::onMergeCellCounting);
    connect(c,&Common::loadProject,this,&ViewerPanel::onLoadProject);
    connect(c,&Common::saveProject,this,&ViewerPanel::onSaveProject);
    connect(c,&Common::regionSelected,[this](int color){m_regionNextColor.exchange(color);});
    connect(c,&Common::buildWarpField,[this](bool bAll){m_nextWarpType=int(bAll);});

    m_dataThread=new std::thread(&ViewerPanel::mainLoop,this);
}

ViewerPanel::~ViewerPanel(){m_bRunning=false;m_dataThread->join();delete m_dataThread;}

void ViewerPanel::mainLoop(){
    BrainRegionModel::i()->loadSelectedModel();

    QElapsedTimer timer;
    while(m_bRunning){
        timer.start();

        if(m_images.empty()){
            loadProject();importDirectory();
        }
        if(!m_images.empty()){
            buildWarpField();importSpots();
            updateImages();selectRegion();transform3d();transform2d();
            saveProject();exportCellCounting();exportPixelCounting();
        }
        mergeCellCounting();

        int timeLeft=100-timer.elapsed();if(timeLeft>0){std::this_thread::sleep_for(std::chrono::milliseconds(timeLeft));}
    }
}

inline void enhanceImage(const cv::Mat &src,uint8_t *pDst,int thresh){
    uint16_t *pData=(uint16_t*)src.data,*pDataEnd=pData+src.size().area();float f=255/float(thresh);
    for(;pData!=pDataEnd;pData++,pDst++){uint16_t v=*pData;*pDst=uint8_t(v>=thresh?255:v*f);}
}
inline void loadImageSpots(Image *image){
    if(nullptr==image||image->bSpotsLoaded){return;}
    image->spots.clear();image->bSpotsLoaded=true;if(image->spotsPath.isEmpty()){return;}

    QFile file(image->spotsPath);if(!file.open(QIODevice::ReadOnly|QIODevice::Text)){return;}
    QTextStream in(&file);
    while(!in.atEnd()){
        QStringList line=in.readLine().split(",");if(line.length()!=8){continue;}
        if(line[4]!="Spot"||line[5]!="Position"){continue;}

        bool bOK;int64_t spotId=line[7].toLongLong(&bOK);if(!bOK){continue;}
        Point3d p(line[0].toDouble(),line[1].toDouble(),line[2].toDouble());
        image->spots[spotId]=p;
    }
}

bool ViewerPanel::updateImages(){
    bool bForced=false;if(m_bImageUpdated){bForced=true;m_bImageUpdated=false;}
    Common *c=Common::i();

    for(int i=0;i<3;i++){
        SectionViewer *p=m_sectionViewers[i];int index;
        bool bIndexUpdated=p->getUpdatedIndex(index);
        if(bForced){if(!bIndexUpdated&&!p->getImageIndex(index)){continue;}}else if(!bIndexUpdated){continue;}

        OutputImageData data;m_volumeViewer->getImageByIndex(i,index,data);

        if(nullptr!=data.imageData){
            int pSize[3]={0};data.imageData->GetDimensions(pSize);
            char *pData=(char*)data.imageData->GetScalarPointer();Mat m(Size(pSize[0],pSize[1]),CV_16UC1,pData);
            double *origin=data.imageData->GetOrigin(),*spacing=data.imageData->GetSpacing(),rotation=0;
            int imageIndex=index,groupIndex=-1;

            if(2==i){
                Image *pImage=m_images[index];m=pImage->data;groupIndex=pImage->groupIndex;
                Group *slice=m_groups.value(groupIndex,nullptr);m_currentGroup=slice;
                TransformParameters *param;if(bIndexUpdated){param=new TransformParameters;param->sliceIndex=groupIndex;}

                if(m_bShowSpots.load()){
                    loadImageSpots(pImage);
                    if(!pImage->spots.empty()){
                        m=m.clone();
                        foreach(cv::Point3d p,pImage->spots){
                            int x=round(p.x/spacing[0]),y=round(m.rows-p.y/spacing[1]-1);
                            if(x>=0&&x<m.cols&&y>=0&&y<m.rows){m.at<uint16_t>(y,x)=65535;}
                        }
                    }
                }

                if(nullptr!=slice){
                    for(int k=0;k<2;k++){
                        origin[k]+=slice->param.offset[k]-pSize[k]*(slice->param.scale[k]-1)*spacing[k]/2;
                        spacing[k]*=slice->param.scale[k];
                    }
                    rotation=slice->param.rotation[2];if(bIndexUpdated){param->copy(slice->param);}

                    if(c->p_bWarpPreview.load()&&!slice->warpField.empty()&&
                            slice->warpField.size()==m.size()&&slice->warpField.type()==CV_16UC4){//(slice->warpField.type()==CV_32FC2||
                        Mat dst=Mat::zeros(m.size(),m.type());int w=m.cols,h=m.rows;
                        uint16_t *pDst=(uint16_t*)dst.data,*pDstEnd=pDst+m.size().area(),*pSrc=(uint16_t*)m.data;
                        float *pMap=(float*)slice->warpField.data;
                        for(;pDst!=pDstEnd;pDst++,pMap+=2){
                            int x=pMap[0],y=pMap[1];if(x>=0&&x<w&&y>=0&&y<h){*pDst=*(pSrc+y*w+x);}
                        }
                        m=dst;
                    }
                }

                if(bIndexUpdated){emit c->sliceChanged(param);}
            }
            p->updateResliceImage(m,origin,spacing,rotation,imageIndex,groupIndex);
        }
        if(nullptr!=data.modelData){
            int pSize[3]={0};data.modelData->GetDimensions(pSize);int w=pSize[0],h=pSize[1];
            uint16_t *pData=(uint16_t*)data.modelData->GetScalarPointer();
            if(w>0&&h>0){//int w=m.cols,h=m.rows;
                double *origin=data.modelData->GetOrigin(),*spacing=data.modelData->GetSpacing();
                Mat m(Size(pSize[0],pSize[1]),CV_16UC1,pData);p->updateModelImage(m,origin,spacing);
            }
        }
        if(nullptr!=data.selectModelData){
            int pSize[3]={0};data.selectModelData->GetDimensions(pSize);
            char *pData=(char*)data.selectModelData->GetScalarPointer();Mat m(Size(pSize[0],pSize[1]),CV_8UC1,pData);
            double *origin=data.selectModelData->GetOrigin(),*spacing=data.selectModelData->GetSpacing();
            p->updateSelectImage(m,origin,spacing);
        }

        emit p->imageUpdated();
    }

    return true;
}

void ViewerPanel::buildWarpField(int sliceIndex, Mat &warpField, bool bInversed){
    Group *slice=m_groups[sliceIndex];
    double offsets[2]={slice->param.offset[0]-m_currentGroup->param.offset[0],slice->param.offset[1]-m_currentGroup->param.offset[1]},
            scales[2]={slice->param.scale[0]/m_currentGroup->param.scale[0],slice->param.scale[1]/m_currentGroup->param.scale[1]};

    QList<QLineF> lines;m_sectionViewers[2]->getMarkers(sliceIndex,offsets,scales,slice->param.rotation[2],lines);
    if(m_totalImageSize.isEmpty()||lines.empty()){warpField=Mat();return;}

    QPointF factor(1,1);int width=m_totalImageSize.width(),height=m_totalImageSize.height();
    int nout=width*height,nin=lines.length()+4;

    point *poutX=new point[nout],*poutY=new point[nout];point *_poutX=poutX,*_poutY=poutY;
    for(int y=0;y<height;y++){
        for(int x=0;x<width;x++){
            _poutX->x=x;_poutX->y=y;_poutX->z=0;_poutY->x=x;_poutY->y=y;_poutY->z=0;_poutX++;_poutY++;
        }
    }

    point *pinX=new point[nin],*pinY=new point[nin];point *_pinX=pinX,*_pinY=pinY;
    foreach(QLineF p,lines){
        double x,y,dx=(p.p1().x()-p.p2().x())*factor.x(),dy=(p.p1().y()-p.p2().y())*factor.y();

        if(bInversed){x=p.p2().x()*factor.x();y=p.p2().y()*factor.y();}
        else{x=p.p1().x()*factor.x();y=p.p1().y()*factor.y();dx=-dx;dy=-dy;}

        _pinX->x=x;_pinX->y=y;_pinX->z=dx;_pinY->x=x;_pinY->y=y;_pinY->z=dy;_pinX++;_pinY++;
    }
    foreach(QPoint p,QList<QPoint>()<<QPoint(-width,-height)<<QPoint(2*width,-height)<<QPoint(-width,2*height)<<QPoint(2*width,2*height)){
        _pinX->x=p.x();_pinX->y=p.y();_pinX->z=0;_pinX++;_pinY->x=p.x();_pinY->y=p.y();_pinY->z=0;_pinY++;
    }

    nnpi_interpolate_points(nin,pinX,-DBL_MAX,nout,poutX);
    nnpi_interpolate_points(nin,pinY,-DBL_MAX,nout,poutY);

    Mat output=Mat(height,width,CV_16UC4);//32FC2
    float *pData=(float*)output.data;_poutX=poutX;_poutY=poutY;
    for(int i=0,x=0,y=0;i<nout;i++,_poutX++,_poutY++,pData+=2){
        float dx=_poutX->z,dy=_poutY->z,x1=x+dx,y1=y+dy;
        pData[0]=x1;pData[1]=y1;x++;if(x==width){x=0;y++;}
    }

    warpField=output;delete[] pinX;delete[] pinY;delete[] poutX;delete[] poutY;
}
void ViewerPanel::buildWarpField(){
    int warpType=m_nextWarpType.exchange(-1);if(warpType<0||nullptr==m_currentGroup){return;}

    QList<Group*> slices;m_bImageUpdated=true;Common *c=Common::i();
    if(0==warpType){slices.append(m_currentGroup);}
    else{foreach(Group *p,m_groups){slices.append(p);}}
    int index=1,number=slices.length();

    foreach(Group *p,slices){
        emit c->showMessage("Performing deformation for group "+QString::number(index)+"/"+QString::number(number));
        buildWarpField(p->param.sliceIndex,p->warpField,true);index++;
    }

    emit c->showMessage("Done",500);
    c->p_bWarpPreview=true;emit c->setProjectStatus("Preview");
}
void ViewerPanel::exportWarpFields(const QString &outputPath){
    int index=1,number=m_groups.keys().length();Common *c=Common::i();
    foreach(Group *p,m_groups){
        QString filePath=outputPath+"/"+QString::number(p->param.sliceIndex)+".tif";
        emit c->showMessage("Saving deformation of group "+QString::number(index)+"/"+QString::number(number));index++;

        if(p->warpField.empty()){
            QFile file(filePath);if(file.exists()){file.remove();}
            continue;
        }
        cv::Mat m(p->warpField.size(),CV_16UC4,p->warpField.data);cv::imwrite(filePath.toStdString(),m);
    }
}

void ViewerPanel::setGroupSize(int v){m_groupSize=v;m_sectionViewers[2]->setGroupSize(v);}

bool ViewerPanel::loadProject(){
    QString *pathPtr=m_projectPathPtr.exchange(nullptr);if(nullptr==pathPtr){return false;}
    QString path=*pathPtr;delete pathPtr;Common *c=Common::i();

    QVariantMap info;if(!Common::loadJson(path,info)){emit c->showMessage("Unable to load project file "+path,3000);return false;}

    setGroupSize(info["group_size"].toInt());QString imageFolder=info["image_path"].toString();
    m_pixelSize=info["voxel_size"].toFloat();m_thickness=m_pixelSize;//info["slide_thickness"].toFloat();

    QFileInfo fileInfo(path);QString imageFolderPath=fileInfo.dir().absolutePath()+"/"+imageFolder+"/";
    int maxWidth=-1,maxHeight=-1;qDeleteAll(m_images);m_images.clear();
    foreach(QVariant v,info["images"].toList()){
        QVariantMap v1=v.toMap();Image *image=new Image;image->index=v1["index"].toInt();image->groupIndex=(image->index-1)/m_groupSize+1;
        image->size=QSize(v1["width"].toInt(),v1["height"].toInt());
        image->filePath=imageFolderPath+v1["file_name"].toString();m_images.append(image);
        maxWidth=std::max(image->size.width(),maxWidth);maxHeight=std::max(image->size.height(),maxHeight);
    }
    if(m_images.empty()){emit c->showMessage("No image",3000);return false;}

    m_totalImageSize=QSize(maxWidth,maxHeight);

    m_imageFolderPath=imageFolderPath;
    emit c->setProjectFileName(fileInfo.fileName());

    QVariantMap params=info["freesia_project"].toMap();
    if(!params.empty()){        
        QVariantMap transformParam3d=params["transform_3d"].toMap();
        QStringList rotations=transformParam3d["rotation"].toString().split(" "),offsets=transformParam3d["translation"].toString().split(" "),scales=transformParam3d["scale"].toString().split(" ");
        if(rotations.length()==2){m_params3d->rotation[0]=rotations[0].toDouble();m_params3d->rotation[1]=rotations[1].toDouble();}
        if(offsets.length()==3){m_params3d->offset[0]=offsets[0].toDouble();m_params3d->offset[1]=offsets[1].toDouble();m_params3d->offset[2]=offsets[2].toDouble();}
        if(scales.length()==3){m_params3d->scale[0]=scales[0].toDouble();m_params3d->scale[1]=scales[1].toDouble();m_params3d->scale[2]=scales[2].toDouble();}

        m_projectPath=path;m_bProjectPathValid=true;transform3d(true);
        TransformParameters *params3d=new TransformParameters;params3d->copy(*m_params3d);emit c->transformParams3dLoaded(params3d);

        QString warpFolderPath=fileInfo.dir().absoluteFilePath(params["warp_path"].toString())+"/";

        qDeleteAll(m_groups);m_groups.clear();
        QVariantList slices=params["transform_2d"].toList();
        foreach(QVariant v,slices){
            QVariantMap v1=v.toMap();Group *slice=new Group;int index=v1["group_index"].toInt();slice->param.sliceIndex=index;m_groups[index]=slice;
            QStringList rotations=v1["rotation"].toString().split(" "),offsets=v1["translation"].toString().split(" "),scales=v1["scale"].toString().split(" ");
            if(rotations.length()==1){slice->param.rotation[2]=rotations[0].toDouble();}
            if(offsets.length()==2){slice->param.offset[0]=offsets[0].toDouble();slice->param.offset[1]=offsets[1].toDouble();}
            if(scales.length()==2){slice->param.scale[0]=scales[0].toDouble();slice->param.scale[1]=scales[1].toDouble();}

            QString warpPath=warpFolderPath+QString::number(index)+".tif";
            slice->warpField=cv::imread(warpPath.toStdString(),-1);
//            if(!slice->warpField.empty()){qDebug()<<index<<warpPath<<slice->warpField.cols<<slice->warpField.rows;}
        }

        m_sectionViewers[2]->importAllMarkers(params["warp_markers"].toList());
    }else{emit c->setEditedState(true);}

    loadImages();return true;
}
void ViewerPanel::onLoadProject(){
    QString path=QFileDialog::getOpenFileName(nullptr,"Open project file","","(*.json)");if(path.isEmpty()){return;}
    delete m_projectPathPtr.exchange(new QString(path));
}

void ViewerPanel::saveProject(){
    QString *pathPtr=m_projectPath2Save.exchange(nullptr);if(nullptr==pathPtr){return;}
    QString path=*pathPtr;delete pathPtr;if(!path.isEmpty()){m_projectPath=path;m_bProjectPathValid=true;}

    QFileInfo fileInfo(m_projectPath);QDir rootDir=fileInfo.dir(),imageDir(m_imageFolderPath);

    QVariantMap info;QVariantList images;
    info["group_size"]=m_groupSize;info["image_path"]=rootDir.relativeFilePath(m_imageFolderPath);
    info["voxel_size"]=m_pixelSize;//info["slide_thickness"]=m_thickness;

    foreach(Image *image,m_images){
        QVariantMap v;v["index"]=image->index;v["width"]=image->size.width();v["height"]=image->size.height();
        v["file_name"]=imageDir.relativeFilePath(image->filePath);images.append(v);
    }
    info["images"]=images;

    QVariantMap params,transformParam3d;
    transformParam3d["rotation"]=QString("%1 %2").arg(QString::number(m_params3d->rotation[0]),QString::number(m_params3d->rotation[1]));
    transformParam3d["translation"]=QString("%1 %2 %3").arg(QString::number(m_params3d->offset[0]),QString::number(m_params3d->offset[1]),QString::number(m_params3d->offset[2]));
    transformParam3d["scale"]=QString("%1 %2 %3").arg(QString::number(m_params3d->scale[0]),QString::number(m_params3d->scale[1]),QString::number(m_params3d->scale[2]));
    params["transform_3d"]=transformParam3d;

    QVariantList localInfos;
    foreach(Group *p,m_groups){
        QVariantMap v;v["rotation"]=QString("%1").arg(QString::number(p->param.rotation[2]));
        v["translation"]=QString("%1 %2").arg(QString::number(p->param.offset[0]),QString::number(p->param.offset[1]));
        v["scale"]=QString("%1 %2").arg(QString::number(p->param.scale[0]),QString::number(p->param.scale[1]));
        v["group_index"]=p->param.sliceIndex;localInfos.append(v);
    }

    params["warp_markers"]=m_sectionViewers[2]->exportAllMarkers();
    QString warpFolderName="deformation",warpFolderPath=fileInfo.dir().absoluteFilePath(warpFolderName)+"/";
    QDir().mkpath(warpFolderPath);exportWarpFields(warpFolderPath);params["warp_path"]=warpFolderName;

    params["transform_2d"]=localInfos;info["freesia_project"]=params;

    bool bOK=Common::saveJson(m_projectPath,info);Common *c=Common::i();
    if(bOK){
        emit c->showMessage("Project has been saved at "+fileInfo.fileName(),1000);
        emit c->setProjectFileName(fileInfo.fileName());emit c->setEditedState(false);
        m_params3dSaved->copy(*m_params3d);foreach(Group *p,m_groups){p->paramSaved.copy(p->param);}
    }else{emit c->showMessage("Unable to save project to "+m_projectPath,3000);}
}
void ViewerPanel::onSaveProject(bool bSaveAs){
    QString projectPath;
    if(bSaveAs||!m_bProjectPathValid.load()){//m_projectPath.isEmpty()
        QString path=QFileDialog::getSaveFileName(nullptr,"Save as project","","(*.json)");
        if(!path.isEmpty()){projectPath=path;}else{return;}
    }
    delete m_projectPath2Save.exchange(new QString(projectPath));
}

void ViewerPanel::loadImages(){
    Common *c=Common::i();BrainRegionModel *model=BrainRegionModel::i();

    int width=m_totalImageSize.width(),height=m_totalImageSize.height();
    size_t bufferSize=width*height*2;int number=m_images.length(),count=0;
    char *buffer=(char*)malloc(bufferSize*number),*pBuffer=buffer;
    foreach(Image *image,m_images){
        count++;emit c->showMessage(QString("Loading image %1/%2").arg(QString::number(count),QString::number(number)));
        Mat m1=imread(image->filePath.toStdString(),-1);
        if(m1.type()==CV_8UC1){m1.convertTo(m1,CV_16UC1,3.92);}
        if(m1.type()!=CV_16UC1){continue;}

        flip(m1,m1,0);
        image->data=m1;memcpy(pBuffer,m1.data,bufferSize);pBuffer+=bufferSize;
        if(!m_groups.contains(image->groupIndex)){Group *p=new Group;p->param.sliceIndex=image->groupIndex;m_groups[image->groupIndex]=p;}
    }
    cv::Point3f imageVoxelSize(m_pixelSize,m_pixelSize,m_thickness);
    m_volumeViewer->setBuffer(buffer,cv::Point3i(width,height,number),imageVoxelSize);
    free(buffer);emit c->showMessage("Done",500);

    emit m_sectionViewers[0]->imageNumberChanged(width);
    emit m_sectionViewers[1]->imageNumberChanged(height);
    emit m_sectionViewers[2]->imageNumberChanged(m_images.length());

    cv::Point3i modelSize;float modelVoxelSize;char *modelVoxels=(char*)model->getVoxels(modelSize,modelVoxelSize);
    m_volumeViewer->setBuffer3(modelVoxels,modelSize,Point3f(modelVoxelSize,modelVoxelSize,modelVoxelSize));
}

bool ViewerPanel::importDirectory(){
    ImportParams *ptr=m_importParams.exchange(nullptr);if(nullptr==ptr){return false;}

    m_pixelSize=ptr->voxels[0];m_thickness=ptr->voxels[1];
    setGroupSize(ptr->groupSize);

    QStringList filePaths;Common *c=Common::i();//,fileNames
    QDirIterator it(ptr->path,QStringList()<<"*.tif"<<"*.tiff"<<"*.jpg"<<"*.bmp",QDir::Files,QDirIterator::NoIteratorFlags);
    while(it.hasNext()){it.next();filePaths.append(it.filePath());}

    Size size;int count=0;qDeleteAll(m_images);m_images.clear();
    foreach(QString filePath,filePaths){
        count++;if(size.empty()){Mat m=imread(filePath.toStdString(),-1);size=m.size();}

        Image *image=new Image;image->filePath=filePath;image->index=count;image->size=QSize(size.width,size.height);
        image->groupIndex=(count-1)/m_groupSize+1;

        m_images.append(image);
    }
    if(m_images.empty()){emit c->showMessage("Unable to load images in directory",3000);return false;}
    m_totalImageSize=QSize(size.width,size.height);m_imageFolderPath=ptr->path;

    loadImages();

    emit c->setProjectFileName(QFileInfo(ptr->path).fileName());emit c->setEditedState(true);
    delete ptr;return true;
}
void ViewerPanel::onImportDirectory(){
    ImportDialog dialog;if(dialog.exec()!=QDialog::Accepted){return;}
    ImportParams *p=new ImportParams;if(dialog.getParameters(*p)){delete m_importParams.exchange(p);}else{delete p;}
}

void ViewerPanel::importSpots(){
    QString *pathPtr=m_spotsPath2Import.exchange(nullptr);if(nullptr==pathPtr){return;}
    QString path=*pathPtr;delete pathPtr;

    foreach(Image *image,m_images){
        QString spotsPath=path+"/"+QFileInfo(image->filePath).fileName()+".csv";
        image->spotsPath=QFile(spotsPath).exists()?spotsPath:"";image->bSpotsLoaded=false;
    }
    m_bImageUpdated=true;
}
void ViewerPanel::onImportSpots(){
    QString path=QFileDialog::getExistingDirectory(nullptr,"Select a directory to import spots","",
                                                   QFileDialog::ShowDirsOnly|QFileDialog::DontResolveSymlinks);
    if(!path.isEmpty()){delete m_spotsPath2Import.exchange(new QString(path));}
}

// duplicated function in SectionViewer, remove it later
inline void rotatePts(const double p[3],const double c[3],double angle,double output[3]){
    cv::Point2d ip(p[0],p[1]),ic(c[0],c[1]);
    double x=ip.x-ic.x,y=ip.y-ic.y,rotation=angle*3.14/180;
    double x1=x*cos(rotation)-y*sin(rotation),y1=x*sin(rotation)+y*cos(rotation);
    x1+=ic.x;y1+=ic.y;output[0]=x1;output[1]=y1;output[2]=0;
}
void ViewerPanel::exportCellCounting(){
    QString *pathPtr=m_exportPath.exchange(nullptr);if(nullptr==pathPtr){return;}
    QString path=*pathPtr;delete pathPtr;Common *c=Common::i();

    QMap<int,Mat> allWarpFields;
    int index=0,imageNumber=m_images.length();size_t colorCounts[65536];
    foreach(Image *image,m_images){
        emit c->showMessage("Export cell counting result for image "+QString::number(index+1)+"/"+QString::number(imageNumber));

        OutputImageData data;m_volumeViewer->getImageByIndex(2,index,data);index++;
        if(nullptr==data.imageData||nullptr==data.modelData){continue;}
        loadImageSpots(image);//if(image->spots.empty()){continue;}

        int pSizeModel[3]={0};data.modelData->GetDimensions(pSizeModel);
        int w=pSizeModel[0],h=pSizeModel[1];if(w<=0||h<=0){continue;}
        uint16_t *pDataModel=(uint16_t*)data.modelData->GetScalarPointer();
        double *originModel=data.modelData->GetOrigin(),*spacingModel=data.modelData->GetSpacing();

        int pSizeImage[3]={0};data.imageData->GetDimensions(pSizeImage);
        double *originImage=data.imageData->GetOrigin(),*spacingImage=data.imageData->GetSpacing(),rotationImage=0;
        double spacingImageRaw[2]={spacingImage[0],spacingImage[1]};

        int groupIndex=image->groupIndex;Group *slice=m_groups.value(groupIndex,nullptr);
        if(nullptr!=slice){
            for(int k=0;k<2;k++){
                originImage[k]+=slice->param.offset[k]-pSizeImage[k]*(slice->param.scale[k]-1)*spacingImage[k]/2;
                spacingImage[k]*=slice->param.scale[k];
            }
            rotationImage=slice->param.rotation[2];
        }
        Mat warpField;int w1=pSizeImage[0],h1=pSizeImage[1];
        if(allWarpFields.contains(groupIndex)){warpField=allWarpFields[groupIndex];}
        else{buildWarpField(groupIndex,warpField,false);allWarpFields[groupIndex]=warpField;}
        bool bWarpValid=(warpField.cols==w1&&warpField.rows==h1&&warpField.type()==CV_16UC4);//(warpField.type()==CV_32FC2||

        double center[3]={originImage[0]+w1*spacingImage[0]/2,originImage[1]+h1*spacingImage[1]/2,0};
        memset(colorCounts,0,65536*8);

        Mat modelImage=Mat(Size(w,h),CV_16UC1,pDataModel).clone();

        foreach(cv::Point3d p,image->spots){
            double px=p.x/spacingImageRaw[0],py=pSizeImage[1]-1-p.y/spacingImageRaw[1];
            if(bWarpValid){
                int x1=round(px),y1=round(py);if(x1<0||x1>=w1||y1<0||y1>=h1){continue;}
                float *pOffset=(float*)warpField.data+(y1*w1+x1)*2;px=pOffset[0];py=pOffset[1];
            }

            double p1[3]={px*spacingImage[0]+originImage[0],py*spacingImage[1]+originImage[1],0},p2[3];
            rotatePts(p1,center,rotationImage,p2);

            int x=round((p2[0]-originModel[0])/spacingModel[0]),y=round((p2[1]-originModel[1])/spacingModel[1]);
            if(x>=0&&x<w&&y>=0&&y<h){colorCounts[*(y*w+x+pDataModel)]++;modelImage.at<ushort>(y,x)=1000;}
        }
        QVariantList regions;
        for(int i=0;i<=65535;i++){
            size_t count=colorCounts[i];if(0==count){continue;}
            QVariantMap v;v["color"]=i;v["number"]=count;regions.append(v);
        }
        QVariantMap info;info["regions"]=regions;//qDebug()<<regions.length()<<"regions";
        c->saveJson(path+"/"+QFileInfo(image->filePath).baseName()+".json",info);

        cv::flip(modelImage,modelImage,0);QString tifPath=path+"/images/";QDir().mkpath(tifPath);
        imwrite((tifPath+QFileInfo(image->filePath).baseName()+".tif").toStdString(),modelImage);
    }
    emit c->showMessage("Done",500);
}
void ViewerPanel::onExportCellCounting(){
    QString path=QFileDialog::getExistingDirectory(nullptr,"Select a directory for cell counting results","",
                                                   QFileDialog::ShowDirsOnly|QFileDialog::DontResolveSymlinks);
    if(!path.isEmpty()){delete m_exportPath.exchange(new QString(path));}
}
void ViewerPanel::exportPixelCounting(){
    QString *pathPtr=m_exportPixelPath.exchange(nullptr);if(nullptr==pathPtr){return;}
    QString path=*pathPtr;delete pathPtr;Common *c=Common::i();

    flsmio::ImageWriter mapWriter(path+"/map.tif"),imageWriter(path+"/image.tif");

//    QMap<int,Mat> allWarpFields;
    int index=0,imageNumber=m_images.length();size_t colorCounts[65536];
    foreach(Image *image,m_images){
        emit c->showMessage("Export cell counting result for image "+QString::number(index+1)+"/"+QString::number(imageNumber));

        OutputImageData data;m_volumeViewer->getImageByIndex(2,index,data);index++;
        if(nullptr==data.imageData||nullptr==data.modelData){continue;}

        int pSizeModel[3]={0};data.modelData->GetDimensions(pSizeModel);
        int w=pSizeModel[0],h=pSizeModel[1];if(w<=0||h<=0){continue;}
        uint16_t *pDataModel=(uint16_t*)data.modelData->GetScalarPointer();
        double *originModel=data.modelData->GetOrigin(),*spacingModel=data.modelData->GetSpacing();

        int pSizeImage[3]={0};data.imageData->GetDimensions(pSizeImage);
        double *originImage=data.imageData->GetOrigin(),*spacingImage=data.imageData->GetSpacing(),rotationImage=0;
//        double spacingImageRaw[2]={spacingImage[0],spacingImage[1]};

        int groupIndex=image->groupIndex;Group *slice=m_groups.value(groupIndex,nullptr);
        if(nullptr!=slice){
            for(int k=0;k<2;k++){
                originImage[k]+=slice->param.offset[k]-pSizeImage[k]*(slice->param.scale[k]-1)*spacingImage[k]/2;
                spacingImage[k]*=slice->param.scale[k];
            }
            rotationImage=slice->param.rotation[2];
        }
        Mat warpField=m_groups[groupIndex]->warpField;int w1=pSizeImage[0],h1=pSizeImage[1];
//        if(allWarpFields.contains(groupIndex)){warpField=allWarpFields[groupIndex];}
//        else{buildWarpField(groupIndex,warpField,false);allWarpFields[groupIndex]=warpField;}
        bool bWarpValid=(warpField.cols==w1&&warpField.rows==h1&&warpField.type()==CV_16UC4);//(warpField.type()==CV_32FC2||

        double center[3]={originImage[0]+w1*spacingImage[0]/2,originImage[1]+h1*spacingImage[1]/2,0};
        memset(colorCounts,0,65536*8);

        Mat countImage=Mat::zeros(Size(w,h),CV_16UC1);

        uint16_t *pData=pDataModel,*pOutput=(uint16_t*)countImage.data;size_t offset=0;
        for(int j=0;j<h;j++){
            for(int i=0;i<w;i++,pData++,pOutput++,offset++){
                double p2[3]={i*spacingModel[0]+originModel[0],j*spacingModel[1]+originModel[1],0},p1[3];
                rotatePts(p2,center,-rotationImage,p1);

                double px=(p1[0]-originImage[0])/spacingImage[0],py=(p1[1]-originImage[1])/spacingImage[1];
                if(bWarpValid){
                    int x1=round(px),y1=round(py);if(x1<0||x1>=w1||y1<0||y1>=h1){continue;}
                    float *pOffset=(float*)warpField.data+(y1*w1+x1)*2;px=pOffset[0];py=pOffset[1];
                }

                int x=round(px),y=round(py);
                if(x>=0&&x<w1&&y>=0&&y<h1){
                    uint16_t value=image->data.at<ushort>(y,x),weight=(value>200?(value-200):0);
                    *pOutput=value;colorCounts[*(offset+pDataModel)]+=weight;
                }
            }
        }

        QVariantList regions;
        for(int i=0;i<=65535;i++){
            size_t count=colorCounts[i];if(0==count){continue;}
            QVariantMap v;v["color"]=i;v["number"]=count;regions.append(v);
        }
        QVariantMap info;info["regions"]=regions;//qDebug()<<regions.length()<<"regions";
        c->saveJson(path+"/"+QFileInfo(image->filePath).baseName()+".json",info);

//        cv::flip(modelImage,modelImage,0);
//        QString tifPath=path+"/images/";QDir().mkpath(tifPath);
//        imwrite((tifPath+QFileInfo(image->filePath).baseName()+".tif").toStdString(),modelImage);

        // duplicated function in sectionviewer
        Mat modelImage=Mat::zeros(Size(w,h),CV_16UC1);
        uint16_t *pData2=pDataModel,*pData3=(uint16_t*)modelImage.data;//int w1=m.cols;
        for(int x=0,y=0;;){
            uint16_t *pData4=pData3+y*w+x;
            uint16_t v1=*pData2,v2=*(pData2+1),v3=*(pData2+w+1),v4=*(pData2+w);

            bool bOK1=(v1==v2),bOK2=(v2==v3),bOK3=(v3==v4),bOK4=(v4==v1);
            if(!bOK1){*(pData4+1)=1000;}
            if(!bOK2){*(pData4+w+2)=1000;}
            if(!bOK3){*(pData4+w*2+1)=1000;}
            if(!bOK4){*(pData4+w)=1000;}
            if((bOK1||bOK2||bOK3||bOK4)&&(bOK1!=bOK2&&bOK2!=bOK3&&bOK3!=bOK4&&bOK4!=bOK1)){*(pData4+w+1)=1000;}

            x++;pData2++;if(x==w-1){x=0;y++;pData2+=1;if(y==h-1){break;}}
        }

        cv::flip(modelImage,modelImage,0);mapWriter.addImage(modelImage);
        cv::flip(countImage,countImage,0);imageWriter.addImage(countImage);
    }
    emit c->showMessage("Done",500);
}
void ViewerPanel::onExportPixelCounting(){
    QString path=QFileDialog::getExistingDirectory(nullptr,"Select a directory for pixel counting results","",
                                                   QFileDialog::ShowDirsOnly|QFileDialog::DontResolveSymlinks);
    if(!path.isEmpty()){delete m_exportPixelPath.exchange(new QString(path));}
}

void ViewerPanel::mergeCellCounting(){
    QString *pathPtr=m_mergePath.exchange(nullptr);if(nullptr==pathPtr){return;}
    QString path=*pathPtr;delete pathPtr;Common *c=Common::i();

    size_t colorCounts[65536]={0};emit c->showMessage("Combining all the cell-counting results");
    QDirIterator it(path,QStringList()<<"*.json",QDir::Files,QDirIterator::Subdirectories);
    while(it.hasNext()){
        it.next();QVariantMap info;if(!c->loadJson(it.filePath(),info)){continue;}
        QVariantList regions=info["regions"].toList();if(regions.empty()){continue;}
        foreach(QVariant v,regions){
            QVariantMap v1=v.toMap();bool bOK1,bOK2;
            size_t color=v1["color"].toULongLong(&bOK1),number=v1["number"].toULongLong(&bOK2);
            if(bOK1&&bOK2&&color<=65535){colorCounts[color]+=number;}
        }
    }

    QString outputName="cell-counting.csv",outputPath=path+"/"+outputName;
    if(BrainRegionModel::i()->dumpCellCounting(colorCounts,outputPath)){
        emit c->showMessage("Result has been saved at "+outputName,2000);
    }else{emit c->showMessage("Failed",2000);}
}
void ViewerPanel::onMergeCellCounting(){
    QString path=QFileDialog::getExistingDirectory(nullptr,"Select a directory to merge cell counting results","",
                                                   QFileDialog::ShowDirsOnly|QFileDialog::DontResolveSymlinks);
    if(!path.isEmpty()){delete m_mergePath.exchange(new QString(path));}
}

bool ViewerPanel::selectRegion(){
    int color=m_regionNextColor.exchange(-1);if(color<0||m_images.empty()){return false;}

    BrainRegionModel *model=BrainRegionModel::i();
    Point3i size,voxelSize;model->getModelSize(size,voxelSize);
    char *data=(0==color?nullptr:model->getRegionVoxelByColor(color));
    m_volumeViewer->setBuffer2(data,size,voxelSize);if(nullptr!=data){free(data);}

    m_bImageUpdated=true;return true;
}

void ViewerPanel::transform3d(bool bForced){
    if(!bForced){
        TransformParameters *ptr=Common::i()->p_params3d.exchange(nullptr);if(nullptr==ptr){return;}
        m_params3d->copy(*ptr);delete ptr;emit Common::i()->setEditedState(true);
    }
    m_volumeViewer->updateTranform3d(*m_params3d);m_bImageUpdated=true;
}

void ViewerPanel::transform2d(){
    TransformParameters *ptr=Common::i()->p_params2d.exchange(nullptr);if(nullptr==ptr){return;}
    Group *slice=m_groups.value(ptr->sliceIndex,nullptr);
    if(nullptr==slice){slice=new Group;m_groups[ptr->sliceIndex]=slice;}
    slice->param.copy(*ptr);m_bImageUpdated=true;//emit Common::i()->setEditedState(true);
}
