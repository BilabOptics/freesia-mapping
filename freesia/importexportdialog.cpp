#include "importexportdialog.h"
#include "common.h"

#include <QLineEdit>
#include <QProcess>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QAction>
#include <QMenu>
#include <QContextMenuEvent>
#include <QDir>
#include <QFileDialog>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QPushButton>

class PathLineEdit : public QLineEdit{
public:
    PathLineEdit();
    void setText(const QString &text);
protected:
    void contextMenuEvent(QContextMenuEvent *event) override;
};
class PathEdit : public QWidget{
    PathLineEdit *m_edit;QString m_path;QString m_fileType;
public:
    PathEdit(const QString &fileType="");
    QString getPath();void setPath(const QString &path);
};

PathLineEdit::PathLineEdit(){setFocusPolicy(Qt::NoFocus);}

void PathLineEdit::setText(const QString &text){QLineEdit::setText(text);setToolTip(text);}

void PathLineEdit::contextMenuEvent(QContextMenuEvent *event){
    QMenu *menu=new QMenu(this);QString filePath=text();bool bValid=!filePath.isEmpty();

    menu->addAction("Show in Explorer",[filePath](){
        QString cmdStr="explorer /select, "+QDir::toNativeSeparators(filePath)+"";QProcess::startDetached(cmdStr);
    })->setEnabled(bValid);

    menu->exec(event->globalPos());delete menu;
}

PathEdit::PathEdit(const QString &fileType):m_fileType(fileType){
    m_edit=new PathLineEdit;QPushButton *browseButton=new QPushButton("Browse");
    QHBoxLayout *lyt=new QHBoxLayout;lyt->setContentsMargins(0,0,0,0);setLayout(lyt);lyt->setSpacing(3);
    lyt->addWidget(m_edit,1);lyt->addWidget(browseButton);

    connect(browseButton,&QPushButton::clicked,[this](){
        QString path;
        if(m_fileType.isEmpty()){
            path=QFileDialog::getExistingDirectory(nullptr,"Select a directory",m_edit->text(),
                                                   QFileDialog::ShowDirsOnly|QFileDialog::DontResolveSymlinks);
        }else{path=QFileDialog::getOpenFileName(nullptr,"Select a file","",m_fileType);}

        if(!path.isEmpty()){m_edit->setText(path);m_path=path;}
    });
}
QString PathEdit::getPath(){return m_path;}
void PathEdit::setPath(const QString &path){m_edit->setText(path);m_path=path;}

ImportDialog::ImportDialog(){
    QWidget *voxelWidget=new QWidget;QHBoxLayout *voxelLyt=new QHBoxLayout;
    voxelWidget->setLayout(voxelLyt);voxelLyt->setContentsMargins(0,0,0,0);
    for(int i=0;i<2;i++){
        QDoubleSpinBox *p=new QDoubleSpinBox;m_voxelBoxes[i]=p;
        p->setRange(0,1000);p->setValue(1);voxelLyt->addWidget(p);
    }
    Common *c=Common::i();QStringList voxelValues=c->readConfig("import_voxels").split(" ");
    if(voxelValues.length()==2){for(int i=0;i<2;i++){m_voxelBoxes[i]->setValue(voxelValues[i].toFloat());}}

    m_groupSizeBox=new QSpinBox;m_groupSizeBox->setRange(1,1000);m_groupSizeBox->setValue(1);
    bool bOK;int groupSize=c->readConfig("group_size").toInt(&bOK);if(bOK){m_groupSizeBox->setValue(groupSize);}

    m_pathEdit=new PathEdit;//m_spotPathEdit=new PathEdit;

    QVBoxLayout *mainLyt=new QVBoxLayout;setLayout(mainLyt);mainLyt->setContentsMargins(10,10,10,10);

    QGridLayout *lyt1=new QGridLayout;int line=0;//lyt1->setSpacing(1);
    lyt1->addWidget(new QLabel("Image directory"),line,0);lyt1->addWidget(m_pathEdit,line,1);line++;
    //lyt1->addWidget(new QLabel("Spots directory"),line,0);lyt1->addWidget(m_spotPathEdit,line,1);line++;
    lyt1->addWidget(new QLabel("Voxel size"),line,0);lyt1->addWidget(voxelWidget,line,1);line++;
    lyt1->addWidget(new QLabel("Group size"),line,0);lyt1->addWidget(m_groupSizeBox,line,1);line++;
    mainLyt->addLayout(lyt1);mainLyt->addSpacing(20);

    QHBoxLayout *lyt2=new QHBoxLayout;lyt2->setSpacing(1);
    QPushButton *btnStart=new QPushButton("Import"),*btnCancel=new QPushButton("Cancel");
    lyt2->addWidget(btnStart);lyt2->addWidget(btnCancel);mainLyt->addLayout(lyt2);

    connect(btnStart,&QPushButton::clicked,this,&ImportDialog::accept);
    connect(btnCancel,&QPushButton::clicked,this,&ImportDialog::close);

    setWindowTitle("Import images from directory");
    QFile file("://assets/style.css");if(file.open(QIODevice::ReadOnly)){setStyleSheet(file.readAll());}
}
ImportDialog::~ImportDialog(){
    QStringList voxelValues;for(int i=0;i<2;i++){voxelValues.append(m_voxelBoxes[i]->text());}
    Common::i()->modifyConfig("import_voxels",voxelValues.join(" "));
    Common::i()->modifyConfig("group_size",QString::number(m_groupSizeBox->value()));
}

bool ImportDialog::getParameters(ImportParams &p){
    p.path=m_pathEdit->getPath();//p.spotsPath=m_spotPathEdit->getPath();
    for(int i=0;i<2;i++){p.voxels[i]=m_voxelBoxes[0]->value();}
    p.groupSize=m_groupSizeBox->value();return !p.path.isEmpty();
}
