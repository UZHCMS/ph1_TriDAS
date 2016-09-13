#include "CalibFormats/SiPixelObjects/interface/PixelCalibConfiguration.h"
#include "CalibFormats/SiPixelObjects/interface/PixelDACNames.h"
#include "PixelCalibrations/include/PixelFEDReadbackCalibration.h"
#include "PixelConfigDBInterface/include/PixelConfigInterface.h"
#include "PixelUtilities/PixelFEDDataTools/include/PixelFEDDataTypes.h"
#include "PixelUtilities/PixelFEDDataTools/include/ErrorFIFODecoder.h"
#include "PixelUtilities/PixelFEDDataTools/include/ColRowAddrDecoder.h"
#include "PixelUtilities/PixelFEDDataTools/include/DigScopeDecoder.h"
#include "PixelUtilities/PixelFEDDataTools/include/DigTransDecoder.h"
#include "PixelUtilities/PixelFEDDataTools/include/FIFO3Decoder.h"
#include "PixelUtilities/PixelRootUtilities/include/PixelRootDirectoryMaker.h"
#include "PixelUtilities/PixelFEDDataTools/include/DigFIFO1Decoder.h"


#include "TH1F.h"
#include "TH2F.h"
#include "TGraphErrors.h"
#include "TF1.h"
#include "TFile.h"
#include "TLine.h"
#include "TCanvas.h"
#include "TAxis.h"
#include "TTree.h"

#include <iomanip>
#include <algorithm>

using namespace pos;

///////////////////////////////////////////////////////////////////////////////////////////////
PixelFEDReadbackCalibration::PixelFEDReadbackCalibration(const PixelFEDSupervisorConfiguration & tempConfiguration, SOAPCommander* mySOAPCmdr)
  : PixelFEDCalibrationBase(tempConfiguration,*mySOAPCmdr), rootf(0)
{
  std::cout << "In PixelFEDReadbackCalibration copy ctor()" << std::endl;
}

///////////////////////////////////////////////////////////////////////////////////////////////
void PixelFEDReadbackCalibration::initializeFED() {
  setFEDModeAndControlRegister(0x8, 0x30010);
  //setFEDModeAndControlRegister(0x8, 0x00014);
  printIfSlinkHeaderMessedup_off();
  sendResets();
  //setFEDModeAndControlRegister(0x8, 0x10015);
  
}

///////////////////////////////////////////////////////////////////////////////////////////////
xoap::MessageReference PixelFEDReadbackCalibration::beginCalibration(xoap::MessageReference msg) {
  std::cout << "In PixelFEDReadbackCalibration::beginCalibration()" << std::endl;

  PixelCalibConfiguration* tempCalibObject = dynamic_cast<PixelCalibConfiguration*>(theCalibObject_);
  assert(tempCalibObject != 0);

  tempCalibObject->writeASCII(outputDir());

  setFIFO1Mode();//jen

  for (unsigned dacnum = 0; dacnum < tempCalibObject->numberOfScanVariables(); ++dacnum) {
    const std::string& dacname = tempCalibObject->scanName(dacnum);
    std::vector<unsigned int> dacvals = tempCalibObject->scanValues(dacname);
    if (dacvals.size() > 1)
      dacsToScan.push_back(dacname);

    for( unsigned int i = 0; i < dacvals.size(); ++i ) std::cout << " dac value " << i << " is " << dacvals[i] << std::endl;
  }

  BookEm("");

  xoap::MessageReference reply = MakeSOAPMessageReference("BeginCalibrationDone");
  return reply;
}

