#include "sectionviewer.h"
#include "stackslider.h"
#include "common.h"

#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkImageProperty.h>
#include <vtkLookupTable.h>
#include <vtkInteractorStyleImage.h>
#include <vtkColorTransferFunction.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkImageImport.h>
#include <vtkPiecewiseFunction.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkSphereSource.h>
#include <vtkLineSource.h>
#include <vtkCamera.h>
#include <vtkMatrix4x4.h>
#include <vtkTransform.h>

#include <QKeyEvent>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QDebug>

bool g_bVolumnPicking=false,g_bPropPicking=false;
static double s_colorNode[3]={255,0,0};
static double s_colorCandidate[3]={0,0,255};
static double s_colorSelected[3]={255,255,0};
static double s_colorSpots[3]={135,206,235};

struct Node{
    double p1[3],p2[3],p1Raw[3],p2Raw[3];int num,imageIndex;
    vtkSmartPointer<vtkActor> actors[2];
    Node():num(0),imageIndex(-1){p1[2]=0;p2[2]=1;p1Raw[2]=0;p2Raw[2]=1;}
};

class ImageInteractorStyle:public vtkInteractorStyleImage{
    ImageInteractorStyle(){}
    void getMousePos(int &x,int &y){x=this->Interactor->GetEventPosition()[0];y=this->Interactor->GetEventPosition()[1];}
public:
    static ImageInteractorStyle* New();
    vtkTypeMacro(ImageInteractorStyle, vtkInteractorStyleImage);

    int p_type;SectionViewer *p_viewer;
protected:
    void OnChar() override{}

    void OnMouseMove() override{
        if(g_bVolumnPicking){}
        else if(g_bPropPicking){
            int x,y;getMousePos(x,y);p_viewer->selectPoint(x,y);
        }else{vtkInteractorStyleImage::OnMouseMove();return;}
    }

    void OnLeftButtonUp() override{if(VTKIS_PAN==State){EndPan();}}
    void OnLeftButtonDown() override{
        if(g_bVolumnPicking){
            int x,y;getMousePos(x,y);p_viewer->addPoint(x,y);
        }else if(g_bPropPicking){
            p_viewer->selectPoint();
        }else{
            if(Interactor->GetShiftKey()){StartPan();}
            else if(Interactor->GetControlKey()){int x,y;getMousePos(x,y);p_viewer->selectRegion(x,y);}
            else if(1==GetInteractor()->GetRepeatCount()){emit Common::i()->toggleFullscreen(p_type);}
        }
    }
    void OnMiddleButtonDown() override{
        if(1==GetInteractor()->GetRepeatCount()){p_viewer->resetViewer();}
        else{vtkInteractorStyleImage::OnMiddleButtonDown();}
    }
};
vtkStandardNewMacro(ImageInteractorStyle);

