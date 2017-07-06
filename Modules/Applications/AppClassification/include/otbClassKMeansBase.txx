/*
 * Copyright (C) 2005-2017 Centre National d'Etudes Spatiales (CNES)
 *
 * This file is part of Orfeo Toolbox
 *
 *     https://www.orfeo-toolbox.org/
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef otbClassKMeansBase_txx
#define otbClassKMeansBase_txx

#include "otbClassKMeansBase.h"

namespace otb
{
namespace Wrapper
{

// todo RM ALL std::cout

void ClassKMeansBase::initKMIO()
{
/*
  AddParameter(ParameterType_InputImage, "in", "Input Image");
  SetParameterDescription("in", "Input image to classify.");
*/
  AddParameter( ParameterType_Empty, "cleanup", "Temporary files cleaning" );
  EnableParameter( "cleanup" );
  SetParameterDescription( "cleanup",
                         "If activated, the application will try to clean all temporary files it created" );
  MandatoryOff( "cleanup" );
}

void ClassKMeansBase::InitKMSampling()
{
  AddApplication("ImageEnvelope", "imgenvelop", "mean shift smoothing"); 
  AddApplication("PolygonClassStatistics", "polystats", "Polygon Class Statistics");
  AddApplication( "SampleSelection", "select", "Sample selection" );
  AddApplication( "SampleExtraction", "extraction", "Sample extraction" );

  // "vm" ShareParameter
  AddParameter(ParameterType_InputImage, "vm", "Validity Mask");
  SetParameterDescription("vm", "Validity mask. Only non-zero pixels will be used to estimate KMeans modes.");
  MandatoryOff("vm");

  AddParameter(ParameterType_Int, "ts", "Training set size");
  SetParameterDescription("ts", "Size of the training set (in pixels).");
  SetDefaultParameterInt("ts", 100);
  MandatoryOff("ts");

  AddParameter(ParameterType_Int, "nc", "Number of classes");
  SetParameterDescription("nc", "Number of modes, which will be used to generate class membership.");
  SetDefaultParameterInt("nc", 5);

  AddParameter(ParameterType_Int, "maxit", "Maximum number of iterations");
  SetParameterDescription("maxit", "Maximum number of iterations for the learning step.");
  SetDefaultParameterInt("maxit", 1000);
  MandatoryOff("maxit");

  AddParameter(ParameterType_Float, "ct", "Convergence threshold");
  SetParameterDescription("ct", "Convergence threshold for class centroid  (L2 distance, by default 0.0001).");
  SetDefaultParameterFloat("ct", 0.0001);
  MandatoryOff("ct");

  AddParameter(ParameterType_OutputFilename, "outmeans", "Centroid filename");
  SetParameterDescription("outmeans", "Output text file containing centroid positions");
  MandatoryOff("outmeans");

  ShareKMSamplingParameters();
  ConnectKMSamplingParams();
}

void ClassKMeansBase::InitKMClassification()
{
  AddApplication( "TrainVectorClassifier", "training", "Model training" );

  ShareKMClassificationParams();
  ConnectKMClassificationParams();
}

void ClassKMeansBase::ShareKMSamplingParameters()
{
  ShareParameter("in", "imgenvelop.in");
  ShareParameter("vm", "select.mask");
  ShareParameter( "ram", "polystats.ram");
}

void ClassKMeansBase::ShareKMClassificationParams()
{
  // TODO
  //ShareParameter( "classifier", "training.classifier" );
}

void ClassKMeansBase::ConnectKMSamplingParams()
{
  Connect("polystats.in", "imgenvelop.in");

  Connect("select.in", "polystats.in");
  Connect("select.vec", "polystats.vec");
  Connect( "select.ram", "polystats.ram" );

  Connect("extraction.in", "select.in");
  Connect("extraction.field", "select.field");
  Connect("extraction.vec", "select.out");
  Connect( "extraction.ram", "polystats.ram" );
}

void ClassKMeansBase::ConnectKMClassificationParams()
{
  Connect("training.cfield", "extraction.field");
  Connect("training.io.stats", "polystats.out");
  // TODO Connect Classification
}

void ClassKMeansBase::ComputeImageEnvelope(const std::string &vectorFileName)
{
  std::cout << "vectorfile = " << vectorFileName << std::endl; // todo RM
  GetInternalApplication("imgenvelop")->SetParameterString("out", vectorFileName, false);
  GetInternalApplication("imgenvelop")->ExecuteAndWriteOutput();
}

