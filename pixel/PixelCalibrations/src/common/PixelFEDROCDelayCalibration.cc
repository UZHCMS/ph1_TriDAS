#include "CalibFormats/SiPixelObjects/interface/PixelCalibConfiguration.h"
#include "CalibFormats/SiPixelObjects/interface/PixelDACNames.h"
#include "PixelCalibrations/include/PixelFEDROCDelayCalibration.h"
#include "PixelConfigDBInterface/include/PixelConfigInterface.h"
#include "PixelUtilities/PixelFEDDataTools/include/PixelFEDDataTypes.h"
#include "PixelUtilities/PixelFEDDataTools/include/ErrorFIFODecoder.h"
#include "PixelUtilities/PixelFEDDataTools/include/ColRowAddrDecoder.h"
#include "PixelUtilities/PixelFEDDataTools/include/DigScopeDecoder.h"
#include "PixelUtilities/PixelFEDDataTools/include/DigTransDecoder.h"
#include "PixelUtilities/PixelFEDDataTools/include/FIFO3Decoder.h"
#include "PixelUtilities/PixelRootUtilities/include/PixelRootDirectoryMaker.h"
#include "PixelUtilities/PixelFEDDataTools/include/DigFIFO1Decoder.h"
#include "TTree.h"
#include "TFile.h"
#include "TH1F.h"
#include "TH2F.h"
#include "TH3F.h"
#include <iomanip>
#include <algorithm>

using namespace pos;

///////////////////////////////////////////////////////////////////////////////////////////////
PixelFEDROCDelayCalibration::PixelFEDROCDelayCalibration(const PixelFEDSupervisorConfiguration & tempConfiguration, SOAPCommander* mySOAPCmdr)
  : PixelFEDCalibrationBase(tempConfiguration,*mySOAPCmdr), rootf(0)
{
  std::cout << "In PixelFEDROCDelayCalibration copy ctor()" << std::endl;
}

///////////////////////////////////////////////////////////////////////////////////////////////
void PixelFEDROCDelayCalibration::initializeFED() {
  setFEDModeAndControlRegister(0x8, 0x30010);
  //setFEDModeAndControlRegister(0x8, 0x00014);
  printIfSlinkHeaderMessedup_off();
  sendResets();
  //setFEDModeAndControlRegister(0x8, 0x10015);
  
}

///////////////////////////////////////////////////////////////////////////////////////////////
xoap::MessageReference PixelFEDROCDelayCalibration::beginCalibration(xoap::MessageReference msg) {
  std::cout << "In PixelFEDROCDelayCalibration::beginCalibration()" << std::endl;

  PixelCalibConfiguration* tempCalibObject = dynamic_cast<PixelCalibConfiguration*>(theCalibObject_);
  assert(tempCalibObject != 0);

  tempCalibObject->writeASCII(outputDir());

  DumpFIFOs = tempCalibObject->parameterValue("DumpFIFOs") == "yes";
  ReadFifo1 = tempCalibObject->parameterValue("ReadFifo1") == "yes";
  writeElog = tempCalibObject->parameterValue("writeElog") == "yes";

  if( ReadFifo1 ) setFIFO1Mode();//jen

  inject_ = false;
  const std::vector<std::vector<unsigned int> > cols = tempCalibObject->columnList();
  const std::vector<std::vector<unsigned int> > rows = tempCalibObject->rowList();
  if( cols[0].size() != 0 && rows[0].size() != 0 ) inject_ = true;

  for (unsigned dacnum = 0; dacnum < tempCalibObject->numberOfScanVariables(); ++dacnum) {
    const std::string& dacname = tempCalibObject->scanName(dacnum);
    std::vector<unsigned int> dacvals = tempCalibObject->scanValues(dacname);
    if (dacvals.size() > 1)
      dacsToScan.push_back(dacname);

    for( unsigned int i = 0; i < dacvals.size(); ++i ) std::cout << " dac value " << i << " is " << dacvals[i] << std::endl;
  }

  if (dacsToScan.empty() && tempCalibObject->parameterValue("NoScanOK") != "yes") {
    cout << "no dacs in scan?" << endl;
    assert(0);
  }

  if (dacsToScan.size() < 3)
    BookEm("");

  outtext.Form("%s/log.txt", outputDir().c_str());
  elog = new PixelElogMaker("ROC Delay Scan");

  xoap::MessageReference reply = MakeSOAPMessageReference("BeginCalibrationDone");
  return reply;
}

