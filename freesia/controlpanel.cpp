#include "controlpanel.h"

#include <QDebug>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWheelEvent>

#include "brainregionpanel.h"
#include "common.h"

class SpinBox : public QDoubleSpinBox {
 public:
  SpinBox(int type) {
    if (0 == type) {
      setRange(-180, 180);
      setSingleStep(1);
    } else if (1 == type) {
      setRange(-100000, 100000);
      setSingleStep(20);
      setDecimals(0);
    } else if (2 == type) {
      setValue(1);
      setRange(0.1, 10);
      setDecimals(3);
      setSingleStep(0.002);
    }

    setFocusPolicy(Qt::StrongFocus);
  }

 protected:
  virtual void wheelEvent(QWheelEvent *event) {
    if (!hasFocus()) {
      event->ignore();
    } else {
      QDoubleSpinBox::wheelEvent(event);
    }
  }
};

inline QWidget *createBox(const QString &title, QGridLayout *lyt1,
                          QLabel *extraLabel = nullptr) {
  QVBoxLayout *lyt = new QVBoxLayout;
  lyt->setContentsMargins(0, 0, 0, 0);
  QWidget *box = new QWidget;
  box->setLayout(lyt);
  QLabel *header = new QLabel(title);

  QHBoxLayout *headerLyt = new QHBoxLayout;
  headerLyt->setSpacing(0);
  headerLyt->addWidget(header, 1);
  header->setObjectName("panel-header");
  if (nullptr != extraLabel) {
    headerLyt->addWidget(extraLabel);
    extraLabel->setObjectName("panel-header");
  }

  lyt->addLayout(headerLyt);
  box->setObjectName("panel");
  int w = 5;
  lyt1->setContentsMargins(w, w, w, w);
  lyt->addLayout(lyt1, 1);
  return box;
}