void ClassKMeansBase::ComputeAddField(const std::string &vectorFileName,
                                      const std::string &fieldName)
{
  std::cout << "add field in the layer ..." << std::endl;
  otb::ogr::DataSource::Pointer ogrDS;
  ogrDS = otb::ogr::DataSource::New(vectorFileName, otb::ogr::DataSource::Modes::Update_LayerUpdate);
  otb::ogr::Layer layer = ogrDS->GetLayer(0);

  OGRFieldDefn confidenceField(fieldName.c_str(), OFTInteger);
  confidenceField.SetWidth(confidenceField.GetWidth());
  confidenceField.SetPrecision(confidenceField.GetPrecision());
  ogr::FieldDefn confFieldDefn(confidenceField);
  layer.CreateField(confFieldDefn);

  std::cout << "Complete field ..." << std::endl;
  // Complete field
  layer.ogr().ResetReading();
  otb::ogr::Feature feature = layer.ogr().GetNextFeature();
  if(feature.addr())
  {
    std::cout << "SetField()" << std::endl;
    feature.ogr().SetField(fieldName.c_str(), 0); // ne connait pas 
    layer.SetFeature(feature);
    std::cout << "GetField " << feature.ogr().GetFieldAsInteger(fieldName.c_str()) << std::endl;
  }
  const OGRErr err = layer.ogr().CommitTransaction();
  if (err != OGRERR_NONE)
    itkExceptionMacro(<< "Unable to commit transaction for OGR layer " << layer.ogr().GetName() << ".");
  ogrDS->SyncToDisk();
/*
    // close input data source
    source->Clear();
*/
}

void ClassKMeansBase::ComputePolygonStatistics(const std::string &statisticsFileName,
                                               const std::string &fieldName)
{
  std::vector<std::string> fieldList = {fieldName};

  GetInternalApplication("polystats")->SetParameterStringList("field", fieldList, false);
  otbAppLogINFO("statsfile : " << statisticsFileName); // TODO RM
  GetInternalApplication("polystats")->SetParameterString("out", statisticsFileName, false);

  ExecuteInternal("polystats");
}

void ClassKMeansBase::SelectAndExtractSamples(std::string sampleFileName,
                                              std::string statisticsFileName,
                                              std::string fieldName,
                                              std::string sampleExtractFileName)
{
  std::cout << "Select init ..." << std::endl; 
  //GetInternalApplication( "select" )->SetParameterInputImage( "in", image );
  GetInternalApplication( "select" )->SetParameterString( "out", sampleFileName, false );

  UpdateInternalParameters( "select" );
  GetInternalApplication( "select" )->SetParameterString( "instats", statisticsFileName, false );
  GetInternalApplication( "select" )->SetParameterString( "field", fieldName, false );

  GetInternalApplication("select" )->SetParameterString("sampler", "random", false);
  GetInternalApplication( "select" )->SetParameterString("strategy", "constant", false);
  GetInternalApplication( "select" )->SetParameterInt("strategy.constant.nb", GetParameterInt("ts"), false);

  std::cout << "select.field = " << GetInternalApplication( "select" )->GetParameterString( "field" ) << std::endl;
  std::cout << "select.out = " << GetInternalApplication( "select" )->GetParameterString( "out" ) << std::endl;
  // select sample positions
  ExecuteInternal( "select" );

  UpdateInternalParameters( "extraction" );
  std::cout << "extraction.field =" << GetInternalApplication( "extraction" )->GetParameterString( "field") << std::endl;

  GetInternalApplication( "extraction" )->SetParameterString( "outfield", "prefix", false );
  GetInternalApplication( "extraction" )->SetParameterString( "outfield.prefix.name", "value_", false );

  GetInternalApplication( "extraction" )->SetParameterString( "out", sampleExtractFileName, false);
  std::cout << "extraction.out = " << sampleExtractFileName << std::endl;

  // extract sample descriptors
  GetInternalApplication( "extraction" )->ExecuteAndWriteOutput();
}

void ClassKMeansBase::TrainKMModel(FloatVectorImageType *image,
                                   std::string sampleTrainFileName)
{
  std::cout << "init train model ..." << std::endl;

  std::vector<std::string> extractOutputList = {sampleTrainFileName};
  GetInternalApplication("training")->SetParameterStringList("io.vd", extractOutputList, false);
  UpdateInternalParameters("training");

  // set field names
  std::string selectPrefix = GetInternalApplication("extraction")->GetParameterString("outfield.prefix.name");
  unsigned int nbBands = image->GetNumberOfComponentsPerPixel();
  std::vector<std::string> selectedNames;
  for( unsigned int i = 0; i < nbBands; i++ )
    {
    std::ostringstream oss;
    oss << i;
    std::cout << "feat : " << std::string(selectPrefix + oss.str()) << std::endl;
    selectedNames.push_back( selectPrefix + oss.str() );
    }

  GetInternalApplication( "training" )->SetParameterStringList("feat", selectedNames, false);
  /* TODO test sans, a enlever
  GetInternalApplication("training")->SetParameterString("classifier", "sharkkm", false);
  GetInternalApplication("training")->SetParameterInt("classifier.sharkkm.maxiter",
                                                      GetParameterInt("maxit"), false);
  GetInternalApplication("training")->SetParameterInt("classifier.sharkkm.k",
                                                      GetParameterInt("nc"), false);
  */
  ExecuteInternal( "training" );

}

}
}

#endif
