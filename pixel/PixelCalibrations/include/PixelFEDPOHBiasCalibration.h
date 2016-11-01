#ifndef _PixelFEDPOHBiasCalibration_h_
#define _PixelFEDPOHBiasCalibration_h_

#include "toolbox/exception/Handler.h"
#include "toolbox/Event.h"

#include "PixelUtilities/PixelFEDDataTools/include/PixelMode.h"
#include "PixelCalibrations/include/PixelCalibrationBase.h"

#include "CalibFormats/SiPixelObjects/interface/PixelROCName.h"
#include "PixelCalibrations/include/PixelFEDCalibrationBase.h"
#include "PixelUtilities/PixelFEDDataTools/include/Moments.h"
#include "PixelUtilities/PixelFEDDataTools/include/PixelScanRecord.h"

#include "PixelUtilities/PixelFEDDataTools/include/PixelScanRecord.h"

// This is for deriving common settings ! 
//#include "PixelSupervisorConfiguration/include/PixelTKFECSupervisorConfiguration.h"
//#include PixelSupervisorConfiguration/include/PixelSupervisorConfiguration.h

#include <cstdint>
#include <fstream>

//class TFile;
//class TTree;
//class TH1F;
//class TH2F;
//class TH3F;

class PixelFEDPOHBiasCalibration: public PixelFEDCalibrationBase {
 public:
  PixelFEDPOHBiasCalibration(const PixelFEDSupervisorConfiguration&, SOAPCommander*);

  virtual void initializeFED();
  virtual xoap::MessageReference beginCalibration(xoap::MessageReference msg);
  virtual xoap::MessageReference execute(xoap::MessageReference msg);
  virtual xoap::MessageReference endCalibration(xoap::MessageReference msg);

 private:
  xoap::MessageReference RetrieveData(unsigned int AOHBias, unsigned AOHGain, unsigned int fednumber, unsigned int channel);
  //  void RetrieveData(unsigned int AOHBias, unsigned int AOHGain, unsigned int fednumber, unsigned int channel);
  //  void Analyze();
  //  void CloseRootf();
  //  void BookEm(const TString& path);
  //  void FillEm(unsigned AOHBias, unsigned AOHGain, int fedid, int ch, int which);

  bool DumpFIFOs;
  //  std::vector<std::string> dacsToScan;
  //  std::map<int,std::map<int, std::map<int, TH1F*> > > effmap;
  //  TH2F *summarymap;

//  TFile* rootf;
//  TTree* tree;
//  
//  Int_t b_channel;
//  Int_t b_fednumber;
//  Int_t b_AOHGain;
//  Int_t b_isPass;
//  Int_t b_isStable;
//  Int_t b_isFirstPlateau;


  // To write ASCII files
  //  pos::PixelPortcardMap *thePortCardMap_;
  //  std::map<std::string, std::map<unsigned, unsigned>> bias_values_by_portcard_and_aoh_new;

};

#endif