///////////////////////////////////////////////////////////////////////////////////////////////
xoap::MessageReference PixelFEDReadbackCalibration::execute(xoap::MessageReference msg) {
  Attribute_Vector parameters(2);
  parameters[0].name_ = "WhatToDo";
  parameters[1].name_ = "StateNum";
  Receive(msg, parameters);

  const unsigned state = atoi(parameters[1].value_.c_str());
  
  if (parameters[0].value_ == "RetrieveData")
    RetrieveData(state);
  else if (parameters[0].value_ == "Analyze")
    Analyze();
  else {
    cout << "ERROR: PixelFEDReadbackCalibration::execute() does not understand the WhatToDo command, "<< parameters[0].value_ <<", sent to it.\n";
    assert(0);
  }

  xoap::MessageReference reply = MakeSOAPMessageReference("FEDCalibrationsDone");
  return reply;
}

///////////////////////////////////////////////////////////////////////////////////////////////
xoap::MessageReference PixelFEDReadbackCalibration::endCalibration(xoap::MessageReference msg) {

  std::cout << "In PixelFEDReadbackCalibration::endCalibration()" << std::endl;
  xoap::MessageReference reply = MakeSOAPMessageReference("EndCalibrationDone");
  return reply;
}

///////////////////////////////////////////////////////////////////////////////////////////////
void PixelFEDReadbackCalibration::RetrieveData(unsigned state) {
  PixelCalibConfiguration* tempCalibObject = dynamic_cast<PixelCalibConfiguration*>(theCalibObject_);
  assert(tempCalibObject != 0);

  std::map<std::string, unsigned int> currentDACValues;
  for (unsigned dacnum = 0; dacnum < tempCalibObject->numberOfScanVariables(); ++dacnum) {
    const std::string& dacname = tempCalibObject->scanName(dacnum);
    const unsigned dacvalue = tempCalibObject->scanValue(tempCalibObject->scanName(dacnum), state);
    currentDACValues[dacname] = dacvalue;
    if( dacname == "TBMPLL" ) isTBMPLLdelayScan = true;
    if( dacname == "TBMADelay" ) isTBMPLLdelayScan = false;
  }

  if(isTBMPLLdelayScan && currentDACValues["TBMPLL"] != lastDelayValue){
   event_ = 0;
   lastDelayValue = currentDACValues["TBMPLL"];
   last_dacs.clear();
  }
  if(!isTBMPLLdelayScan && currentDACValues["TBMADelay"] != lastDelayValue){
   event_ = 0;
   lastDelayValue = currentDACValues["TBMADelay"];
   last_dacs.clear();
  }
  
  const std::vector<std::pair<unsigned, std::vector<unsigned> > >& fedsAndChannels = tempCalibObject->fedCardsAndChannels(crate_, theNameTranslation_, theFEDConfiguration_, theDetectorConfiguration_);
  
  for (unsigned ifed = 0; ifed < fedsAndChannels.size(); ++ifed) {

    const unsigned fednumber = fedsAndChannels[ifed].first;
    const unsigned long vmeBaseAddress = theFEDConfiguration_->VMEBaseAddressFromFEDNumber(fednumber);
    PixelFEDInterface* iFED = FEDInterface_[vmeBaseAddress];

    const int MaxChans = 37;
    uint32_t bufferFifo1[MaxChans][1024];
    int statusFifo1[MaxChans] = {0};
    
    for( unsigned int ch = 0; ch < fedsAndChannels[ifed].second.size(); ch++ ){
     statusFifo1[ch] = iFED->drainFifo1(fedsAndChannels[ifed].second[ch], bufferFifo1[ch], 1024);
    }

    for( unsigned int ch = 0; ch < fedsAndChannels[ifed].second.size(); ch++ ){
      
     int channel = (fedsAndChannels[ifed].second)[ch];

     const std::vector<PixelROCName>& rocs = theNameTranslation_->getROCsFromFEDChannel(fednumber, channel);
      
     if (statusFifo1[ch] > 0){ 

      for( int i = 0; i < statusFifo1[ch]; ++i ){
       uint32_t w = bufferFifo1[ch][i];
       int decCh = (w >> 26) & 0x3f;       
       if( decCh != channel ) continue;       
       uint32_t mk = (w >> 21) & 0x1f;
       uint32_t az = (w >> 8) & 0x1fff;
       uint32_t f8 = w & 0xff;
       if (az == 0 && mk != 0x1e ){
        if( mk <= rocs.size() ){
         last_dacs[fedsAndChannels[ifed].first][channel][mk].push_back(f8);
        }
       }// close reading dac from roc header
      }// close fifo1 reading  
     }// close if status > 0
     
    }// close loop on channels

  }// close loop on feds

  if( event_ == 31 ){

     ReadbackROCs();
  }

  event_++;
  sendResets();
    
}

