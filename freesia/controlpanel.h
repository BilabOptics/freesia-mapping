#ifndef CONTROLPANEL_H
#define CONTROLPANEL_H

#include <QWidget>

QT_BEGIN_NAMESPACE
class SpinBox;
QT_END_NAMESPACE

class ControlPanel : public QWidget
{
    Q_OBJECT

    SpinBox *m_rotation3dX,*m_rotation3dY,*m_offset3dX,*m_offset3dY,*m_offset3dZ,*m_scale3dX,*m_scale3dY,*m_scale3dZ;//*m_rotation3dZ,

    int m_groupIndex;
    SpinBox *m_rotation2dZ,*m_offset2dX,*m_offset2dY,*m_scale2dX,*m_scale2dY;
public:
    explicit ControlPanel();

signals:

private slots:
    void valueChanged3d();
    void valueChanged2d();
};

#endif // CONTROLPANEL_H
