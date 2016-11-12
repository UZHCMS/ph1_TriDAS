#include "CalibFormats/SiPixelObjects/interface/PixelCalibConfiguration.h"
//#include "CalibFormats/SiPixelObjects/interface/PixelDACNames.h"
#include "CalibFormats/SiPixelObjects/interface/PixelPortCardSettingNames.h"
#include "CalibFormats/SiPixelObjects/interface/PixelPortcardMap.h"
#include "PixelCalibrations/include/PixelFEDPOHBiasCalibration.h"
#include "PixelCalibrations/include/PixelPOHBiasCalibration.h"
#include "PixelCalibrations/include/PixelCalibrationBase.h"
#include "PixelConfigDBInterface/include/PixelConfigInterface.h"
#include "PixelUtilities/PixelFEDDataTools/include/PixelFEDDataTypes.h"
#include "PixelUtilities/PixelFEDDataTools/include/ErrorFIFODecoder.h"
#include "PixelUtilities/PixelFEDDataTools/include/ColRowAddrDecoder.h"
#include "PixelUtilities/PixelFEDDataTools/include/DigScopeDecoder.h"
#include "PixelUtilities/PixelFEDDataTools/include/DigTransDecoder.h"
#include "PixelUtilities/PixelFEDDataTools/include/FIFO3Decoder.h"
#include "PixelUtilities/PixelFEDDataTools/include/DigFIFO1Decoder.h"

#include "TFile.h"
#include "TH1F.h"
#include "TH2F.h"
#include "TTree.h"
#include <iomanip>
#include <algorithm>
#include "PixelCalibrations/include/PixelPOHBiasCalibrationParameters.h"


#include <toolbox/convertstring.h>



using namespace pos;
using namespace PixelPOHBiasCalibrationParameters;


///////////////////////////////////////////////////////////////////////////////////////////////
PixelFEDPOHBiasCalibration::PixelFEDPOHBiasCalibration(const PixelFEDSupervisorConfiguration & tempConfiguration, SOAPCommander* mySOAPCmdr)
  : PixelFEDCalibrationBase(tempConfiguration,*mySOAPCmdr)
{
  std::cout << "In PixelFEDPOHBiasCalibration copy ctor()" << std::endl;
}

///////////////////////////////////////////////////////////////////////////////////////////////
void PixelFEDPOHBiasCalibration::initializeFED() {
  setFEDModeAndControlRegister(0x8, 0x30010);
  printIfSlinkHeaderMessedup_off();
  sendResets();
  
}

///////////////////////////////////////////////////////////////////////////////////////////////
xoap::MessageReference PixelFEDPOHBiasCalibration::beginCalibration(xoap::MessageReference msg) {
  std::cout << "In PixelFEDPOHBiasCalibration::beginCalibration()" << std::endl;

  PixelCalibConfiguration* tempCalibObject = dynamic_cast<PixelCalibConfiguration*>(theCalibObject_);
  assert(tempCalibObject != 0);

  //  tempCalibObject->writeASCII(outputDir());
  DumpFIFOs = tempCalibObject->parameterValue("DumpFIFOs") == "yes";

  std::cout << "DumpFIFOs = " <<DumpFIFOs << std::endl;

  setFIFO1Mode();//jen

  //  BookEm("");

  xoap::MessageReference reply = MakeSOAPMessageReference("BeginCalibrationDone");
  return reply;
}

///////////////////////////////////////////////////////////////////////////////////////////////
xoap::MessageReference PixelFEDPOHBiasCalibration::execute(xoap::MessageReference msg) {
  Attribute_Vector parameters(5);
  parameters[0].name_ = "WhatToDo";
  parameters[1].name_ = "AOHBias";
  parameters[2].name_ = "AOHGain";
  parameters[3].name_ = "fednumber";
  parameters[4].name_ = "channel";
  Receive(msg, parameters);

  const unsigned AOHBias = atoi(parameters[1].value_.c_str());
  const unsigned AOHGain = atoi(parameters[2].value_.c_str());
  const unsigned fednumber = atoi(parameters[3].value_.c_str());
  const unsigned channel = atoi(parameters[4].value_.c_str());

    
  xoap::MessageReference reply = MakeSOAPMessageReference("FEDCalibrationsDone");
  
  if (parameters[0].value_ == "RetrieveData")
    reply = RetrieveData(AOHBias, AOHGain, fednumber, channel);
  else {
    cout << "ERROR: PixelFEDPOHBiasCalibration::execute() does not understand the WhatToDo command, "<< parameters[0].value_ <<", sent to it.\n";
    assert(0);
  }

  return reply;
}

