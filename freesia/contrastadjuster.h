#ifndef CONTRASTADJUSTER_H
#define CONTRASTADJUSTER_H

#include <QWidget>

QT_BEGIN_NAMESPACE
class QSpinBox;
class QSlider;
QT_END_NAMESPACE

class ContrastAdjuster : public QWidget {
  Q_OBJECT

  int m_maxValue, m_middleValue;
  QSpinBox *m_spinbox;
  QSlider *m_slider;

  void initUI();

 public:
  ContrastAdjuster(QWidget *parent = 0);
  ContrastAdjuster(int maxValue, int middleValue);

  int value();
  void setValue(int v);
 signals:
  void valueChanged(int);
  void updateViewerThreshold(int);
 private slots:
  void updateThreshold(int);
  void updateSlider(int);
  void updateSpinbox(int);
 public slots:
  void viewerCreated();
};

#endif  // CONTRASTADJUSTER_H
