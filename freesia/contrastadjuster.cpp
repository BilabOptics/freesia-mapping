#include "contrastadjuster.h"

#include <QDebug>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>


static const int MaxPixelValue = 65535;
static const int MiddleValue = 1000;
static const float MiddleRangeFactor = 0.8;

ContrastAdjuster::ContrastAdjuster(QWidget *parent)
    : QWidget(parent), m_maxValue(MaxPixelValue), m_middleValue(MiddleValue) {
  initUI();
}
ContrastAdjuster::ContrastAdjuster(int maxValue, int middleValue)
    : m_maxValue(maxValue), m_middleValue(middleValue) {
  initUI();
}

void ContrastAdjuster::initUI() {
  QHBoxLayout *toolLayout = new QHBoxLayout;
  setLayout(toolLayout);
  toolLayout->setContentsMargins(0, 0, 0, 0);
  toolLayout->setSpacing(1);

  m_spinbox = new QSpinBox;
  m_spinbox->setMaximumWidth(70);
  m_spinbox->setRange(0, m_maxValue);

  m_slider = new QSlider(Qt::Horizontal);
  m_slider->setMaximumHeight(20);
  m_slider->setRange(0, m_maxValue);

  toolLayout->addWidget(m_slider);
  toolLayout->addWidget(m_spinbox);

  connect(m_slider, SIGNAL(valueChanged(int)), this, SLOT(updateSlider(int)));
  connect(m_spinbox, SIGNAL(valueChanged(int)), this, SLOT(updateSpinbox(int)));

  m_spinbox->setValue(m_middleValue);  // m_spinbox->maximum()
}

void ContrastAdjuster::updateThreshold(int v) { emit updateViewerThreshold(v); }

void ContrastAdjuster::updateSlider(int v) {
  double maxX = m_slider->maximum(), middleV = MiddleRangeFactor * maxX;
  if (v < middleV) {
    v = v / middleV * m_middleValue;
  } else {
    v = (v - middleV) / double(maxX - middleV) * (m_maxValue - m_middleValue) +
        m_middleValue;
  }

  if (v < m_spinbox->minimum()) {
    v = m_spinbox->minimum();
  }
  if (v > m_spinbox->maximum()) {
    v = m_spinbox->maximum();
  }

  m_spinbox->blockSignals(true);
  m_spinbox->setValue(v);
  m_spinbox->blockSignals(false);

  updateThreshold(v);
}
void ContrastAdjuster::updateSpinbox(int v) {
  updateThreshold(v);

  double maxX = m_slider->maximum(), middleV = MiddleRangeFactor * maxX;
  if (v > m_maxValue) {
    v = double(v - m_middleValue) / (m_maxValue - m_middleValue) *
            (maxX - middleV) +
        middleV;
  } else {
    v = double(v) / m_middleValue * middleV;
  }

  if (v < m_slider->minimum()) {
    v = m_slider->minimum();
  }
  if (v > m_slider->maximum()) {
    v = m_slider->maximum();
  }

  m_slider->blockSignals(true);
  m_slider->setValue(v);
  m_slider->blockSignals(false);
}

void ContrastAdjuster::viewerCreated() { updateSpinbox(m_spinbox->value()); }

void ContrastAdjuster::setValue(int v) { m_spinbox->setValue(v); }
int ContrastAdjuster::value() { return m_spinbox->value(); }