SectionViewer::SectionViewer(int type):m_imageViewer(new QVTKOpenGLWidget),m_type(type),m_nodes(nullptr),m_modelFactor(2),
    m_nextImageIndex(-1),m_imageIndex(-1),m_bViewerInited(false),m_currentNode(nullptr),m_candidateNode(nullptr),m_selectNode(nullptr),m_groupSize(1){

    QVBoxLayout *lyt=new QVBoxLayout;setLayout(lyt);lyt->setContentsMargins(0,0,0,0);lyt->setSpacing(0);
    lyt->addWidget(m_imageViewer,1);
    m_imageSlider=new StackSlider;m_imageSlider->showPlayButton(false);lyt->addWidget(m_imageSlider);

    connect(this,&SectionViewer::imageNumberChanged,this,[this](int number){
        m_imageSlider->setRange(1,number);int v=(1+number)/2;m_imageSlider->setValueSimple(v);setImageIndex(v);
    },Qt::QueuedConnection);
    connect(m_imageSlider,&StackSlider::valueChanged,this,&SectionViewer::setImageIndex);

    m_renderer=vtkSmartPointer<vtkRenderer>::New();m_imageViewer->GetRenderWindow()->AddRenderer(m_renderer);
    vtkSmartPointer<ImageInteractorStyle> interactor=vtkSmartPointer<ImageInteractorStyle>::New();
    m_imageViewer->GetInteractor()->SetInteractorStyle(interactor);
    interactor->p_type=type;interactor->p_viewer=this;

    connect(this,&SectionViewer::imageUpdated,this,[this](){
        m_imageActorMutex.lock();

        m_renderer->RemoveActor(m_imageActors[0]);
        if(nullptr!=m_imageActors[1]){m_renderer->AddActor(m_imageActors[1]);}
        m_imageActors[0]=m_imageActors[1];m_imageActors[1]=nullptr;

        m_renderer->RemoveActor(m_modelActors[0]);
        if(nullptr!=m_modelActors[1]){m_renderer->AddActor(m_modelActors[1]);}
        m_modelActors[0]=m_modelActors[1];m_modelActors[1]=nullptr;

        m_renderer->RemoveActor(m_selectActors[0]);
        if(nullptr!=m_selectActors[1]){m_renderer->AddActor(m_selectActors[1]);}
        m_selectActors[0]=m_selectActors[1];m_selectActors[1]=nullptr;

        m_imageIndex2=m_nextImageIndex2;m_groupIndex=m_nextGroupIndex;
        m_modelImage=m_nextModelImage;

        m_imageActorMutex.unlock();

        if(m_groupIndex>=0){
            QList<Node*> *nodes=nullptr;
            nodes=m_allNodes.value(m_groupIndex,nullptr);
            if(nullptr==nodes){
                nodes=new QList<Node*>();
                m_nodesMutex.lock();m_allNodes[m_groupIndex]=nodes;m_nodesMutex.unlock();
            }
            updateNodesPoisition(nodes);
        }

        if(!m_bViewerInited){m_renderer->ResetCamera();m_bViewerInited=true;}
        m_imageViewer->GetRenderWindow()->Render();
    },Qt::QueuedConnection);

    m_colorTable=vtkSmartPointer<vtkLookupTable>::New();
    m_colorTable->SetRange(0,1000);m_colorTable->SetValueRange(0.0,1.0);m_colorTable->SetSaturationRange(0,0);m_colorTable->SetHueRange(0,0);
    m_colorTable->SetRampToLinear();m_colorTable->Build();
    m_colorMap2=vtkSmartPointer<vtkImageMapToColors>::New();m_colorMap2->SetLookupTable(m_colorTable);

    Common *c=Common::i();
    connect(c,&Common::sliceContrastChanged,[this](int v){
        v=std::max(100,v);m_colorTable->SetRange(0,v);m_colorTable->Build();m_colorMap2->Update();m_imageViewer->GetRenderWindow()->Render();
    });
    if(2==m_type){connect(c,&Common::removeCurrentMarkers,this,&SectionViewer::removeCurrentMarkers);}
}

void SectionViewer::setGroupSize(int groupSize){m_groupSize=groupSize;}

inline vtkSmartPointer<vtkActor> getActor(vtkAlgorithmOutput *output,double color[3]){
    vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();mapper->SetInputConnection(output);
    vtkSmartPointer<vtkProperty> prop=vtkSmartPointer<vtkProperty>::New();prop->SetColor(color);
    vtkSmartPointer<vtkActor> actor=vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);actor->SetProperty(prop);return actor;
}
inline vtkSmartPointer<vtkActor> getNodeActor(double *pos,double color[3]){
    vtkSmartPointer<vtkSphereSource> sphereSource=vtkSmartPointer<vtkSphereSource>::New();
    sphereSource->SetCenter(pos);sphereSource->SetRadius(15);
    return getActor(sphereSource->GetOutputPort(),color);//s_colorSelected
}

void SectionViewer::updateResliceImage(const cv::Mat &image, //const QList<cv::Point2d> &spots,
                                       double *origin, double *spacing, double rotation, int imageIndex, int groupIndex){
    vtkSmartPointer<vtkImageImport> importer=vtkSmartPointer<vtkImageImport>::New();//qDebug()<<imageSize<<imageNumber;
    importer->SetDataScalarTypeToUnsignedShort();importer->SetWholeExtent(0,image.cols-1,0,image.rows-1,0,0);
    importer->SetNumberOfScalarComponents(1);importer->SetDataExtentToWholeExtent();importer->SetImportVoidPointer(image.data);
    importer->Update();

    vtkSmartPointer<vtkImageData> imageData=vtkSmartPointer<vtkImageData>::New();imageData->DeepCopy(importer->GetOutput());
    imageData->SetSpacing(spacing);

    m_colorMap2->SetInputData(imageData);m_colorMap2->Update();

    vtkSmartPointer<vtkImageActor> imgActor=vtkSmartPointer<vtkImageActor>::New();
    imgActor->SetInputData(m_colorMap2->GetOutput());imgActor->SetPosition(origin);
    imgActor->SetOrigin(image.cols*spacing[0]/2,image.rows*spacing[1]/2,0);
    if(rotation!=0){imgActor->SetOrientation(0,0,rotation);}

    m_imageActorMutex.lock();
    m_imageActors[1]=imgActor;
    m_nextGroupIndex=groupIndex;m_nextImageIndex2=imageIndex;
    m_imageActorMutex.unlock();
}