///////////////////////////////////////////////////////////////////////////////////////////////
xoap::MessageReference PixelFEDROCDelayCalibration::execute(xoap::MessageReference msg) {
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
    cout << "ERROR: PixelFEDROCDelayCalibration::execute() does not understand the WhatToDo command, "<< parameters[0].value_ <<", sent to it.\n";
    assert(0);
  }

  xoap::MessageReference reply = MakeSOAPMessageReference("FEDCalibrationsDone");
  return reply;
}

xoap::MessageReference PixelFEDROCDelayCalibration::endCalibration(xoap::MessageReference msg) {

  std::cout << "In PixelFEDROCDelayCalibration::endCalibration()" << std::endl;
  xoap::MessageReference reply = MakeSOAPMessageReference("EndCalibrationDone");
  return reply;
}

///////////////////////////////////////////////////////////////////////////////////////////////
void PixelFEDROCDelayCalibration::RetrieveData(unsigned state) {
  PixelCalibConfiguration* tempCalibObject = dynamic_cast<PixelCalibConfiguration*>(theCalibObject_);
  assert(tempCalibObject != 0);

  /*const std::vector<PixelROCName>& rocs = tempCalibObject->rocList();
  typedef std::set< std::pair<unsigned int, unsigned int> > colrow_t;
  const colrow_t colrows = tempCalibObject->pixelsWithHits(state);
  if (PrintHits) {
    std::cout << "ZZ ";
    for (colrow_t::const_iterator cr = colrows.begin(); cr != colrows.end(); ++cr)
      std::cout << "c " << cr->first << " r " << cr->second << " ";
    std::cout << std::endl;
  }*/

  const std::vector<std::pair<unsigned, std::vector<unsigned> > >& fedsAndChannels = tempCalibObject->fedCardsAndChannels(crate_, theNameTranslation_, theFEDConfiguration_, theDetectorConfiguration_);

  if (DumpFIFOs) std::cout << "NEW FEDTBMDelay TRIGGER " << event_ << " state " << state << " ";
  std::map<std::string, unsigned int> currentDACValues;
  for (unsigned dacnum = 0; dacnum < tempCalibObject->numberOfScanVariables(); ++dacnum) {
    const std::string& dacname = tempCalibObject->scanName(dacnum);
    const unsigned dacvalue = tempCalibObject->scanValue(tempCalibObject->scanName(dacnum), state);
    currentDACValues[dacname] = dacvalue;
    if (DumpFIFOs) std::cout << dacname << " " << dacvalue << " ";
  }
  if (DumpFIFOs) std::cout << std::endl;

  if(dacsToScan.size() < 2 && currentDACValues["TBMADelay"] != lastTBMADelay){
   event_ = 0;
   lastTBMADelay = currentDACValues["TBMADelay"];
  }

  ////////////////////////////////////////////////////////////////////////////////////////

  for (unsigned ifed = 0; ifed < fedsAndChannels.size(); ++ifed) {
    const unsigned fednumber = fedsAndChannels[ifed].first;
    const unsigned long vmeBaseAddress = theFEDConfiguration_->VMEBaseAddressFromFEDNumber(fednumber);
    PixelFEDInterface* iFED = FEDInterface_[vmeBaseAddress];
    iFED->readDigFEDStatus(false, false);

    //const uint32_t fifoStatus = iFED->getFifoStatus();

    const int MaxChans = 37;    
    uint32_t bufferFifo1[MaxChans][1024];
    int statusFifo1[MaxChans] = {0};

    //iFED->SetFitelFiberSwitchTopDauCard(0); // this should be configurable from outside
    //iFED->SetFitelFiberSwitchBottomDauCard(0);

    /* read fifo1 */
    if( ReadFifo1 ){

      for( unsigned int ch = 0; ch < fedsAndChannels[ifed].second.size(); ch++ ){
       statusFifo1[ch] = iFED->drainFifo1(fedsAndChannels[ifed].second[ch], bufferFifo1[ch], 1024);
      }

      for( unsigned int ch = 0; ch < fedsAndChannels[ifed].second.size(); ch++ ){

       int channel = (fedsAndChannels[ifed].second)[ch];
       bool found_TBMA = false;
       std::vector<int> ch_decodedROCs;

       if (statusFifo1[ch] > 0) {

        DigFIFO1Decoder theFIFO1Decoder(bufferFifo1[ch],statusFifo1[ch]);
        if( theFIFO1Decoder.globalChannel() != channel ) continue;
        found_TBMA = theFIFO1Decoder.foundTBM();
        //if( theFIFO1Decoder.nhits() != 0 ) continue;
        if( !inject_ ) ch_decodedROCs = theFIFO1Decoder.ROCHeaders();
        else{
         for( unsigned int h = 0; h < theFIFO1Decoder.nhits(); ++h ){
          if( std::find(ch_decodedROCs.begin(),ch_decodedROCs.end(),theFIFO1Decoder.rocid(h))==ch_decodedROCs.end() ) ch_decodedROCs.push_back(theFIFO1Decoder.rocid(h));
         }
        } 

        if( DumpFIFOs ){
         std::cout << "-----------------------------------" << std::endl;
         std::cout << "Contents of FIFO 1 for channel " << channel << " (status = " << statusFifo1[ch] << ")" << std::endl;
         std::cout << "-----------------------------------" << std::endl;
         theFIFO1Decoder.printToStream(std::cout);
        }

       }

       const std::vector<PixelROCName>& rocs = theNameTranslation_->getROCsFromFEDChannel(fedsAndChannels[ifed].first, (fedsAndChannels[ifed].second)[ch]);
       FillEm(state, fedsAndChannels[ifed].first, channel, 0, ch_decodedROCs.size(), rocs.size());       
       for( int r = 0; r < (int)rocs.size(); ++r ){
        bool ch_foundROC = std::find(ch_decodedROCs.begin(),ch_decodedROCs.end(),r+1)!=ch_decodedROCs.end();
        if( ch_foundROC ) FillEm(state, fedsAndChannels[ifed].first, channel, 0, 1, r);
       }

      }// end loop on channels

    }//end readFifo1
        
  }

  event_++;
  sendResets();

}

