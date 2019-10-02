#include "common.h"

#include <QFile>
#include <QApplication>
#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariant>
#include <QVariantMap>

Common::Common():p_selectionMode(1),p_params3d(nullptr),p_params2d(nullptr),p_bWarpPreview(false),
    p_softwareName(QFileInfo(QApplication::applicationFilePath()).completeBaseName()){

    m_configFolderPath=QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)+"/VISoR/freesia/";
    QDir().mkpath(m_configFolderPath);m_configFilePath=m_configFolderPath+"config.json";
    loadJson(m_configFilePath,m_configParams);
}

bool Common::loadJson(const QString &filePath,QVariantMap &params){
    QFile file(filePath);if(!file.exists()||!file.open(QIODevice::ReadOnly)){return false;}
    QByteArray saveData = file.readAll();file.close();
    QJsonDocument loadDoc(QJsonDocument::fromJson(saveData));
    params=loadDoc.object().toVariantMap();return !params.empty();
}
bool Common::saveJson(const QString &filePath,const QVariantMap& params){
    QFile file(filePath);if(!file.open(QIODevice::WriteOnly)){return false;}
    QJsonDocument saveDoc(QJsonObject::fromVariantMap(params));
    file.write(saveDoc.toJson());file.close();return true;
}

QString Common::readConfig(const QString &key){
    return m_configParams.contains(key)?m_configParams[key].toString():"";
}
bool Common::modifyConfig(const QString &key, const QString &value){
    m_configParams[key]=value;return saveJson(m_configFilePath,m_configParams);
}