void SectionViewer::updateSelectImage(const cv::Mat &image,double *origin,double *spacing){
    vtkSmartPointer<vtkImageImport> importer=vtkSmartPointer<vtkImageImport>::New();
    importer->SetDataScalarTypeToUnsignedChar();importer->SetWholeExtent(0,image.cols-1,0,image.rows-1,0,0);
    importer->SetNumberOfScalarComponents(1);importer->SetDataExtentToWholeExtent();importer->SetImportVoidPointer(image.data);
    importer->Update();

    vtkSmartPointer<vtkImageData> imageData=vtkSmartPointer<vtkImageData>::New();imageData->DeepCopy(importer->GetOutput());
    imageData->SetSpacing(spacing);

    vtkSmartPointer<vtkLookupTable> colorTable=vtkSmartPointer<vtkLookupTable>::New();
    colorTable->SetRange(0,255);colorTable->SetValueRange(0.0,1.0);colorTable->SetAlphaRange(0,0.5);
    colorTable->SetSaturationRange(0,1);colorTable->SetHueRange(0,0.08);
    colorTable->SetRampToLinear();colorTable->Build();

    vtkSmartPointer<vtkImageMapToColors> colorMap=vtkSmartPointer<vtkImageMapToColors>::New();
    colorMap->SetLookupTable(colorTable);colorMap->SetInputData(imageData);colorMap->Update();

    vtkSmartPointer<vtkImageActor> imgActor=vtkSmartPointer<vtkImageActor>::New();
    imgActor->SetInputData(colorMap->GetOutput());imgActor->SetPosition(origin);

    m_imageActorMutex.lock();m_selectActors[1]=imgActor;m_imageActorMutex.unlock();
}

void SectionViewer::updateModelImage(const cv::Mat &image,double *origin,double *spacing){
    int w=image.cols,h=image.rows;if(image.empty()){return;}
    cv::Mat m=cv::Mat::zeros(cv::Size(2*w,2*h),CV_8UC1);
    double spacing1[3]={spacing[0]/m_modelFactor,spacing[1]/m_modelFactor,spacing[2]/m_modelFactor};

    uint16_t *pData2=(uint16_t*)image.data;uint8_t *pData3=m.data;int w1=m.cols;
    for(int x=0,y=0;;){
        uint8_t *pData4=pData3+2*y*w1+2*x;
        uint16_t v1=*pData2,v2=*(pData2+1),v3=*(pData2+w+1),v4=*(pData2+w);

        bool bOK1=(v1==v2),bOK2=(v2==v3),bOK3=(v3==v4),bOK4=(v4==v1);
        if(!bOK1){*(pData4+1)=255;}
        if(!bOK2){*(pData4+w1+2)=255;}
        if(!bOK3){*(pData4+w1*2+1)=255;}
        if(!bOK4){*(pData4+w1)=255;}
        if((bOK1||bOK2||bOK3||bOK4)&&(bOK1!=bOK2&&bOK2!=bOK3&&bOK3!=bOK4&&bOK4!=bOK1)){*(pData4+w1+1)=255;}

        x++;pData2++;if(x==w-1){x=0;y++;pData2+=1;if(y==h-1){break;}}
    }

    vtkSmartPointer<vtkImageImport> importer=vtkSmartPointer<vtkImageImport>::New();
    importer->SetDataScalarTypeToUnsignedChar();importer->SetWholeExtent(0,m.cols-1,0,m.rows-1,0,0);
    importer->SetNumberOfScalarComponents(1);importer->SetDataExtentToWholeExtent();importer->SetImportVoidPointer(m.data);
    importer->Update();

    vtkSmartPointer<vtkImageData> imageData=vtkSmartPointer<vtkImageData>::New();imageData->DeepCopy(importer->GetOutput());
    imageData->SetSpacing(spacing1);

    vtkSmartPointer<vtkLookupTable> colorTable=vtkSmartPointer<vtkLookupTable>::New();
    colorTable->SetRange(0,255);colorTable->SetValueRange(0.0,1.0);colorTable->SetAlphaRange(0,1);
    colorTable->SetSaturationRange(0,1);colorTable->SetHueRange(0,0.08);

    colorTable->SetRampToLinear();colorTable->Build();

    vtkSmartPointer<vtkImageMapToColors> colorMap=vtkSmartPointer<vtkImageMapToColors>::New();
    colorMap->SetLookupTable(colorTable);colorMap->SetInputData(imageData);colorMap->Update();

    vtkSmartPointer<vtkImageActor> imgActor=vtkSmartPointer<vtkImageActor>::New();
    imgActor->SetInputData(colorMap->GetOutput());imgActor->SetPosition(origin);

    m_imageActorMutex.lock();
    m_modelActors[1]=imgActor;m_nextModelImage=image.clone();
    m_imageActorMutex.unlock();
}

