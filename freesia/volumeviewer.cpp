#include "volumeviewer.h"
#include "common.h"

#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkImageImport.h>
#include <vtkImageData.h>
#include <vtkVolumeProperty.h>
#include <vtkGPUVolumeRayCastMapper.h>
#include <vtkColorTransferFunction.h>
#include <vtkMatrix4x4.h>
#include <vtkTransform.h>
#include <vtkPlane.h>
#include <vtkMath.h>
#include <vtkCamera.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkObjectFactory.h>

#include <QDebug>

class VolumeInteractorStyle:public vtkInteractorStyleTrackballCamera{
    VolumeInteractorStyle(){}
public:
    static VolumeInteractorStyle* New();
    vtkTypeMacro(VolumeInteractorStyle, vtkInteractorStyleTrackballCamera);

    int p_type;VolumeViewer *p_viewer;
protected:
    void OnLeftButtonDown() override{
        if(1==GetInteractor()->GetRepeatCount()){emit Common::i()->toggleFullscreen(p_type);}
        else{vtkInteractorStyleTrackballCamera::OnLeftButtonDown();}
    }
    void OnMiddleButtonDown() override{
        if(1==GetInteractor()->GetRepeatCount()){p_viewer->resetViewer();}
        else{vtkInteractorStyleTrackballCamera::OnMiddleButtonDown();}
    }
};
vtkStandardNewMacro(VolumeInteractorStyle);

inline vtkSmartPointer<vtkMatrix4x4> getTransformMatrix(const TransformParameters *p,const cv::Point3f &size){
    vtkSmartPointer<vtkTransform> tranform=vtkSmartPointer<vtkTransform>::New();
    cv::Point3f c=size/2;tranform->Translate(c.x,c.y,c.z);
    tranform->RotateX(p->rotation[0]);tranform->RotateY(p->rotation[1]);//tranform->RotateZ(p->rotation[2]);
    tranform->Translate(-c.x,-c.y,-c.z);tranform->Translate(p->offset);

    vtkSmartPointer<vtkMatrix4x4> matrix=vtkSmartPointer<vtkMatrix4x4>::New();
    matrix->DeepCopy(tranform->GetMatrix());return matrix;
}

VolumeViewer::VolumeViewer():m_transform3d(new TransformParameters){//m_resliceSelect(nullptr),
    m_opacity=vtkSmartPointer<vtkPiecewiseFunction>::New();
    m_opacity->AddPoint(0,0);m_opacity->AddPoint(100,0);m_opacity->AddPoint(1000,1);m_opacity->AddPoint(65535,1);

    m_renderer=vtkSmartPointer<vtkRenderer>::New();GetRenderWindow()->AddRenderer(m_renderer);

    vtkSmartPointer<VolumeInteractorStyle> interactor=vtkSmartPointer<VolumeInteractorStyle>::New();
    GetInteractor()->SetInteractorStyle(interactor);interactor->p_type=3;interactor->p_viewer=this;

    connect(this,&VolumeViewer::updateRender,this,[this](bool bReset){
        if(bReset){
            // Copied from https://github.com/Slicer/Slicer/blob/master/Libs/MRML/Core/vtkMRMLCameraNode.cxx
            vtkCamera *cam=m_renderer->GetActiveCamera();
            double directionOfView[3];
            vtkMath::Subtract(cam->GetFocalPoint(),cam->GetPosition(),directionOfView);
            double norm=vtkMath::Norm(directionOfView),newDirectionOfView[3]={0.,0.,0.},newViewUp[3]={0.,0.,0.};

            newDirectionOfView[1]=1.;newViewUp[2]=1.;
            vtkMath::MultiplyScalar(newDirectionOfView,norm);

            double newPosition[3];
            vtkMath::Subtract(cam->GetFocalPoint(),newDirectionOfView,newPosition);
            cam->SetPosition(newPosition);cam->SetViewUp(newViewUp);

            m_renderer->ResetCamera();
        }

        GetRenderWindow()->Render();
    },Qt::QueuedConnection);

    Common *c=Common::i();
    connect(c,&Common::volumeContrastChanged,[this](int v){
        m_opacity->RemoveAllPoints();//v=std::min(65530,std::max(105,v));
        m_opacity->AddPoint(0,0);m_opacity->AddPoint(100,0);m_opacity->AddPoint(v,1);m_opacity->AddPoint(65535,1);
        GetRenderWindow()->Render();
    });
}

void VolumeViewer::resetViewer(){m_renderer->ResetCamera();GetRenderWindow()->Render();}

