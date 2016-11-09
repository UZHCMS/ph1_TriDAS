#ifndef _PixelFEDIanaCalibration_h_
#define _PixelFEDIanaCalibration_h_

#include "CalibFormats/SiPixelObjects/interface/PixelROCName.h"
#include "PixelCalibrations/include/PixelFEDCalibrationBase.h"
#include "PixelUtilities/PixelFEDDataTools/include/Moments.h"
#include "PixelUtilities/PixelFEDDataTools/include/PixelScanRecord.h"
#include "CalibFormats/SiPixelObjects/interface/PixelCalibConfiguration.h"

#include <cstdint>
#include <fstream>

#include<TFile.h>
#include<TTree.h>
#include<TH1F.h>
#include<TH1D.h>
#include<TH2F.h>
#include<TLegend.h>
#include<TCanvas.h>
#include<TROOT.h>
#include<TStyle.h>

//class TFile;
//class TH1F;
//class TH2F;
//class TH3F;

class PixelFEDIanaCalibration: public PixelFEDCalibrationBase {
 public:
  PixelFEDIanaCalibration(const PixelFEDSupervisorConfiguration&, SOAPCommander*);

  virtual void initializeFED();
  virtual xoap::MessageReference beginCalibration(xoap::MessageReference msg);
  virtual xoap::MessageReference execute(xoap::MessageReference msg);
  virtual xoap::MessageReference endCalibration(xoap::MessageReference msg);

 private:
  TString outtext;
  bool writeElog;

  void RetrieveData(unsigned int state);
  void Analyze();
  void IanaAnalysis(pos::PixelCalibConfiguration* tmpCalib);
  void VdigAnalysis(pos::PixelCalibConfiguration* tmpCalib);
  void CloseRootf();
  void BookEm(const TString& path);
  void ReadIana(void);
  void ReadVdig(void);

   struct branch{
    float pass;
    char rocName[38];
  };

  struct branch_sum{
    float deltaVana;
    float newVana;
    float newIana;
    //float maxIana;
    float fitChisquare;
    char rocName[38];
  };

  struct branch_sum_vdig{
    float Mean;
    float RMS;
    char rocName[38];
  };
   
  std::map<pos::PixelModuleName,pos::PixelDACSettings*> dacsettings_;  
  TFile* rootf;
  unsigned int lastDACValue;
  std::vector<std::string> dacsToScan;  
  std::map<int,std::map<int,std::map<int,std::map<int,int> > > > values;
  std::map<int,std::map<int,std::map<int,std::vector<int> > > > last_dacs;
  std::map<int,std::map<int,std::map<int,std::map<int,int> > > > rocids;
  std::map<int,std::map<int,std::map<int,std::map<int,int> > > > rbreg;

};

#endif
