#ifndef VIEWERPANEL_H
#define VIEWERPANEL_H

#include <QWidget>
#include <QMap>

#include <thread>
#include <atomic>

#include <opencv2/core.hpp>

QT_BEGIN_NAMESPACE
class SectionViewer;
class VolumeViewer;
struct TransformParameters;
struct ImportParams;
struct Group;
QT_END_NAMESPACE

struct Image{
    int index,groupIndex;QString filePath,spotsPath;QSize size;
    cv::Mat data;QMap<int64_t,cv::Point3d> spots;bool bSpotsLoaded;
    Image():bSpotsLoaded(true){}
};

class ViewerPanel : public QWidget
{
    Q_OBJECT

    std::atomic_int m_regionNextColor,m_nextWarpType;
    std::atomic<QString*> m_projectPathPtr,m_projectPath2Save,m_exportPath,m_mergePath,m_spotsPath2Import;

    TransformParameters *m_params3d,*m_params3dSaved;
    QMap<int,Group*> m_groups;Group *m_currentGroup;

    std::atomic<ImportParams*> m_importParams;
    QString m_imageFolderPath,m_projectPath;
    std::atomic_bool m_bProjectPathValid,m_bShowSpots;
    VolumeViewer *m_volumeViewer;SectionViewer *m_sectionViewers[3];
    bool m_bViewerFullscreen;

    int m_groupSize;float m_pixelSize,m_thickness;
    QList<Image*> m_images;QSize m_totalImageSize;

    bool m_bRunning,m_bEdited,m_bImageUpdated;std::thread *m_dataThread;

    void mainLoop();

    void setGroupSize(int);

    bool loadProject();
    bool importDirectory();void importSpots();
    void exportCellCounting();void mergeCellCounting();
    void loadImages();void saveProject();

    bool selectRegion();
    bool updateImages();void transform3d(bool bForced=false);void transform2d();
    void buildWarpField();void buildWarpField(int sliceIndex,cv::Mat &warpField,bool bInversed);
public:
    explicit ViewerPanel();
    ~ViewerPanel();

signals:

private slots:
    void onImportDirectory();void onImportSpots();
    void onExportCellCounting();void onMergeCellCounting();
    void onLoadProject();
    void onSaveProject(bool bSaveAs);
};

#endif // VIEWERPANEL_H