void VolumeViewer::setBuffer(char *buffer,const cv::Point3i &volumeSize,const cv::Point3d &voxelSize){
    vtkSmartPointer<vtkImageImport> importer=vtkSmartPointer<vtkImageImport>::New();//qDebug()<<imageSize<<imageNumber;
    importer->SetDataScalarTypeToUnsignedShort();importer->SetWholeExtent(0,volumeSize.x-1,0,volumeSize.y-1,0,volumeSize.z-1);
    importer->SetNumberOfScalarComponents(1);importer->SetDataExtentToWholeExtent();importer->SetImportVoidPointer(buffer);
    importer->Update();

    m_voxelSize=voxelSize;
    m_volumeSize=cv::Point3f(volumeSize.x*voxelSize.x,volumeSize.y*voxelSize.y,volumeSize.z*voxelSize.z);

    vtkSmartPointer<vtkImageData> imageData=vtkSmartPointer<vtkImageData>::New();imageData->DeepCopy(importer->GetOutput());
    imageData->SetSpacing(voxelSize.x,voxelSize.y,voxelSize.z);

    m_reslice=vtkSmartPointer<vtkImageReslice>::New();m_reslice->SetInputData(imageData);
    m_reslice->SetOutputDimensionality(2);m_reslice->SetInterpolationModeToLinear();m_reslice->SetAutoCropOutput(true);

    vtkSmartPointer<vtkGPUVolumeRayCastMapper> volumeMapperGpu=vtkSmartPointer<vtkGPUVolumeRayCastMapper>::New();
    volumeMapperGpu->SetInputData(imageData);volumeMapperGpu->SetSampleDistance(1.0);
    volumeMapperGpu->SetAutoAdjustSampleDistances(1);volumeMapperGpu->SetBlendMode(vtkVolumeMapper::MAXIMUM_INTENSITY_BLEND);
    volumeMapperGpu->SetUseJittering(true);

    vtkSmartPointer<vtkVolumeProperty> volumeProperty=vtkSmartPointer<vtkVolumeProperty>::New();
    volumeProperty->SetInterpolationTypeToLinear();volumeProperty->ShadeOff();

    volumeProperty->SetScalarOpacity(m_opacity);

    vtkSmartPointer<vtkColorTransferFunction> color=vtkSmartPointer<vtkColorTransferFunction>::New();
    color->AddRGBPoint(0,1,1,1);color->AddRGBPoint(65535,1,1,1);volumeProperty->SetColor(color);

    m_volume=vtkSmartPointer<vtkVolume>::New();m_volume->SetOrigin(m_volumeSize.x/2,m_volumeSize.y/2,m_volumeSize.z/2);
    m_volume->SetMapper(volumeMapperGpu);m_volume->SetProperty(volumeProperty);
    m_renderer->AddVolume(m_volume);emit updateRender(true);
}

void VolumeViewer::setBuffer2(char *buffer,const cv::Point3i &volumeSize,const cv::Point3d &voxelSize){
    if(nullptr==buffer){m_resliceSelect=nullptr;return;}

    vtkSmartPointer<vtkImageImport> importer=vtkSmartPointer<vtkImageImport>::New();//qDebug()<<imageSize<<imageNumber;
    importer->SetDataScalarTypeToUnsignedChar();importer->SetWholeExtent(0,volumeSize.x-1,0,volumeSize.y-1,0,volumeSize.z-1);
    importer->SetNumberOfScalarComponents(1);importer->SetDataExtentToWholeExtent();importer->SetImportVoidPointer(buffer);
    importer->Update();

    m_selectImageData=vtkSmartPointer<vtkImageData>::New();m_selectImageData->DeepCopy(importer->GetOutput());
    m_selectImageData->SetSpacing(voxelSize.x,voxelSize.y,voxelSize.z);

    m_resliceSelect=vtkSmartPointer<vtkImageReslice>::New();m_resliceSelect->SetInputData(m_selectImageData);
    m_resliceSelect->SetOutputDimensionality(2);m_resliceSelect->SetInterpolationModeToNearestNeighbor();m_resliceSelect->SetAutoCropOutput(true);
}

void VolumeViewer::setBuffer3(char *buffer,const cv::Point3i &volumeSize,const cv::Point3d &voxelSize){
    vtkSmartPointer<vtkImageImport> importer=vtkSmartPointer<vtkImageImport>::New();//qDebug()<<imageSize<<imageNumber;
    importer->SetDataScalarTypeToUnsignedShort();importer->SetWholeExtent(0,volumeSize.x-1,0,volumeSize.y-1,0,volumeSize.z-1);
    importer->SetNumberOfScalarComponents(1);importer->SetDataExtentToWholeExtent();importer->SetImportVoidPointer(buffer);
    importer->Update();

    m_modelImageData=vtkSmartPointer<vtkImageData>::New();m_modelImageData->DeepCopy(importer->GetOutput());
    m_modelVoxelSize=voxelSize;m_modelVolumeSize=volumeSize;m_modelImageData->SetSpacing(voxelSize.x,voxelSize.y,voxelSize.z);

    m_resliceModel=vtkSmartPointer<vtkImageReslice>::New();m_resliceModel->SetInputData(m_modelImageData);
    m_resliceModel->SetOutputDimensionality(2);m_resliceModel->SetInterpolationModeToNearestNeighbor();m_resliceModel->SetAutoCropOutput(true);
}