///////////////////////////////////////////////////////////////////////////////////////////////
xoap::MessageReference PixelFEDPOHBiasCalibration::endCalibration(xoap::MessageReference msg) {
  
  std::cout << "In PixelFEDPOHBiasCalibration::endCalibration()" << std::endl;
  xoap::MessageReference reply = MakeSOAPMessageReference("EndCalibrationDone");
  return reply;
}

///////////////////////////////////////////////////////////////////////////////////////////////
xoap::MessageReference PixelFEDPOHBiasCalibration::RetrieveData(unsigned AOHBias, unsigned AOHGain, unsigned int fednumber, unsigned int channel){

  //  std::cout << "Enter retrieve data @ event = " << event_ << std::endl;
  //  std::cout << "AOHBias = " << AOHBias << ", AOHGain = " << AOHGain << " fednumber = " << fednumber << ", channel = " << channel << std::endl;

 
  PixelCalibConfiguration* tempCalibObject = dynamic_cast <PixelCalibConfiguration*> (theCalibObject_);
  assert(tempCalibObject!=0);

  const unsigned long vmeBaseAddress = theFEDConfiguration_->VMEBaseAddressFromFEDNumber(fednumber);
  PixelFEDInterface* iFED = FEDInterface_[vmeBaseAddress];

  uint32_t bufferFifo1[1024];
  int statusFifo1 = iFED->drainFifo1(channel, bufferFifo1, 1024);
     
  bool found_TBMA = false;

  if (statusFifo1 > 0) {

    DigFIFO1Decoder theFIFO1Decoder(bufferFifo1,statusFifo1);

    if( (int)theFIFO1Decoder.globalChannel() == (int)channel ){
      found_TBMA = theFIFO1Decoder.foundTBM();

      if( DumpFIFOs ){	
	std::cout << "-----------------------------------" << std::endl;
	std::cout << "Contents of FIFO 1 for channel " << channel << " (status = " << statusFifo1 << ")" << std::endl;
	std::cout << "-----------------------------------" << std::endl;
	theFIFO1Decoder.printToStream(std::cout);
      }

    }
  }

  
  event_++;
  sendResets();

  Attribute_Vector returnValues(1);
  returnValues[0].name_="foundTBMA"; 
  returnValues[0].value_= itoa((int)found_TBMA);

  xoap::MessageReference reply = MakeSOAPMessageReference("FEDCalibrationsDone", returnValues);

  return reply;
  
}


