#ifndef COMMON_H
#define COMMON_H

#include <QObject>
#include <atomic>
#include <QVariantMap>

struct TransformParameters{
    double rotation[3],offset[3],scale[3];int sliceIndex;
    TransformParameters(){for(int i=0;i<3;i++){rotation[i]=0;offset[i]=0;scale[i]=1;}}
    TransformParameters(const TransformParameters &p){copy(p);}
    void copy(const TransformParameters &p){
        for(int i=0;i<3;i++){rotation[i]=p.rotation[i];offset[i]=p.offset[i];scale[i]=p.scale[i];}
        sliceIndex=p.sliceIndex;
    }
};

class Common : public QObject
{
    Q_OBJECT

    QString m_configFolderPath,m_configFilePath;
    QVariantMap m_configParams;

    explicit Common();
public:
    static Common *i(){static Common c;return &c;}

    const QString p_softwareName;std::atomic_int p_selectionMode;
    std::atomic<TransformParameters*> p_params3d,p_params2d;
    std::atomic_bool p_bWarpPreview;

    QString readConfig(const QString &key);
    bool modifyConfig(const QString &key,const QString &value);

    static bool loadJson(const QString &filePath,QVariantMap &params);
    static bool saveJson(const QString &filePath,const QVariantMap& params);
signals:
    void showMessage(const QString & message, int timeout = 0);
    void switchViewerTracking(bool);void toggleFullscreen(int);

    void setEditedState(bool);void setProjectFileName(QString);void setProjectStatus(QString);

    void regionSelected(int);void regionSelected2(int);

    void transformParams3dLoaded(TransformParameters*);
    void sliceChanged(TransformParameters*);
    void sliceContrastChanged(int);void volumeContrastChanged(int);

    void buildWarpField(bool bAll);void removeCurrentMarkers();
    void toggleWarpPreview();void toggleShowSpots();

    void importDirectory();void importSpotsDirectory();
    void exportCellCounting();void exportPixelCounting();void mergeCellCounting();
    void loadProject();void saveProject(bool bSaveAs);
public slots:
};

#endif // COMMON_H
