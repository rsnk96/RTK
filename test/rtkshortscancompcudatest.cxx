#include "rtkTest.h"
#include "rtkMacro.h"
#include "rtkCudaParkerShortScanImageFilter.h"
#include "rtkConstantImageSource.h"
#include "rtkSheppLoganPhantomFilter.h"

#include <itkStreamingImageFilter.h>
#include <itkImageRegionSplitterDirection.h>

/**
 * \file rtkshortscancompcudatest.cxx
 *
 * \brief Test rtk::CudaParkerShortScanImageFilter vs rtk::ParkerShortScanImageFilter
 *
 * This test compares weighted projections using CPU and Cuda implementations
 * of the filter for short scan handling in FBP.
 *
 * \author Simon Rit
 */

int main(int, char** )
{
  const unsigned int Dimension = 3;
  typedef float OutputPixelType;
  typedef itk::CudaImage< OutputPixelType, Dimension > OutputImageType;


  // Constant image sources
  typedef rtk::ConstantImageSource< OutputImageType > ConstantImageSourceType;
  ConstantImageSourceType::PointType origin;
  ConstantImageSourceType::SizeType size;
  ConstantImageSourceType::SpacingType spacing;

  ConstantImageSourceType::Pointer projSource  = ConstantImageSourceType::New();
  origin[0] = -127.;
  origin[1] = -3.;
  origin[2] = 0.;
  size[0] = 128;
  size[1] = 4;
  size[2] = 4;
  spacing[0] = 2.;
  spacing[1] = 2.;
  spacing[2] = 2.;

  projSource->SetOrigin( origin );
  projSource->SetSpacing( spacing );
  projSource->SetSize( size );

  // Geometry
  typedef rtk::ThreeDCircularProjectionGeometry GeometryType;
  GeometryType::Pointer geometry = GeometryType::New();
  geometry->AddProjection(600., 700., 0.  , 84., 35, 23, 15, 21, 26);
  geometry->AddProjection(500., 800., 45. , 21., 12, 16, 546, 14, 41);
  geometry->AddProjection(700., 900., 90. , 68., 68, 54, 38, 35, 56);
  geometry->AddProjection(900., 1000., 135., 48., 35, 84, 10, 84, 59);

  // Projections
  typedef rtk::SheppLoganPhantomFilter<OutputImageType, OutputImageType> SLPType;
  SLPType::Pointer slp=SLPType::New();
  slp->SetInput( projSource->GetOutput() );
  slp->SetGeometry(geometry);
  slp->SetPhantomScale(116);
  TRY_AND_EXIT_ON_ITK_EXCEPTION( slp->Update() );

  for(int inPlace=0; inPlace<2; inPlace++)
    {
    std::cout << "\n\n****** Case " << inPlace*2 << ": no streaming, ";
    if(!inPlace)
      std::cout << "not";
    std::cout << " in place ******" << std::endl;

    typedef rtk::CudaParkerShortScanImageFilter              CUDASSFType;
    CUDASSFType::Pointer cudassf = CUDASSFType::New();
    cudassf->SetInput( slp->GetOutput() );
    cudassf->SetGeometry(geometry);
    cudassf->InPlaceOff();
    TRY_AND_EXIT_ON_ITK_EXCEPTION( cudassf->Update() );

    typedef rtk::ParkerShortScanImageFilter<OutputImageType> CPUSSFType;
    CPUSSFType::Pointer cpussf = CPUSSFType::New();
    cpussf->SetInput( slp->GetOutput() );
    cpussf->SetGeometry(geometry);
    cpussf->InPlaceOff();
    TRY_AND_EXIT_ON_ITK_EXCEPTION( cpussf->Update() );

    CheckImageQuality< OutputImageType >(cudassf->GetOutput(), cpussf->GetOutput(), 1.e-5, 100, 1.);

    std::cout << "\n\n****** Case " << inPlace*2+1 << ": with streaming, ";
    if(!inPlace)
      std::cout << "not";
    std::cout << " in place ******" << std::endl;

    // Idem with streaming
    cudassf = CUDASSFType::New();
    cudassf->SetInput( slp->GetOutput() );
    cudassf->SetGeometry(geometry);
    cudassf->InPlaceOff();

    typedef itk::StreamingImageFilter<OutputImageType, OutputImageType> StreamingType;
    StreamingType::Pointer streamingCUDA = StreamingType::New();
    streamingCUDA->SetInput( cudassf->GetOutput() );
    streamingCUDA->SetNumberOfStreamDivisions(4);
    itk::ImageRegionSplitterDirection::Pointer splitter = itk::ImageRegionSplitterDirection::New();
    splitter->SetDirection(2); // Splitting along direction 1, NOT 2
    streamingCUDA->SetRegionSplitter(splitter);
    TRY_AND_EXIT_ON_ITK_EXCEPTION( streamingCUDA->Update() );

    cpussf = CPUSSFType::New();
    cpussf->SetInput( slp->GetOutput() );
    cpussf->SetGeometry(geometry);
    cpussf->InPlaceOff();

    StreamingType::Pointer streamingCPU = StreamingType::New();
    streamingCPU->SetInput( cpussf->GetOutput() );
    streamingCPU->SetNumberOfStreamDivisions(2);
    TRY_AND_EXIT_ON_ITK_EXCEPTION( streamingCPU->Update() );

    CheckImageQuality< OutputImageType >(streamingCUDA->GetOutput(), streamingCPU->GetOutput(), 1.e-5, 100, 1.);
    }

  // If all succeed
  std::cout << "\n\nTest PASSED! " << std::endl;
  return EXIT_SUCCESS;
}
