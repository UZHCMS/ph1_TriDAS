#ifndef _PixelFEDTBMDelayCalibration_h_
#define _PixelFEDTBMDelayCalibration_h_

#include "CalibFormats/SiPixelObjects/interface/PixelROCName.h"
#include "PixelCalibrations/include/PixelFEDCalibrationBase.h"
#include "PixelUtilities/PixelFEDDataTools/include/Moments.h"
#include "PixelUtilities/PixelFEDDataTools/include/PixelScanRecord.h"
#include "PixelUtilities/PixelRootUtilities/include/PixelElogMaker.h"

#include <cstdint>
#include <fstream>

class TFile;
class TH1F;
class TH2F;
class TH3F;

#include<TFile.h>
#include<TH1F.h>
#include<TH1D.h>
#include<TH2F.h>
#include<TCanvas.h>
#include<TROOT.h>
#include<TStyle.h>

class PixelFEDTBMDelayCalibration: public PixelFEDCalibrationBase {
 public:
  PixelFEDTBMDelayCalibration(const PixelFEDSupervisorConfiguration&, SOAPCommander*);

  virtual void initializeFED();
  virtual xoap::MessageReference beginCalibration(xoap::MessageReference msg);
  virtual xoap::MessageReference execute(xoap::MessageReference msg);
  virtual xoap::MessageReference endCalibration(xoap::MessageReference msg);

 private:
  bool writeElog;
  TString outtext;
  PixelElogMaker* elog;

  void RetrieveData(unsigned int state);
  void Analyze();
  void CloseRootf();
  void BookEm(const TString& path);
  void FillEm(unsigned state,int fedid, int ch, int which, float c);

  bool DumpFIFOs;
  bool PrintHits;
  bool ReadFifo1;
  std::vector<std::string> dacsToScan;
  unsigned lastTBMPLL;
  TFile* rootf;
  bool inject_;

  std::map<int,std::map<int,std::vector<TH1F*> > > ntrigsTBM;
  std::map<int,std::map<int,std::vector<TH2F*> > > scansTBM;
  std::map<std::string,std::vector<TH2F*> > TBMsHistoSum;

   struct branch{
    float pass;
    char moduleName[38];
  };

  struct branch_sum{
    int deltaTBMPLLdelayX;
    int deltaTBMPLLdelayY;
    int newTBMPLLdelayX;
    int newTBMPLLdelayY;
    double efficiency;
    char moduleName[38];
  };

};

#endif