void SectionViewer::setImageIndex(int index){m_imageIndex=index-1;m_nextImageIndex=m_imageIndex.load();}
bool SectionViewer::getUpdatedIndex(int &index){index=m_nextImageIndex.exchange(-1);return index>=0;}
bool SectionViewer::getImageIndex(int &index){index=m_imageIndex;return index>=0;}

void SectionViewer::resetViewer(){if(m_bViewerInited){m_renderer->ResetCamera();m_imageViewer->GetRenderWindow()->Render();}}

inline vtkSmartPointer<vtkActor> getLineActor(double p1[3],double p2[3],double color[3]){
    vtkSmartPointer<vtkLineSource> line=vtkSmartPointer<vtkLineSource>::New();
    line->SetPoint1(p1);line->SetPoint2(p2);return getActor(line->GetOutputPort(),color);
}

void SectionViewer::getPointPosition(int x, int y,double picked1[3]){
    m_renderer->SetDisplayPoint(x,y,0);m_renderer->DisplayToWorld();double *picked=m_renderer->GetWorldPoint();
    double cameraPos[3];m_renderer->GetActiveCamera()->GetPosition(cameraPos);

    double f=cameraPos[2]/(cameraPos[2]-picked[2]);
    picked1[2]={0};for(int i=0;i<2;i++){picked1[i]=cameraPos[i]+(picked[i]-cameraPos[i])*f;}
}
inline void rotatePts(const double p[3],const double c[3],double angle,double output[3]){
    cv::Point2d ip(p[0],p[1]),ic(c[0],c[1]);
    double x=ip.x-ic.x,y=ip.y-ic.y,rotation=angle*3.14/180;
    double x1=x*cos(rotation)-y*sin(rotation),y1=x*sin(rotation)+y*cos(rotation);
    x1+=ic.x;y1+=ic.y;output[0]=x1;output[1]=y1;output[2]=0;
}
void SectionViewer::addPoint(int x, int y){
    int imageIndex=m_imageIndex2;//.load();
    if(imageIndex<0||nullptr==m_nodes||nullptr==m_imageActors[0]||nullptr==m_modelActors[0]){return;}

    double picked1[3];getPointPosition(x,y,picked1);

    if(nullptr==m_candidateNode){m_candidateNode=new Node;}
    if(0==m_candidateNode->num){
        vtkSmartPointer<vtkImageActor> act=m_imageActors[0];

        double *pos=act->GetPosition(),*spacing=act->GetInput()->GetSpacing(),*origin=act->GetOrigin();
        double origin2[2]={pos[0]+origin[0],pos[1]+origin[1]};
        double unRotPoint[3];rotatePts(picked1,origin2,-act->GetOrientation()[2],unRotPoint);

        double picked2[2]={(unRotPoint[0]-pos[0])/spacing[0],(unRotPoint[1]-pos[1])/spacing[1]};

        m_candidateNode->p1Raw[0]=picked1[0];m_candidateNode->p1Raw[1]=picked1[1];
        m_candidateNode->p1[0]=picked2[0];m_candidateNode->p1[1]=picked2[1];
        vtkSmartPointer<vtkActor> actor=getNodeActor(picked1,s_colorCandidate);m_renderer->AddActor(actor);
        m_candidateNode->actors[0]=actor;m_candidateNode->num=1;m_candidateNode->imageIndex=imageIndex;
    }else{
        if(m_candidateNode->imageIndex!=imageIndex){emit Common::i()->showMessage("Points should be contained in the same image",2000);return;}

        m_renderer->RemoveActor(m_candidateNode->actors[1]);
        m_candidateNode->p2[0]=picked1[0];m_candidateNode->p2[1]=picked1[1];m_candidateNode->num=2;

        vtkSmartPointer<vtkLineSource> line=vtkSmartPointer<vtkLineSource>::New();
        line->SetPoint1(m_candidateNode->p1Raw);line->SetPoint2(m_candidateNode->p2);

        vtkSmartPointer<vtkActor> actor1=getActor(line->GetOutputPort(),s_colorCandidate);
        m_candidateNode->actors[1]=actor1;m_renderer->AddActor(actor1);
    }

    m_imageViewer->GetRenderWindow()->Render();
}