ControlPanel::ControlPanel() : m_groupIndex(-1) {
  QVBoxLayout *mainLyt = new QVBoxLayout;
  mainLyt->setContentsMargins(0, 0, 0, 0);
  setLayout(mainLyt);
  Common *c = Common::i();

  m_rotation3dX = new SpinBox(0);
  m_rotation3dY = new SpinBox(0);  // m_rotation3dZ=new SpinBox(0);
  void (QDoubleSpinBox::*valueChanged)(double) = &SpinBox::valueChanged;
  connect(m_rotation3dX, valueChanged, this, &ControlPanel::valueChanged3d);
  connect(m_rotation3dY, valueChanged, this, &ControlPanel::valueChanged3d);

  m_offset3dX = new SpinBox(1);
  m_offset3dY = new SpinBox(1);
  m_offset3dZ = new SpinBox(1);
  m_offset3dZ->setRange(-1000000, 1000000);
  connect(m_offset3dX, valueChanged, this, &ControlPanel::valueChanged3d);
  connect(m_offset3dY, valueChanged, this, &ControlPanel::valueChanged3d);
  connect(m_offset3dZ, valueChanged, this, &ControlPanel::valueChanged3d);

  m_scale3dX = new SpinBox(2);
  m_scale3dY = new SpinBox(2);
  m_scale3dZ = new SpinBox(2);
  connect(m_scale3dX, valueChanged, this, &ControlPanel::valueChanged3d);
  connect(m_scale3dY, valueChanged, this, &ControlPanel::valueChanged3d);
  connect(m_scale3dZ, valueChanged, this, &ControlPanel::valueChanged3d);

  connect(
      c, &Common::transformParams3dLoaded, this,
      [this](TransformParameters *p) {
        m_rotation3dX->setValue(p->rotation[0]);
        m_rotation3dY->setValue(p->rotation[1]);
        m_offset3dX->setValue(p->offset[0]);
        m_offset3dY->setValue(p->offset[1]);
        m_offset3dZ->setValue(p->offset[2]);
        m_scale3dX->setValue(p->scale[0]);
        m_scale3dY->setValue(p->scale[1]);
        m_scale3dZ->setValue(p->scale[2]);
        delete p;
      },
      Qt::QueuedConnection);

  QGridLayout *paramLyt = new QGridLayout;
  paramLyt->setContentsMargins(0, 0, 0, 0);
  int line = 0;
  paramLyt->addWidget(new QLabel("Rotation"), line, 0);
  paramLyt->addWidget(m_rotation3dX, line, 1);
  paramLyt->addWidget(m_rotation3dY, line, 2);
  line++;  // paramLyt->addWidget(m_rotation3dZ,line,3);
  paramLyt->addWidget(new QLabel("Offset"), line, 0);
  paramLyt->addWidget(m_offset3dX, line, 1);
  paramLyt->addWidget(m_offset3dY, line, 2);
  paramLyt->addWidget(m_offset3dZ, line, 3);
  line++;
  paramLyt->addWidget(new QLabel("Scale"), line, 0);
  paramLyt->addWidget(m_scale3dX, line, 1);
  paramLyt->addWidget(m_scale3dY, line, 2);
  paramLyt->addWidget(m_scale3dZ, line, 3);
  line++;
  mainLyt->addWidget(createBox("Parameters (3d x,y,z)", paramLyt));

  m_rotation2dZ = new SpinBox(0);
  m_offset2dX = new SpinBox(1);
  m_offset2dY = new SpinBox(1);
  m_scale2dX = new SpinBox(2);
  m_scale2dY = new SpinBox(2);
  connect(m_rotation2dZ, valueChanged, this, &ControlPanel::valueChanged2d);
  connect(m_offset2dX, valueChanged, this, &ControlPanel::valueChanged2d);
  connect(m_offset2dY, valueChanged, this, &ControlPanel::valueChanged2d);
  connect(m_scale2dX, valueChanged, this, &ControlPanel::valueChanged2d);
  connect(m_scale2dY, valueChanged, this, &ControlPanel::valueChanged2d);

  paramLyt = new QGridLayout;
  paramLyt->setContentsMargins(0, 0, 0, 0);
  line = 0;
  paramLyt->addWidget(new QLabel("Rotation (z)"), line, 0);
  paramLyt->addWidget(m_rotation2dZ, line, 1);
  line++;
  paramLyt->addWidget(new QLabel("Offset (x,y)"), line, 0);
  paramLyt->addWidget(m_offset2dX, line, 1);
  paramLyt->addWidget(m_offset2dY, line, 2);
  line++;
  paramLyt->addWidget(new QLabel("Scale (x,y)"), line, 0);
  paramLyt->addWidget(m_scale2dX, line, 1);
  paramLyt->addWidget(m_scale2dY, line, 2);
  line++;
  QLabel *sliceIndexLabel = new QLabel;
  mainLyt->addWidget(createBox("Parameters (2d)", paramLyt, sliceIndexLabel));
  connect(
      c, &Common::sliceChanged, this,
      [sliceIndexLabel, this](TransformParameters *p) {
        m_rotation2dZ->setValue(p->rotation[2]);
        m_offset2dX->setValue(p->offset[0]);
        m_offset2dY->setValue(p->offset[1]);
        m_scale2dX->setValue(p->scale[0]);
        m_scale2dY->setValue(p->scale[1]);
        m_groupIndex = p->sliceIndex;
        sliceIndexLabel->setText("#" + QString::number(m_groupIndex));
        delete p;
      },
      Qt::QueuedConnection);

  paramLyt = new QGridLayout;
  paramLyt->setContentsMargins(0, 0, 0, 0);
  paramLyt->addWidget(new BrainRegionPanel, 0, 0);
  mainLyt->addWidget(createBox("Brain regions", paramLyt), 1);
}

void ControlPanel::valueChanged3d() {
  if (!((SpinBox *)sender())->hasFocus()) {
    return;
  }

  TransformParameters *ptr = new TransformParameters;
  ptr->rotation[0] = m_rotation3dX->value();
  ptr->rotation[1] = m_rotation3dY->value();
  ptr->rotation[2] = 0;  // m_rotation3dZ->value();
  ptr->offset[0] = m_offset3dX->value();
  ptr->offset[1] = m_offset3dY->value();
  ptr->offset[2] = m_offset3dZ->value();
  ptr->scale[0] = m_scale3dX->value();
  ptr->scale[1] = m_scale3dY->value();
  ptr->scale[2] = m_scale3dZ->value();
  delete Common::i()->p_params3d.exchange(ptr);
}

void ControlPanel::valueChanged2d() {
  if (!((SpinBox *)sender())->hasFocus() || m_groupIndex <= 0) {
    return;
  }

  TransformParameters *ptr = new TransformParameters;
  ptr->sliceIndex = m_groupIndex;
  ptr->rotation[2] = m_rotation2dZ->value();
  ptr->offset[0] = m_offset2dX->value();
  ptr->offset[1] = m_offset2dY->value();
  ptr->scale[0] = m_scale2dX->value();
  ptr->scale[1] = m_scale2dY->value();
  delete Common::i()->p_params2d.exchange(ptr);
}