///////////////////////////////////////////////////////////////////////////////////////////////
//void PixelFEDPOHBiasCalibration::Analyze() {
//
//  std::cout << "[DEBUG] entering Analyze function @ event_ = " << event_ << std::endl;
//
//  PixelCalibConfiguration* tempCalibObject = dynamic_cast<PixelCalibConfiguration*>(theCalibObject_);
//  assert(tempCalibObject != 0);
//
//  //  const std::set<unsigned int> fedcrates=tempCalibObject->getFEDCrates(theNameTranslation_, theFEDConfiguration_);
//  //  const std::set<unsigned int> TKFECcrates=tempCalibObject->getTKFECCrates(thePortcardMap_, *getmapNamePortCard(), theTKFECConfiguration_);
//
//  unsigned int nTriggersPerPOHBias = k_nTriggersPerPOHBias_default;
//  if ( tempCalibObject->parameterValue("nTriggersPerPOHBias") != "" ) nTriggersPerPOHBias = atoi(tempCalibObject->parameterValue("nTriggersPerPOHBias").c_str());
//
//  int fedcount = 0;
//
//  for(std::map<int,std::map<int, std::map<int, TH1F* > > >::iterator it1 = effmap.begin(); it1 != effmap.end(); ++it1){
//    fedcount ++;
//    
//    int channelcount = 0;
//    for(std::map<int, std::map<int, TH1F* > >::iterator it2 = it1->second.begin(); it2 != it1->second.end(); ++it2){
//      channelcount ++;
//      
//      for(std::map<int, TH1F*>::iterator it3 = it2->second.begin(); it3 != it2->second.end(); ++it3){
//
//	Int_t fednumber = it1->first;
//	Int_t channel = it2->first;
//	Int_t AOHGain = it3->first;
//
//	std::cout << "[summary] fednumber = " << fednumber << std::endl;
//	std::cout << "[summary] channel = " << channel << std::endl;
//	std::cout << "[summary] AOHGain = " << AOHGain << std::endl;
//	
//	//	std::cout << it3->second->GetBinContent(1) << std::endl;
//	
//	//	bool isStable = false;
//	//	bool isLatePlateau = false;
//	bool reach_plateau = false;
//
//	Int_t counter_fail = 0;
//	Int_t counter_success = 0;
//
//	Int_t firstPlateau = 999;
//
//	std::cout << "number of bins = " << it3->second->GetXaxis()->GetNbins() << std::endl;
//	
//	for(int ibin=1; ibin < it3->second->GetXaxis()->GetNbins()+1; ibin++){
//	  Float_t yval = it3->second->GetBinContent(ibin);
//	  std::cout << "yval = " << yval << std::endl;
//
//	  if(yval==nTriggersPerPOHBias){
//	    std::cout << "reached plateau at " << it3->second->GetBinLowEdge(ibin) << std::endl;
//	    if(reach_plateau==false) firstPlateau = it3->second->GetBinLowEdge(ibin);
//	    reach_plateau = true;
//	  }
//	  
//
//	  if(reach_plateau){
//	    if(yval==nTriggersPerPOHBias) counter_success ++;
//	    else counter_fail ++;
//	  }
//
//	}
//	
//
//
//	Float_t plateau_eff = 0;
//	if(counter_success+counter_fail!=0){
//	  plateau_eff = counter_success/(counter_success + counter_fail);
//	}
//
//	std::cout << "plateau eff. = " << plateau_eff << std::endl;
//
//	if(plateau_eff == 1) b_isStable = 1;
//	else b_isStable = 0;
//
//	if(firstPlateau < 15) b_isFirstPlateau = 1;
//	else b_isFirstPlateau = 0;
//
//	std::cout << "check6 : " << firstPlateau << "-> isFirstPlateau = " << b_isFirstPlateau << std::endl;
//
//	b_isPass = (b_isStable==1 && b_isFirstPlateau==1);
//	b_channel = channel;
//	b_fednumber = fednumber;
//	b_AOHGain = AOHGain;
//	tree->Fill();
//	
//	// store here the best AOH Bias values ! 
//	//	bias_values_by_portcard_and_aoh_new[portCardName][AOHNumber] = selectedBiasValue;
//
//	if(AOHGain==2){
//	  summarymap->SetBinContent(fedcount, channelcount, b_isPass);
//	}
//
//      }
//    }
//  }
//
//
//
//
//
//  //bias_values_by_portcard_and_aoh_new[portCardName][AOHNumber] = selectedBiasValue;
//  //Write out the configs
//
//  // mimic PixelSuperVisorConfiguration ... 
////  PixelPortcardMap *thePortCardMap_;
////  pos::PixelTKFECConfig *theTKFECConfiguration_;
////
////  PixelConfigInterface::get(thePortCardMap_, "pixel/portcardmap/", *theGlobalKey_);
////  PixelConfigInterface::get(theTKFECConfiguration_, "pixel/tkfecconfig/", *theGlobalKey_);
////
////  std::map<std::string,pos::PixelPortCardConfig*> * getmapNamePortCard();
////
////  tempCalibObject->getTKFECCrates(thePortCardMap_, *getmapNamePortCard(), theTKFECConfiguration_);
//
////  const std::set<PixelChannel>& channelsToCalibrate = tempCalibObject->channelList();
////  for(std::set<PixelChannel>::const_iterator channelsToCalibrate_itr = channelsToCalibrate.begin();
////      channelsToCalibrate_itr != channelsToCalibrate.end(); channelsToCalibrate_itr++){
////
////    const PixelHdwAddress& channelHdwAddress = theNameTranslation_->getHdwAddress(*channelsToCalibrate_itr);
////    const unsigned int NFed = channelHdwAddress.fednumber();
////    const unsigned int NChannel = channelHdwAddress.fedchannel();
////    const unsigned int NFiber = NChannel/2;
////
////    std::cout << NFed << " " << NChannel << " " << NFiber << std::endl;
////
////    const std::pair< std::string, int > portCardAndAOH = thePortCardMap_->PortCardAndAOH(*channelsToCalibrate_itr);
////    const std::string portCardName = portCardAndAOH.first; assert(portCardName!="none");
////    const int AOHNumber = portCardAndAOH.second;
////
////    std::cout << portCardName << " " << AOHNumber << std::endl;
////
////    // This should be fixed !
////    bias_values_by_portcard_and_aoh_new[portCardName][AOHNumber] = 1.;
////  }
////
////
////  for ( std::map< std::string, std::map< unsigned int, unsigned int > >::iterator portCardName_itr = bias_values_by_portcard_and_aoh_new.begin(); 
////	portCardName_itr != bias_values_by_portcard_and_aoh_new.end(); ++portCardName_itr ){
////
////    std::string portCardName = portCardName_itr->first;
////    std::map<std::string,PixelPortCardConfig*>::iterator mapNamePortCard_itr = getmapNamePortCard()->find(portCardName);
////    assert( mapNamePortCard_itr != getmapNamePortCard()->end() );
////    PixelPortCardConfig* thisPortCardConfig = mapNamePortCard_itr->second;
////		
////    for ( std::map< unsigned int, unsigned int >::iterator AOHNumber_itr = portCardName_itr->second.begin(); AOHNumber_itr != portCardName_itr->second.end(); ++AOHNumber_itr )
////      {
////	unsigned int AOHNumber = AOHNumber_itr->first;
////	unsigned int AOHBiasAddress = thisPortCardConfig->AOHBiasAddressFromAOHNumber(AOHNumber);
////	thisPortCardConfig->setdeviceValues(AOHBiasAddress, bias_values_by_portcard_and_aoh_new[portCardName][AOHNumber]);
////	
////      }
////		
////    //    thisPortCardConfig->writeASCII(outputDir());
////    std::cout << "Wrote portcard_"+portCardName+".dat"<<endl;
////  }
//
//
//
//  CloseRootf();
//
//}