void SectionViewer::selectRegion(int x, int y){
    vtkSmartPointer<vtkImageActor> act=m_modelActors[0];if(nullptr==act){return;}
    double *pos=act->GetPosition(),*spacing=act->GetInput()->GetSpacing();
    double picked1[3];getPointPosition(x,y,picked1);
    int x1=round((picked1[0]-pos[0])/m_modelFactor/spacing[0]),y1=round((picked1[1]-pos[1])/m_modelFactor/spacing[1]),color=0;
    if(x1>=0&&y1>=0&&x1<m_modelImage.cols&&y1<m_modelImage.rows){color=m_modelImage.at<uint16_t>(y1,x1);}
    Common *c=Common::i();emit c->regionSelected(color);emit c->regionSelected2(color);
}

// Copied from https://blog.csdn.net/angelazy/article/details/38489293
inline double calcuDistance(double x,double y,double x1,double y1,double x2,double y2) {
    double cross = (x2 - x1) * (x - x1) + (y2 - y1) * (y - y1); if (cross <= 0) return ((x - x1) * (x - x1) + (y - y1) * (y - y1));//sqrt
    double d2 = (x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1); if (cross >= d2) return ((x - x2) * (x - x2) + (y - y2) * (y - y2));//sqrt
    double r = cross / d2; double px = x1 + (x2 - x1) * r; double py = y1 + (y2 - y1) * r; return ((x - px) * (x - px) + (y - py) * (y - py));//sqrt
}
void SectionViewer::selectPoint(int x, int y){
    if(nullptr==m_nodes||m_nodes->empty()){return;}

    double picked[3];getPointPosition(x,y,picked);
    double minDist=DBL_MAX;Node *closestNode=nullptr;
    QListIterator<Node*> iter(*m_nodes);
    while(iter.hasNext()){Node *p=iter.next();
        double d=calcuDistance(picked[0],picked[1],p->p1[0],p->p1[1],p->p2[0],p->p2[1]);
        if(d<minDist){minDist=d;closestNode=p;}
    }

    if(nullptr!=m_selectNode&&m_currentNode!=m_selectNode){for(int i=0;i<2;i++){m_selectNode->actors[i]->GetProperty()->SetColor(s_colorNode);}}
    if(nullptr!=closestNode){for(int i=0;i<2;i++){closestNode->actors[i]->GetProperty()->SetColor(s_colorSelected);}}
    m_selectNode=closestNode;m_imageViewer->GetRenderWindow()->Render();
}
void SectionViewer::selectPoint(){
    if(nullptr==m_selectNode){return;}
    changeCurrentNode(m_selectNode);
    m_imageSlider->setValueSimple(m_selectNode->imageIndex+1);
    m_selectNode=nullptr;
}

void SectionViewer::changeCurrentNode(Node *node){
    if(nullptr!=m_currentNode){
        for(int i=0;i<2;i++){m_currentNode->actors[i]->GetProperty()->SetColor(s_colorNode);}
    }

    m_currentNode=node;
    if(nullptr!=node){
        for(int i=0;i<2;i++){node->actors[i]->GetProperty()->SetColor(s_colorSelected);}
    }

    m_imageViewer->GetRenderWindow()->Render();
}