///////////////////////////////////////////////////////////////////////////////////////////////
void PixelFEDReadbackCalibration::Analyze() { 

  PixelCalibConfiguration* tempCalibObject = dynamic_cast<PixelCalibConfiguration*>(theCalibObject_);
  assert(tempCalibObject != 0);

  for( std::map<int,std::map<int,std::map<int,std::map<int,int> > > >::iterator it = values.begin(); it!=values.end(); ++it ){//feds   
   for( std::map<int,std::map<int,std::map<int,int> > >::iterator it2 = (it->second).begin(); it2!=(it->second).end(); ++it2 ){//channels per fed   

    const std::vector<PixelROCName>& rocs = theNameTranslation_->getROCsFromFEDChannel(it->first, it2->first);

    for( std::map<int,std::map<int,int> >::iterator it3 = (it2->second).begin(); it3!=(it2->second).end(); ++it3 ){//rocs per channel
 
     for( std::map<int,int>::iterator it4 = (it3->second).begin(); it4!=(it3->second).end(); ++it4 ){

      if( it4->second == rocs[it3->first-1].roc() ){ 
       int tmp = it4->first; 
       int delay1 = -1;
       int delay2 = -1;
       if( isTBMPLLdelayScan ){
        delay1 = (tmp>>2)&0x7;
        delay2 = ((tmp>>2)&0x38)>>3;
       }
       else{
        delay1 = (tmp>>3)&0x7;
        delay2 = tmp&0x7;
       }
       scansROCs[it->first][it2->first][it3->first-1]->Fill(delay1,delay2,1);
      }

     }//tbmplldelay values

    }//rocs per channel
   }//channels per fed
  }//feds

  //fill histo with sum of channels
  for( std::map<int,std::map<int,std::vector<TH2F*> > >::iterator it1 = scansROCs.begin(); it1 != scansROCs.end(); ++it1 ){
   std::string moduleName = "";
   PixelChannel theChannel;
   for( std::map<int,std::vector<TH2F*> >::iterator it2 = it1->second.begin(); it2 != it1->second.end(); ++it2 ){  
     
    if( theNameTranslation_->FEDChannelExist(it1->first, it2->first) ){
     theChannel = theNameTranslation_->ChannelFromFEDChannel(it1->first, it2->first);
     moduleName = theChannel.modulename();
    }

    for( unsigned int h = 0; h < it2->second.size(); ++h ) ROCsHistoSum[moduleName][0]->Add(it2->second[h]);

   }//close loop on channels
  }//close loop on fed

  std::map<std::string,int> currentDelay;
  std::map<std::string,int> newDelay;
  std::map<std::string,int> nROCsForcurrentDelay;
  std::map<std::string,int> passState;

  rootf->cd();
  branch theBranch;
  TDirectory* dirSummaries = gDirectory->mkdir("SummaryTrees","SummaryTrees");
  dirSummaries->cd();

  TTree* tree = new TTree("PassState","PassState");  
  tree->Branch("PassState",&theBranch,"pass/F:moduleName/C",4096000);
  rootf->cd();

  for( std::map<std::string,std::vector<TH2F*> >::iterator it = ROCsHistoSum.begin(); it != ROCsHistoSum.end(); ++it ){

   std::string moduleName=it->first;//(module_name->modulename());

   PixelTBMSettings *TBMSettingsForThisModule=0;
   PixelConfigInterface::get(TBMSettingsForThisModule, "pixel/tbm/"+moduleName, *theGlobalKey_);
   assert(TBMSettingsForThisModule!=0);

   int delayX;
   int delayY;
   if( isTBMPLLdelayScan ){
    currentDelay[moduleName] = TBMSettingsForThisModule->getTBMPLLDelay();
    delayX = (currentDelay[moduleName]&28)>>2;
    delayY = (currentDelay[moduleName]&224)>>5;
   }
   else{
    currentDelay[moduleName] = TBMSettingsForThisModule->getTBMADelay();
    delayX = (currentDelay[moduleName]&56)>>3;
    delayY = (currentDelay[moduleName]&7);
   }

   nROCsForcurrentDelay[moduleName] = it->second[0]->GetBinContent(delayX+1,delayY+1);

   if( nROCsForcurrentDelay[moduleName] == 16 ){
    passState[moduleName] = 1;
   }
   else{//maybe try here +=1 or 2 bins from the current value?

    passState[moduleName] = 0;

    for( int bx = 1; bx < it->second[0]->GetNbinsX()+1; ++bx ){
     for( int by = 1; by < it->second[0]->GetNbinsY()+1; ++by ){ 
      if( it->second[0]->GetBinContent(bx,by) == 16 ){ delayX = bx-1; delayY = by-1; passState[moduleName] = 1; break; }
     }//close loop on by
    }//close loop on bx
    
   }

   if( isTBMPLLdelayScan ){
    delayX = (delayX<<2);
    delayY = (delayY<<5);
   }
   else{

    int scanValueMin = tempCalibObject->scanValueMin("TBMADelay");
    scanValueMin = (scanValueMin&192)>>6;
    int mask=0;
    if( scanValueMin==0 ) mask=0;
    else if( scanValueMin==1 ) mask=64;
    else if( scanValueMin==2 ) mask=128;
    else if( scanValueMin==3 ) mask=192;

    delayX = (delayX<<3)|mask;
    delayY = delayY;

   }
   newDelay[moduleName] = delayX+delayY;

   if( isTBMPLLdelayScan ) TBMSettingsForThisModule->setTBMPLLDelay(newDelay[moduleName]);
   else{
    TBMSettingsForThisModule->setTBMADelay(newDelay[moduleName]);
    TBMSettingsForThisModule->setTBMBDelay(newDelay[moduleName]);
   }
   TBMSettingsForThisModule->writeASCII(outputDir());
   //std::cout << "Wrote TBM settings for module:" << moduleName << endl;			
   delete TBMSettingsForThisModule;

   theBranch.pass = passState[moduleName];
   strcpy(theBranch.moduleName,moduleName.c_str());
   tree->Fill();

  }
  
  CloseRootf();

}