///////////////////////////////////////////////////////////////////////////////////////////////
void PixelFEDROCDelayCalibration::Analyze() {

  PixelCalibConfiguration* tempCalibObject = dynamic_cast<PixelCalibConfiguration*>(theCalibObject_);
  assert(tempCalibObject != 0);
  const std::vector<std::pair<unsigned, std::vector<unsigned> > >& fedsAndChannels = tempCalibObject->fedCardsAndChannels(crate_, theNameTranslation_, theFEDConfiguration_, theDetectorConfiguration_);

  int scanValueMin = tempCalibObject->scanValueMin("TBMADelay");
  scanValueMin = (scanValueMin&192)>>6;
  int mask=0;
  if( scanValueMin==0 ) mask=0;
  else if( scanValueMin==1 ) mask=64;
  else if( scanValueMin==2 ) mask=128;
  else if( scanValueMin==3 ) mask=192;

  int ntriggers = event_-1;

  //normalize by number of triggers
  for( std::map<int,std::map<int,std::vector<TH2F*> > >::iterator it1 = scansROCs.begin(); it1 != scansROCs.end(); ++it1 ){
   for( std::map<int,std::vector<TH2F*> >::iterator it2 = it1->second.begin(); it2 != it1->second.end(); ++it2 ){
    for(unsigned int i = 0; i < it2->second.size(); ++i ) it2->second[i]->Scale(1./ntriggers);
   }
  }

  std::map<std::string,std::map<int,int> > nGoodBinsPerModule;  
  std::map<std::string,int> nROCsPerModule;  
  //fill histo with sum of channels
  for( std::map<int,std::map<int,std::vector<TH2F*> > >::iterator it1 = scansROCs.begin(); it1 != scansROCs.end(); ++it1 ){
   std::string moduleName = "";
   PixelChannel theChannel;
   for( std::map<int,std::vector<TH2F*> >::iterator it2 = it1->second.begin(); it2 != it1->second.end(); ++it2 ){  

    if( theNameTranslation_->FEDChannelExist(it1->first, it2->first) ){
     theChannel = theNameTranslation_->ChannelFromFEDChannel(it1->first, it2->first);
     moduleName = theChannel.modulename();
    }

    const std::vector<PixelROCName>& rocs = theNameTranslation_->getROCsFromFEDChannel(it1->first, it2->first);
    if( nROCsPerModule.find(moduleName) == nROCsPerModule.end() ) nROCsPerModule[moduleName] = rocs.size();
    else nROCsPerModule[moduleName]+=rocs.size();

    //count bins with nrocs == 4/8
    int nGoodBins = 0;     
    for( int bx = 1; bx < it2->second[rocs.size()]->GetNbinsX()+1; ++bx ){
     for( int by = 1; by < it2->second[rocs.size()]->GetNbinsY()+1; ++by ){ 
      if( it2->second[rocs.size()]->GetBinContent(bx,by) == rocs.size() ){
        nGoodBins++;
        //std::cout << " - found bin (" << bx << "," << by << ") nrocs = " << it2->second[rocs.size()]->GetBinContent(bx,by) << std::endl; 
      }
     }//close loop on by
    }//close loop on bx   
    nGoodBinsPerModule[moduleName][it2->first] = nGoodBins;
    ROCsHistoSum[moduleName][0]->Add(it2->second[rocs.size()]);   

   }//close loop on channels
  }//close loop on fed

  std::map<std::string,int> currentTBMAdelay;
  std::map<std::string,int> currentTBMBdelay;
  std::map<std::string,double> nROCsForCurrentROCDelay;
  std::map<std::string,int> passState;
  std::map<std::string,int> bestROCDelay;
  std::map<std::string,double> nROCsForBestROCDelay;
  
  //find best settings for each module
  for( std::map<std::string,std::vector<TH2F*> >::iterator it = ROCsHistoSum.begin(); it != ROCsHistoSum.end(); ++it ){
  
   PixelTBMSettings *TBMSettingsForThisModule=0;
   std::string moduleName=it->first;
   PixelConfigInterface::get(TBMSettingsForThisModule, "pixel/tbm/"+moduleName, *theGlobalKey_);
   assert(TBMSettingsForThisModule!=0);
   
   currentTBMAdelay[moduleName] = TBMSettingsForThisModule->getTBMADelay();
   currentTBMBdelay[moduleName] = TBMSettingsForThisModule->getTBMBDelay();
   
   int port0_A = (currentTBMAdelay[moduleName]&7);  
   int port1_A = (currentTBMAdelay[moduleName]&56)>>3;  
   int port0_B = (currentTBMBdelay[moduleName]&7);  
   int port1_B = (currentTBMBdelay[moduleName]&56)>>3;  
   
   nROCsForCurrentROCDelay[moduleName] = (it->second)[0]->GetBinContent(port1_A+1,port0_A+1);
   bestROCDelay[moduleName] = currentTBMAdelay[moduleName];
   nROCsForBestROCDelay[moduleName] = nROCsForCurrentROCDelay[moduleName];
   
   //first check if current roc delay gives 16 rocs
   if( nROCsForCurrentROCDelay[moduleName] == nROCsPerModule[moduleName] ){
    passState[moduleName] = 1;
   }
   else{// if not then find bins for which nrocs = 16
   
    std::map<int,int> bestBins;
    for( int bx = 1; bx < it->second[0]->GetNbinsX()+1; ++bx ){
     for( int by = 1; by < it->second[0]->GetNbinsY()+1; ++by ){       
      if( it->second[0]->GetBinContent(bx,by) == nROCsPerModule[moduleName] ) bestBins[bx-1] = by-1;
     }
    }
    
    if( bestBins.size() == 0 ){//if no bins are found with nrocs = 16 find the bins with max nrocs
     passState[moduleName] = 0;
 
     double nrocs = 0;
     int binxbest = 0;
     int binybest = 0;
     double maxEff = 0;
     for( int bx = 1; bx < it->second[0]->GetNbinsX()+1; ++bx ){
      for( int by = 1; by < it->second[0]->GetNbinsY()+1; ++by ){       
       if( it->second[0]->GetBinContent(bx,by) >= nrocs ){
        nrocs = it->second[0]->GetBinContent(bx,by);
        binxbest = bx-1;
        binybest = by-1;
       }//close if
      }//close loop on by
     }//close loop on bx

     bestBins[binxbest] = binybest; 
     double maxNrocs = it->second[0]->GetBinContent(binxbest+1,binybest+1);
     //check if there are other bins with same max eff
     for( int bx = 1; bx < it->second[0]->GetNbinsX()+1; ++bx ){
      for( int by = 1; by < it->second[0]->GetNbinsY()+1; ++by ){ 

       if( it->second[0]->GetBinContent(bx,by) == maxNrocs ){       
        std::map<int,int>::iterator binsIt = bestBins.find(bx-1);
        if( binsIt->first != binxbest && binsIt->second != binybest ) bestBins[bx-1] = by-1;
       }

      }//close loop on by
     }//close loop on bx   
    
    }//close bestBins.size() == 0

    if( bestBins.size() == 0 ) passState[moduleName] = 0;//at this point this should not be true
    else{
    
     int bestX = 0;
     int bestY = 0;
     for( std::map<int,int>::iterator binsIt = bestBins.begin(); binsIt != bestBins.end(); ++binsIt ){
      if( binsIt->first >= 0 && binsIt->first <= 7 && binsIt->second >= 0 && binsIt->second <= 7 ){
       bestX = binsIt->first;
       bestY = binsIt->second;
       break;
      }
     }  
     
     if( bestX == 0 && bestY == 0 ){
      bestX = (bestBins.begin())->first;
      bestY = (bestBins.begin())->second;
     }

     if( it->second[0]->GetBinContent(bestX+1,bestY+1) != nROCsPerModule[moduleName] ) passState[moduleName] = 0;
     else passState[moduleName] = 1;

     nROCsForBestROCDelay[moduleName] = it->second[0]->GetBinContent(bestX+1,bestY+1);
     bestX = (bestX<<3)|mask;
     bestY = bestY;
     bestROCDelay[moduleName] = bestX+bestY;
          
    }//found nROCs==16
    
   }//clos case nROCS!=16 for current settings

   //check that the module has enough good bins for each fed channel otherwise set flag = 0
   for( std::map<int,int>::iterator gb = nGoodBinsPerModule[moduleName].begin(); 
        gb != nGoodBinsPerModule[moduleName].end(); ++gb ){
        if( gb->second < 10 ){passState[moduleName] = 0;break;}
   }
   
  }//close loop on modules

  rootf->cd();
  branch theBranch;
  branch_sum theBranch_sum;
  TDirectory* dirSummaries = gDirectory->mkdir("SummaryTrees","SummaryTrees");
  dirSummaries->cd();

  TTree* tree = new TTree("PassState","PassState");
  TTree* tree_sum =new TTree("SummaryInfo","SummaryInfo");
  
  tree->Branch("PassState",&theBranch,"pass/F:moduleName/C",4096000);
  tree_sum->Branch("SummaryInfo",&theBranch_sum,"deltaTBMAdelayX/I:deltaTBMAdelayY/I:deltaTBMBdelayX/I:deltaTBMBdelayY/I:newTBMAdelayX/I:newTBMAdelayY/I:newTBMBdelayX/I:newTBMBdelayX/I:nROCs/D:moduleName/C",4096000);
  rootf->cd();

  ofstream out((outputDir()+"/summary.txt").c_str()); //leave the file method intact for now
  assert(out.good());

  for( std::map<std::string,std::vector<TH2F*> >::iterator it = ROCsHistoSum.begin(); it != ROCsHistoSum.end(); ++it ){

   std::string moduleName=it->first;

   int oldTBMADelayX = (currentTBMAdelay[moduleName]&56)>>3;
   int oldTBMADelayY = (currentTBMAdelay[moduleName]&7);
   int oldTBMBDelayX = (currentTBMBdelay[moduleName]&56)>>3;
   int oldTBMBDelayY = (currentTBMBdelay[moduleName]&7);

   int newTBMADelayX = (bestROCDelay[moduleName]&56)>>3;
   int newTBMADelayY = (bestROCDelay[moduleName]&7);
   int newTBMBDelayX = (bestROCDelay[moduleName]&56)>>3;
   int newTBMBDelayY = (bestROCDelay[moduleName]&7);

   theBranch_sum.deltaTBMAdelayX = newTBMADelayX-oldTBMADelayX;
   theBranch_sum.deltaTBMAdelayY = newTBMADelayY-oldTBMADelayY;
   theBranch_sum.deltaTBMBdelayX = newTBMBDelayX-oldTBMBDelayX;
   theBranch_sum.deltaTBMBdelayY = newTBMBDelayY-oldTBMBDelayY;
   theBranch_sum.newTBMAdelayX = newTBMADelayX;
   theBranch_sum.newTBMAdelayY = newTBMADelayY;
   theBranch_sum.newTBMBdelayX = newTBMBDelayX;
   theBranch_sum.newTBMBdelayY = newTBMBDelayY;
   theBranch_sum.nROCs = nROCsForBestROCDelay[moduleName];
   strcpy(theBranch_sum.moduleName,moduleName.c_str());
   theBranch.pass = passState[moduleName];
   strcpy(theBranch.moduleName,moduleName.c_str());

   tree->Fill();
   tree_sum->Fill();

   PixelTBMSettings *TBMSettingsForThisModule=0;
   PixelConfigInterface::get(TBMSettingsForThisModule, "pixel/tbm/"+moduleName, *theGlobalKey_);
   assert(TBMSettingsForThisModule!=0);
   TBMSettingsForThisModule->setTBMADelay(bestROCDelay[moduleName]);
   TBMSettingsForThisModule->setTBMBDelay(bestROCDelay[moduleName]);
   TBMSettingsForThisModule->writeASCII(outputDir());
   //std::cout << "Wrote TBM settings for module:" << moduleName << endl;			
   delete TBMSettingsForThisModule;

  }


  //now print summary and save it on text file
  std::map<std::string,std::vector<int> > FEDchannelsPerModule;
  for (unsigned ifed = 0; ifed < fedsAndChannels.size(); ++ifed) {
   std::string moduleName = "";
   for( unsigned int ch = 0; ch < fedsAndChannels[ifed].second.size(); ch++ ){
    PixelChannel theChannel = theNameTranslation_->ChannelFromFEDChannel(fedsAndChannels[ifed].first, (fedsAndChannels[ifed].second)[ch]);
    moduleName = theChannel.modulename();
    FEDchannelsPerModule[moduleName].push_back((fedsAndChannels[ifed].second)[ch]);
   }
  }

  std::ofstream ofs(outtext);
  ofs << std::endl;
  ofs << "Inject signal : " << inject_ << std::endl;

  out << "Module                        | FED channels | port 0 phase | port 1 phase | DeltaROCDelay | # ROCs | Pass | \n";
  for( std::map<std::string,std::vector<int> >::iterator it = FEDchannelsPerModule.begin(); it != FEDchannelsPerModule.end(); ++it ){
   out << it->first << " | ";
   ofs << "Module = " << it->first << ", ch = ";

   for( unsigned int i = 0; i < (it->second).size(); ++i ){
     out << (it->second)[i] << " ";
     ofs << (it->second)[i] << " ";
   }
   ofs << std::endl;

   out << " | " << (bestROCDelay[it->first]&7) << "            | " << ((bestROCDelay[it->first]&56)>>3);
   out << "            | " << bestROCDelay[it->first] - currentTBMAdelay[it->first];
   out.precision(4);
   out << "            | " << nROCsForBestROCDelay[it->first] << "  | " << passState[it->first] << "    | \n";



   ofs << "[result] current ROC delay = " << currentTBMAdelay[it->first] << std::endl;
   ofs << "[result] best ROC delay = " << bestROCDelay[it->first] << std::endl;
   ofs << "[result] delta ROC delay = " << bestROCDelay[it->first] - currentTBMAdelay[it->first] << std::endl;
   ofs << "[result] port0 phase = " << (bestROCDelay[it->first]&7) << std::endl;
   ofs << "[result] port1 phase = " << ((bestROCDelay[it->first]&56)>>3) << std::endl;
   ofs << "[result] # ROCs = " << nROCsForBestROCDelay[it->first] << std::endl;
   ofs << "[result] isPass = " << passState[it->first] << std::endl;

  }


  if(writeElog){

    string cmd = "";
    
    for( std::map<std::string,std::vector<TH2F*> >::iterator it = ROCsHistoSum.begin(); it != ROCsHistoSum.end(); ++it ){
      
      TString cname = "Summary_";
      cname += it->first;

      TCanvas *c = new TCanvas(cname, cname);
      it->second[0]->Draw("colztext");
      
      TString filename; 
      filename += outputDir().c_str();
      filename += "/";
      filename += cname;
      filename += ".gif";
      //      filename.Form("%s/%s.gif", outputDir().c_str(), cname);
      c->Print(filename);
      
      cmd += " -f ";
      cmd += filename;
      
    }

    elog->post(runDir(), (string)outtext, cmd);
  }


  CloseRootf();  
}