inline bool isWarpPreview(){
    Common *c=Common::i();bool v=c->p_bWarpPreview.load();
    if(v){emit c->showMessage("Please exit warp preview mode before adding new markers",2000);}
    return v;
}
void SectionViewer::keyPressEvent(QKeyEvent *e){
    if(e->isAutoRepeat()||e->modifiers()!=Qt::NoModifier){return;}
    bool bUpdated=false;

    switch(e->key()){
    case Qt::Key_R:{
        if(m_bViewerInited){m_renderer->ResetCamera();bUpdated=true;}
    }break;

    case Qt::Key_Z:{g_bVolumnPicking=true;}break;
    case Qt::Key_X:{g_bPropPicking=true;}break;
    case Qt::Key_B:{emit Common::i()->buildWarpField(false);}break;

    case Qt::Key_Right:{m_imageSlider->moveSliderRelative(1);}break;
    case Qt::Key_Left:{m_imageSlider->moveSliderRelative(-1);}break;
    case Qt::Key_PageDown:{m_imageSlider->moveSliderRelative(m_groupSize);}break;
    case Qt::Key_PageUp:{m_imageSlider->moveSliderRelative(-m_groupSize);}break;

    case Qt::Key_Space:{
        if(nullptr!=m_candidateNode&&m_candidateNode->num==2&&nullptr!=m_nodes){
            if(isWarpPreview()){return;}

            m_nodesMutex.lock();m_nodes->append(m_candidateNode);m_nodesMutex.unlock();
            changeCurrentNode(m_candidateNode);m_candidateNode=nullptr;
        }
    }break;
    case Qt::Key_Delete:{
        if(nullptr!=m_currentNode&&nullptr!=m_nodes){
            //if(isWarpPreview()){return;}

            for(int i=0;i<2;i++){m_renderer->RemoveActor(m_currentNode->actors[i]);}
            m_nodesMutex.lock();m_nodes->removeOne(m_currentNode);m_nodesMutex.unlock();
            delete m_currentNode;m_currentNode=nullptr;bUpdated=true;
        }
    }break;
    case Qt::Key_Escape:{
        if(nullptr!=m_candidateNode){
            for(int i=0;i<m_candidateNode->num;i++){m_renderer->RemoveActor(m_candidateNode->actors[i]);}
            m_candidateNode->num=0;bUpdated=true;
        }
        if(nullptr!=m_currentNode){changeCurrentNode(nullptr);}
    }break;
    }

    if(bUpdated){m_imageViewer->GetRenderWindow()->Render();}
}

void SectionViewer::keyReleaseEvent(QKeyEvent *e){
    if(e->isAutoRepeat()||e->modifiers()!=Qt::NoModifier){return;}

    switch(e->key()){
    case Qt::Key_Z:{g_bVolumnPicking=false;}break;
    case Qt::Key_X:{
        g_bPropPicking=false;
        if(nullptr!=m_selectNode){
            if(m_currentNode!=m_selectNode){for(int i=0;i<2;i++){m_selectNode->actors[i]->GetProperty()->SetColor(s_colorNode);}}
            m_selectNode=nullptr;m_imageViewer->GetRenderWindow()->Render();
        }
    }break;
    }
}

void SectionViewer::updateNodesPoisition(QList<Node *> *nodes){
    if(nullptr!=m_nodes){
        QListIterator<Node*> iter(*m_nodes);
        while(iter.hasNext()){
            Node *p=iter.next();
            m_renderer->RemoveActor(p->actors[0]);m_renderer->RemoveActor(p->actors[1]);
        }
    }

    m_nodes=nodes;if(nullptr==m_nodes){return;}
    vtkSmartPointer<vtkImageActor> act=m_imageActors[0];if(nullptr==act){return;}

    double *pos=act->GetPosition(),*spacing=act->GetInput()->GetSpacing(),*origin=act->GetOrigin();
    double origin2[2]={pos[0]+origin[0],pos[1]+origin[1]},rotation=-act->GetOrientation()[2];

    QListIterator<Node*> iter(*m_nodes);
    while(iter.hasNext()){Node *p=iter.next();
        double p1[3]={p->p1[0]*spacing[0]+pos[0],p->p1[1]*spacing[1]+pos[1],0};
        double rotPoint[3];rotatePts(p1,origin2,-rotation,rotPoint);rotPoint[2]=10;
        bool bSelected=(p==m_currentNode);

        p->actors[0]=getNodeActor(rotPoint,bSelected?s_colorSelected:s_colorNode);
        m_renderer->AddActor(p->actors[0]);

        vtkSmartPointer<vtkLineSource> line=vtkSmartPointer<vtkLineSource>::New();
        line->SetPoint1(rotPoint);line->SetPoint2(p->p2);
        vtkSmartPointer<vtkActor> actor1=getActor(line->GetOutputPort(),bSelected?s_colorSelected:s_colorNode);
        p->actors[1]=actor1;m_renderer->AddActor(actor1);
    }
}

