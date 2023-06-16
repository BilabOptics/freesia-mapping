#ifndef VOLUMEVIEWER_H
#define VOLUMEVIEWER_H

#include <vtkAutoInit.h>  // if not using CMake to compile, necessary to use this macro
#define vtkRenderingCore_AUTOINIT \
  3(vtkInteractionStyle, vtkRenderingFreeType, vtkRenderingOpenGL2)
#define vtkRenderingVolume_AUTOINIT 1(vtkRenderingVolumeOpenGL2)
#define vtkRenderingContext2D_AUTOINIT 1(vtkRenderingContextOpenGL2)

#include <QVTKOpenGLWidget.h>
#include <vtkImageData.h>
#include <vtkImageReslice.h>
#include <vtkPiecewiseFunction.h>
#include <vtkSmartPointer.h>
#include <vtkVolume.h>

#include <QMutex>
#include <opencv2/core.hpp>


QT_BEGIN_NAMESPACE
struct TransformParameters;
QT_END_NAMESPACE

struct OutputImageData {
  vtkImageData *imageData, *modelData, *selectModelData;
  OutputImageData()
      : imageData(nullptr), modelData(nullptr), selectModelData(nullptr) {}
};

class VolumeViewer : public QVTKOpenGLWidget {
  Q_OBJECT

  cv::Point3d m_volumeSize, m_voxelSize, m_modelVolumeSize, m_modelVoxelSize;
  TransformParameters *m_transform3d;

  vtkSmartPointer<vtkPiecewiseFunction> m_opacity;
  vtkSmartPointer<vtkRenderer> m_renderer;
  vtkSmartPointer<vtkVolume> m_volume;
  vtkSmartPointer<vtkImageData> m_modelImageData, m_selectImageData;

  vtkSmartPointer<vtkImageReslice> m_reslice, m_resliceSelect, m_resliceModel;

 public:
  explicit VolumeViewer();
  void resetViewer();

  void setBuffer(char *buffer, const cv::Point3i &volumeSize,
                 const cv::Point3d &voxelSize);
  void setBuffer2(char *buffer, const cv::Point3i &volumeSize,
                  const cv::Point3d &voxelSize);
  void setBuffer3(char *buffer, const cv::Point3i &volumeSize,
                  const cv::Point3d &voxelSize);

  void updateTranform3d(const TransformParameters &);
  void getImageByIndex(int type, int index, OutputImageData &output);
 signals:
  void updateRender(bool bReset);
  void volume2Updated();
 private slots:
};

#endif  // VOLUMEVIEWER_H