// Copied from https://github.com/Slicer/Slicer/blob/master/Libs/MRML/Core/vtkMRMLSliceNode.cxx
inline void InitializeAxialMatrix(vtkMatrix4x4* orientationMatrix)
{
    if (!orientationMatrix)
    {
        return;
    }
    orientationMatrix->SetElement(0,0,-1.0);
    orientationMatrix->SetElement(1,0,0.0);
    orientationMatrix->SetElement(2,0,0.0);
    orientationMatrix->SetElement(0,1,0.0);
    orientationMatrix->SetElement(1,1,1.0);
    orientationMatrix->SetElement(2,1,0.0);
    orientationMatrix->SetElement(0,2,0.0);
    orientationMatrix->SetElement(1,2,0.0);
    orientationMatrix->SetElement(2,2,1.0);
}
//----------------------------------------------------------------------------
inline void InitializeSagittalMatrix(vtkMatrix4x4* orientationMatrix)
{
    if (!orientationMatrix)
    {
        return;
    }
    orientationMatrix->SetElement(0,0,0.0);
    orientationMatrix->SetElement(1,0,-1.0);
    orientationMatrix->SetElement(2,0,0.0);
    orientationMatrix->SetElement(0,1,0.0);
    orientationMatrix->SetElement(1,1,0.0);
    orientationMatrix->SetElement(2,1,1.0);
    orientationMatrix->SetElement(0,2,1.0);
    orientationMatrix->SetElement(1,2,0.0);
    orientationMatrix->SetElement(2,2,0.0);
}
//----------------------------------------------------------------------------
inline void InitializeCoronalMatrix(vtkMatrix4x4* orientationMatrix)
{
    if (!orientationMatrix)
    {
        return;
    }
    orientationMatrix->SetElement(0,0,-1.0);
    orientationMatrix->SetElement(1,0,0.0);
    orientationMatrix->SetElement(2,0,0.0);
    orientationMatrix->SetElement(0,1,0.0);
    orientationMatrix->SetElement(1,1,0.0);
    orientationMatrix->SetElement(2,1,1.0);
    orientationMatrix->SetElement(0,2,0.0);
    orientationMatrix->SetElement(1,2,1.0);
    orientationMatrix->SetElement(2,2,0.0);
}

void VolumeViewer::getImageByIndex(int type,int index,OutputImageData &output){
    vtkSmartPointer<vtkMatrix4x4> resliceAxes1=vtkSmartPointer<vtkMatrix4x4>::New();

    resliceAxes1->Identity();
    if(0==type){
        InitializeSagittalMatrix(resliceAxes1);
    }else if(1==type){
        InitializeCoronalMatrix(resliceAxes1);
    }else{
        InitializeAxialMatrix(resliceAxes1);
    }

    vtkSmartPointer<vtkMatrix4x4> resliceAxes=vtkSmartPointer<vtkMatrix4x4>::New();
    resliceAxes->Multiply4x4(getTransformMatrix(m_transform3d,m_volumeSize),resliceAxes1,resliceAxes);//tranform->GetMatrix()

    double modelSpacing[]={m_modelVoxelSize.x*m_transform3d->scale[0],m_modelVoxelSize.y*m_transform3d->scale[1],m_modelVoxelSize.z*m_transform3d->scale[2]};
    double originOffset[]={-(m_transform3d->scale[0]-1)*m_volumeSize.x/2,-(m_transform3d->scale[1]-1)*m_volumeSize.y/2,-(m_transform3d->scale[2]-1)*m_volumeSize.z/2};
    m_modelImageData->SetSpacing(modelSpacing);m_modelImageData->SetOrigin(originOffset);

    double *pVoxel=(double*)(&m_voxelSize);
    double position[4]={0,0,index*pVoxel[type],1},*position1=resliceAxes->MultiplyDoublePoint(position);
    for(int i=0;i<3;i++){resliceAxes->SetElement(i,3,position1[i]);}

    m_resliceModel->SetResliceAxes(resliceAxes);m_resliceModel->Update();
    output.modelData=m_resliceModel->GetOutput();

    if(nullptr!=m_resliceSelect){
        m_selectImageData->SetSpacing(modelSpacing);m_selectImageData->SetOrigin(originOffset);
        m_resliceSelect->SetResliceAxes(resliceAxes);m_resliceSelect->Update();
        output.selectModelData=m_resliceSelect->GetOutput();
    }

    double *position2=resliceAxes1->MultiplyDoublePoint(position);
    for(int i=0;i<3;i++){resliceAxes1->SetElement(i,3,position2[i]);}

    m_reslice->SetResliceAxes(resliceAxes1);m_reslice->Update();
    output.imageData=m_reslice->GetOutput();
}

void VolumeViewer::updateTranform3d(const TransformParameters &p){m_transform3d->copy(p);}//emit transformed3d(p);
