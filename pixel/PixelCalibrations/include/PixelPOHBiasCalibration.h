#ifndef _PixelPOHBiasCalibration_h_
#define _PixelPOHBiasCalibration_h_

#include "PixelCalibrations/include/PixelCalibrationBase.h"
#include "PixelUtilities/PixelRootUtilities/include/PixelRootDirectoryMaker.h"
#include "toolbox/exception/Handler.h"
#include "toolbox/Event.h"

#include "CalibFormats/SiPixelObjects/interface/PixelChannel.h"
#include "PixelUtilities/PixelFEDDataTools/include/Moments.h"

//class TFile;
//class TTree;
//class TH1F;
//class TH2F;
//class TString;
//class TInt;

#include <cstdint>
#include <fstream>

#include<TFile.h>
#include<TTree.h>
#include<TH1F.h>
#include<TH1D.h>
#include<TH2F.h>
#include<TCanvas.h>

class PixelPOHBiasCalibration : public PixelCalibrationBase {
 public:
  PixelPOHBiasCalibration(const PixelSupervisorConfiguration&, SOAPCommander*);

  void beginCalibration();
  virtual bool execute();
  void endCalibration();
  virtual std::vector<std::string> calibrated();


 private:

  bool writeElog;

  void Analyze();
  void CloseRootf();
  //  void CloseRoots();
  void BookEm();
  
  TFile* rootf;
  //  TFile* roots;
  TTree* tree;
  //  TTree* summarytree;
  
  std::map<int, TH2F*> summary;


  //  std::map<int,std::map<int, std::map<int, TH1F*> > > effmap;
  //    std::map<int, std::map<int, TH1F*> > effmap;

  int b_channel;
  int b_fednumber;
  int b_AOHGain;
  int b_isStable;
  int b_isFastPlateau;
  int b_plateauX;
  float b_plateauEff;
  int b_isPass;

//  int s_channel;
//  int s_fednumber;
//  int s_isPass;
//  int s_isStable;
//  int s_isFastPlateau;
//  int s_plateauX;

  unsigned int AOHBiasMin;
  unsigned int AOHBiasMax;
  unsigned int AOHBiasStepSize;
  unsigned int AOHGainMin;
  unsigned int AOHGainMax;
  unsigned int AOHGainStepSize;
  unsigned int nTriggersPerPOHBias;
  float AllowPlateau;
  
  TString root_fn;
  TString outtext;
  //  TString root_fs;
    
//  TString cname;
//  TString cname_start;
//  TString cname_end;

  PixelRootDirectoryMaker* rootDirs;
  std::vector<std::string> vectorOfPortcards;
  std::vector<std::string> portcard_;
  std::vector<int> aohnumber_;
  std::vector<int> channel_;
  std::vector<int> fednumber_;
  
  std::map<std::string, std::map<unsigned, unsigned>> bias_values_by_portcard_and_aoh_new;
  std::map<int, std::map<int, std::map<int, TH1F*>>> histos;

};

#endif