///////////////////////////////////////////////////////////////////////////////////////////////
//void PixelFEDPOHBiasCalibration::CloseRootf() {
//  if (rootf) {
//    rootf->Write();
//    rootf->Close();
//    delete rootf;
//  }
//}

///////////////////////////////////////////////////////////////////////////////////////////////
//void PixelFEDPOHBiasCalibration::BookEm(const TString& path) {
//
//  TString root_fn;
//  if (path == "")
//    root_fn.Form("%s/POHBias.root", outputDir().c_str());
//  else
//    root_fn.Form("%s/POHBias_%s.root", outputDir().c_str(), path.Data());
//
//  cout << "writing histograms to file " << root_fn << endl;
//  CloseRootf();
//
//  rootf = new TFile(root_fn, "create");
//  assert(rootf->IsOpen());
//
//  tree = new TTree("tree", "tree");
//
//  tree->Branch("channel",&b_channel,"channel/I");
//  tree->Branch("fednumber",&b_fednumber,"fednumber/I");
//  tree->Branch("AOHGain",&b_AOHGain,"AOHGain/I");
//  tree->Branch("isPass",&b_isPass,"isPass/I");
//  tree->Branch("isStable",&b_isStable,"isStable/I");
//  tree->Branch("isFirstPlateau",&b_isFirstPlateau,"isFirstPlateau/I");
//    
//
//  PixelCalibConfiguration* tempCalibObject = dynamic_cast<PixelCalibConfiguration*>(theCalibObject_);
//  assert(tempCalibObject != 0);
//
//
//  unsigned int AOHBiasMin = k_ScanMin_default;
//  if ( tempCalibObject->parameterValue("ScanMin") != "" ) AOHBiasMin = atoi(tempCalibObject->parameterValue("ScanMin").c_str());
//
//  unsigned int AOHBiasMax = k_ScanMax_default;;
//  if ( tempCalibObject->parameterValue("ScanMax") != "" ) AOHBiasMax = atoi(tempCalibObject->parameterValue("ScanMax").c_str());
//
//  unsigned int AOHBiasStepSize = k_ScanStepSize_default;
//  if ( tempCalibObject->parameterValue("ScanStepSize") != "" ) AOHBiasStepSize = atoi(tempCalibObject->parameterValue("ScanStepSize").c_str());
//
//  unsigned int AOHGainMin = k_GainMin_default;
//  if ( tempCalibObject->parameterValue("GainMin") != "" ) AOHGainMin = atoi(tempCalibObject->parameterValue("GainMin").c_str());
//
//  unsigned int AOHGainMax = k_GainMax_default;;
//  if ( tempCalibObject->parameterValue("GainMax") != "" ) AOHGainMax = atoi(tempCalibObject->parameterValue("GainMax").c_str());
//
//  unsigned int AOHGainStepSize = k_GainStepSize_default;
//  if ( tempCalibObject->parameterValue("GainStepSize") != "" ) AOHGainStepSize = atoi(tempCalibObject->parameterValue("GainStepSize").c_str());
//
//
//  const std::vector<std::pair<unsigned, std::vector<unsigned> > >& fedsAndChannels = tempCalibObject->fedCardsAndChannels(crate_, theNameTranslation_, theFEDConfiguration_, theDetectorConfiguration_);
//
//  PixelRootDirectoryMaker rootDirs(fedsAndChannels,gDirectory);
//
//  std::vector<int> fed_contents;
//  std::vector<int> channel_contents;
//  
//
//  for (unsigned ifed = 0; ifed < fedsAndChannels.size(); ++ifed) {
//
//    const unsigned fednumber = fedsAndChannels[ifed].first;    
//
//    fed_contents.push_back(fednumber);
//    std::cout << "[Analyze] fednumber : " << fednumber << std::endl;
//
//    std::map<int, std::map<int, TH1F*>> chmap;
//    
//    for( unsigned int ch = 0; ch < fedsAndChannels[ifed].second.size(); ch++ ){
//
//      int channel = (fedsAndChannels[ifed].second)[ch];
//      if(ifed==0) channel_contents.push_back(channel); // avoid over-counting
//      
//      std::cout << "[Analyze] channel : " << channel << std::endl;
//      
//      rootDirs.cdDirectory(fedsAndChannels[ifed].first, (fedsAndChannels[ifed].second)[ch]);
//      
//      std::map<int, TH1F*> gainmap;
//      TH1F *h_eff;
//
//      for (unsigned int AOHGain=AOHGainMin; AOHGain <= AOHGainMax; ++AOHGain){      
//	
//	TString hname; 
//	hname.Form("Ch%i_Gain%i",(fedsAndChannels[ifed].second)[ch], AOHGain);
//	
//	h_eff = new TH1F(hname+"_eff", hname+"_eff", (AOHBiasMax - AOHBiasMin), AOHBiasMin, AOHBiasMax);
//	h_eff->Sumw2();
//	gainmap[AOHGain] = h_eff;
//	
//      }
//      
//      chmap[(fedsAndChannels[ifed].second)[ch]] = gainmap;
//      
//    }
//
//    effmap[fedsAndChannels[ifed].first] = chmap;
//  }//close loop on feds
//
//   
//  rootf->cd();
//
//
//  summarymap = new TH2F("summarymap", "summarymap", 
//			fed_contents.size(), 0, fed_contents.size(),
//			channel_contents.size(), 0, channel_contents.size()
//			);
//  
//  for(int xbin=1; xbin < summarymap->GetXaxis()->GetNbins()+1; xbin++){
//    TString xlabel; xlabel.Form("FED%i", fed_contents.at(xbin-1));
//
//    summarymap->GetXaxis()->SetBinLabel(xbin, xlabel);
//  }
//  for(int ybin=1; ybin < summarymap->GetYaxis()->GetNbins()+1; ybin++){
//    TString ylabel; ylabel.Form("Ch%i", channel_contents.at(ybin-1));
//
//    summarymap->GetYaxis()->SetBinLabel(ybin, ylabel);
//  }
//  summarymap->GetXaxis()->SetTitle("FED number");
//  summarymap->GetYaxis()->SetTitle("Channel");
//
//  
//  rootf->cd();
//}

///////////////////////////////////////////////////////////////////////////////////////////////
//void PixelFEDPOHBiasCalibration::FillEm(unsigned AOHBias, unsigned AOHGain, int fedid, int ch, int which) {
//
//
//  std::cout << "[DEBUG] FilEm = " << " "<< AOHBias << " " << AOHGain << " " << fedid << " " << ch << " " << which << std::endl;
//  
//  if(which){
//    std::cout << "Fill in efficiency for fedid = " << fedid << " ch = " << ch << " AOHBias = "  << AOHBias << " " << std::endl;
//
//    effmap[fedid][ch][AOHGain]->Fill(AOHBias);
//  }
//
//}