void SectionViewer::getMarkers(int groupIndex, double offsets[2],double scales[2],double rotation,QList<QLineF> &lines){
    QMutexLocker locker(&m_nodesMutex);
    QList<Node*> *nodes=m_allNodes.value(groupIndex,nullptr);if(nullptr==nodes){return;}
    vtkSmartPointer<vtkImageActor> act=m_imageActors[0];if(nullptr==act){return;}

    const double *pos=act->GetPosition(),*spacing=act->GetInput()->GetSpacing(),*origin=act->GetOrigin();
    double pos1[3]={pos[0],pos[1],pos[2]},spacing1[3]={spacing[0],spacing[1],spacing[2]};
    pos1[0]+=offsets[0];pos1[1]+=offsets[1];spacing1[0]*=scales[0];spacing1[1]*=scales[1];
    double origin2[2]={pos1[0]+origin[0],pos1[1]+origin[1]};

    QListIterator<Node*> iter(*nodes);
    while(iter.hasNext()){
        Node *p=iter.next();

        double unRotPoint[3];rotatePts(p->p2,origin2,-rotation,unRotPoint);
        double p2[2]={(unRotPoint[0]-pos1[0])/spacing1[0],(unRotPoint[1]-pos1[1])/spacing1[1]};

        QLineF line;line.setP1(QPointF(p->p1[0],p->p1[1]));line.setP2(QPointF(p2[0],p2[1]));
        lines.append(line);
    }
}
QVariantList SectionViewer::exportAllMarkers(){
    QVariantList list;QMutexLocker locker(&m_nodesMutex);

    QMapIterator<int,QList<Node*>*> iter1(m_allNodes);
    while(iter1.hasNext()){
        iter1.next();int groupIndex=iter1.key();
        QListIterator<Node*> iter2(*iter1.value());
        while(iter2.hasNext()){
            Node *p=iter2.next();
            QVariantMap v;v["image_point"]=QString::number(p->p1[0])+" "+QString::number(p->p1[1]);
            v["atlas_point"]=QString::number(p->p2[0])+" "+QString::number(p->p2[1]);
            v["image_index"]=p->imageIndex;v["group_index"]=groupIndex;list.append(v);
        }
    }
    return list;
}
inline void string2Point(const QVariant &v,double pos[3]){
    QStringList list=v.toString().split(" ");if(list.length()!=2){return;}
    pos[0]=list[0].toDouble();pos[1]=list[1].toDouble();
}
void SectionViewer::importAllMarkers(const QVariantList &list){
    QList<Node*> *lastNodes=nullptr;int lastGroupIndex=-1;
    foreach(QVariant v,list){
        QVariantMap v1=v.toMap();Node *p=new Node;p->imageIndex=v1["image_index"].toInt();
        string2Point(v1["image_point"],p->p1);string2Point(v1["atlas_point"],p->p2);

        int groupIndex=v1["group_index"].toInt();if(groupIndex<0){continue;}
        if(groupIndex==lastGroupIndex){lastNodes->append(p);continue;}

        lastNodes=m_allNodes.value(groupIndex,nullptr);lastGroupIndex=groupIndex;
        if(nullptr==lastNodes){lastNodes=new QList<Node*>();m_allNodes[groupIndex]=lastNodes;}
        lastNodes->append(p);
    }
}

void SectionViewer::removeCurrentMarkers(){
    if(QMessageBox::Yes!=QMessageBox::warning(nullptr,"Checking before removing markers",
                                              "All markers in current group will be removed.\nContinue?",
                                              QMessageBox::Yes|QMessageBox::No,QMessageBox::No)){
        return;
    }

    m_nodesMutex.lock();
    QList<Node*> nodes;if(nullptr!=m_nodes){nodes=*m_nodes;m_nodes->clear();}
    m_nodesMutex.unlock();

    m_selectNode=nullptr;
    foreach(Node *p,nodes){for(int i=0;i<2;i++){m_renderer->RemoveActor(p->actors[i]);}}
    changeCurrentNode(nullptr);qDeleteAll(nodes);
}