///////////////////////////////////////////////////////////////////////////////////////////////
void PixelFEDROCDelayCalibration::CloseRootf() {
  if (rootf) {
    rootf->Write();
    rootf->Close();
    delete rootf;
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////
void PixelFEDROCDelayCalibration::BookEm(const TString& path) {

  scansROCs.clear();
  ROCsHistoSum.clear();

  gStyle->SetPaintTextFormat("2.2f");
  gStyle->SetOptStat(0);

  TString root_fn;
  if (path == "")
    root_fn.Form("%s/ROCDelay.root", outputDir().c_str());
  else
    root_fn.Form("%s/ROCDelay_%s.root", outputDir().c_str(), path.Data());
  cout << "writing histograms to file " << root_fn << endl;
  CloseRootf();
  rootf = new TFile(root_fn, "create");
  assert(rootf->IsOpen());

  PixelCalibConfiguration* tempCalibObject = dynamic_cast<PixelCalibConfiguration*>(theCalibObject_);
  assert(tempCalibObject != 0);
  const std::vector<std::pair<unsigned, std::vector<unsigned> > >& fedsAndChannels = tempCalibObject->fedCardsAndChannels(crate_, theNameTranslation_, theFEDConfiguration_, theDetectorConfiguration_);

  PixelRootDirectoryMaker rootDirs(fedsAndChannels,gDirectory);

  for (unsigned ifed = 0; ifed < fedsAndChannels.size(); ++ifed) {
   
   std::map<int,std::vector<TH1F*> > chTBMmap;
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
    TString hname; hname.Form("Ch%i",(fedsAndChannels[ifed].second)[ch]);
    h_nROCHeaders = new TH2F(hname+"_nROCHeaders", hname+"_nROCHeaders", 8, 0, 8, 8, 0, 8 );
    histosROCs.push_back(h_nROCHeaders);     
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
   h_nROCHeaders->SetXTitle("Port 1 phase");
   h_nROCHeaders->SetYTitle("Port 0 phase");
   histosROC.push_back(h_nROCHeaders);   
   ROCsHistoSum[moduleName] = histosROC;
    
  }

  rootf->cd(0);

}

///////////////////////////////////////////////////////////////////////////////////////////////
void PixelFEDROCDelayCalibration::FillEm(unsigned state, int fedid, int ch, int which, float c, int roc) {
  PixelCalibConfiguration* tempCalibObject = dynamic_cast<PixelCalibConfiguration*>(theCalibObject_);
  assert(tempCalibObject != 0);

  if (event_==0) return;

  const std::string& iname = dacsToScan[0];
  const double ival(tempCalibObject->scanValue(iname, state)); 
  uint32_t tmp = ival; 
  int delay1 = (tmp>>3)&0x7;
  int delay2 = tmp&0x7;
  scansROCs[fedid][ch][roc]->Fill(delay1,delay2,c);


}