///////////////////////////////////////////////////////////////////////////////////////////////
void PixelFEDReadbackCalibration::CloseRootf() {
  if (rootf) {
    rootf->Write();
    rootf->Close();
    delete rootf;
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////
void PixelFEDReadbackCalibration::BookEm(const TString& path) {

  values.clear();
  scansROCs.clear();
  
  TString root_fn;
  root_fn.Form("%s/ReadbackScan.root", outputDir().c_str());
  cout << "writing histograms to file " << root_fn << endl;
  CloseRootf();
  rootf = new TFile(root_fn, "create");
  assert(rootf->IsOpen());
  rootf->cd(0);

  PixelCalibConfiguration* tempCalibObject = dynamic_cast<PixelCalibConfiguration*>(theCalibObject_);
  assert(tempCalibObject != 0);
  const std::vector<std::pair<unsigned, std::vector<unsigned> > >& fedsAndChannels = tempCalibObject->fedCardsAndChannels(crate_, theNameTranslation_, theFEDConfiguration_, theDetectorConfiguration_);

  PixelRootDirectoryMaker rootDirs(fedsAndChannels,gDirectory);

  for (unsigned ifed = 0; ifed < fedsAndChannels.size(); ++ifed) {
   
   std::map<int,std::vector<TH2F*> > chTBMmap2D;
   for( unsigned int ch = 0; ch < fedsAndChannels[ifed].second.size(); ch++ ){

    rootDirs.cdDirectory(fedsAndChannels[ifed].first, (fedsAndChannels[ifed].second)[ch]);

    std::vector<TH2F*> histosROCs;
    TH2F* h_nROCHeaders = 0;

    const std::vector<PixelROCName>& rocs = theNameTranslation_->getROCsFromFEDChannel(fedsAndChannels[ifed].first, (fedsAndChannels[ifed].second)[ch]);
    for( int r = 0; r < rocs.size(); ++r ){
     TString hname(rocs[r].rocname());
     h_nROCHeaders = new TH2F(hname, hname, 8, 0, 8, 8, 0, 8 );
     histosROCs.push_back(h_nROCHeaders);    
    }
    chTBMmap2D[(fedsAndChannels[ifed].second)[ch]] = histosROCs;
  
   }//close loop on channels

   scansROCs[fedsAndChannels[ifed].first] = chTBMmap2D;

  }//close loop on feds

  rootf->cd();

  std::set<PixelChannel> names = theNameTranslation_->getChannels(); 
  PixelRootDirectoryMaker rootDirsModules(names,gDirectory);
  for(std::set<PixelChannel>::iterator PixelChannel_itr=names.begin(), PixelChannel_itr_end = names.end();PixelChannel_itr!=PixelChannel_itr_end; ++PixelChannel_itr){

   rootDirsModules.cdDirectory((*PixelChannel_itr));
   std::string moduleName = (*PixelChannel_itr).modulename();
   std::vector<TH2F*> histosROC;
   TString hname(moduleName);
   TH2F* h_nROCHeaders = new TH2F(hname, hname, 8, 0, 8, 8, 0, 8 );
   //h_nROCHeaders->SetXTitle("400 MHz phase");
   //h_nROCHeaders->SetYTitle("160 MHz phase");
   histosROC.push_back(h_nROCHeaders);   
   ROCsHistoSum[moduleName] = histosROC;
    
  }

  rootf->cd(0);

}
  
///////////////////////////////////////////////////////////////////////////////////////////////
void PixelFEDReadbackCalibration::ReadbackROCs( void ) {
  
  for( std::map<int,std::map<int,std::map<int,std::vector<int> > > >::iterator it = last_dacs.begin(); it!=last_dacs.end(); ++it ){//feds
   
   for( std::map<int,std::map<int,std::vector<int> > >::iterator it2 = (it->second).begin(); it2!=(it->second).end(); ++it2 ){//channels per fed 

    for( std::map<int,std::vector<int> >::iterator it3 = (it2->second).begin(); it3!=(it2->second).end(); ++it3 ){//rocs per channel

      int start = -1;
      int stop = -1;
      int rocid = -1;
      std::vector<int> bits;
       
      for( unsigned int i=0; i<(it3->second).size(); ++i ){
       if( (((it3->second)[i])&(0x2)) == 2 && start == -1 ){start = i;}    
       else if( (((it3->second)[i])&(0x2)) == 2 && start != -1 ){stop = i;}         
      }
      
      if( start != stop && (stop-start)==16){   
        rocid=0;
        for( int i = start+1; i <= stop; ++i ){ bits.push_back((it3->second)[i]&0x1);}
        for( unsigned int b=0; b<4; ++b ) rocid+=(bits[b]<<(3-b));    
       }
         
       values[it->first][it2->first][it3->first][lastDelayValue] = rocid;            
      
    }      
   }      
  }//close loop

}

