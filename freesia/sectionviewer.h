#ifndef SECTIONVIEWER_H
#define SECTIONVIEWER_H

#include <vtkAutoInit.h> // if not using CMake to compile, necessary to use this macro
#define vtkRenderingCore_AUTOINIT 3(vtkInteractionStyle, vtkRenderingFreeType, vtkRenderingOpenGL2)
#define vtkRenderingVolume_AUTOINIT 1(vtkRenderingVolumeOpenGL2)
#define vtkRenderingContext2D_AUTOINIT 1(vtkRenderingContextOpenGL2)

#include <vtkSmartPointer.h>
#include <QVTKOpenGLWidget.h>
#include <vtkRenderer.h>
#include <vtkImageActor.h>
#include <vtkImageData.h>
#include <vtkImageMapToColors.h>
#include <vtkLookupTable.h>

#include <QMap>
#include <QLineF>
#include <QMutexLocker>
#include <QWidget>
#include <atomic>
#include <opencv2/core.hpp>

QT_BEGIN_NAMESPACE
struct Node;
class StackSlider;
QT_END_NAMESPACE

class SectionViewer : public QWidget
{
    Q_OBJECT

    const int m_type,m_modelFactor;int m_groupSize;
    std::atomic_int m_nextImageIndex,m_imageIndex;
    StackSlider *m_imageSlider;
    cv::Mat m_modelImage,m_nextModelImage;

    bool m_bViewerInited,m_bNodeVisible;
    QVTKOpenGLWidget *m_imageViewer;

    QMap<int,QList<Node*>*> m_allNodes;int m_groupIndex,m_imageIndex2,m_nextGroupIndex,m_nextImageIndex2;
    QList<Node*> *m_nodes;Node *m_currentNode,*m_candidateNode,*m_selectNode;

    vtkSmartPointer<vtkRenderer> m_renderer;
    vtkSmartPointer<vtkImageActor> m_imageActors[2],m_selectActors[2],m_modelActors[2];
    vtkSmartPointer<vtkImageMapToColors> m_colorMap2;
    vtkSmartPointer<vtkLookupTable> m_colorTable;
    QMutex m_imageActorMutex,m_nodesMutex;

    void changeCurrentNode(Node *node);
    void updateNodesPoisition(QList<Node*> *);
    void getPointPosition(int x, int y, double picked[]);
public:
    explicit SectionViewer(int type);
    void resetViewer();

    void setGroupSize(int groupSize);

    void addPoint(int x,int y);
    void selectPoint(int x,int y);void selectPoint();
    void selectRegion(int x,int y);

    void getMarkers(int groupIndex,double offsets[2],double scales[2],double rotation,QList<QLineF> &lines);
    QVariantList exportAllMarkers();void importAllMarkers(const QVariantList &list);

    bool getUpdatedIndex(int &index);bool getImageIndex(int &index);

    void updateResliceImage(const cv::Mat &image, double *origin, double *spacing, double rotation, int imageIndex, int groupIndex);
    void updateSelectImage(const cv::Mat &image,double *origin,double *spacing);
    void updateModelImage(const cv::Mat &image,double *origin,double *spacing);
signals:
    void imageUpdated();

    void imageNumberChanged(int number);
private slots:
    void setImageIndex(int index);
    void removeCurrentMarkers();
protected:
    virtual void keyPressEvent(QKeyEvent*);
    virtual void keyReleaseEvent(QKeyEvent*);
};

#endif // SECTIONVIEWER_H
