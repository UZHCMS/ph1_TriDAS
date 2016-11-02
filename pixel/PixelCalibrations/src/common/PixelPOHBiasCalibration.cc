#include "CalibFormats/SiPixelObjects/interface/PixelCalibConfiguration.h"
#include "CalibFormats/SiPixelObjects/interface/PixelPortCardSettingNames.h"
#include "PixelCalibrations/include/PixelPOHBiasCalibration.h"
#include "PixelCalibrations/include/PixelPOHBiasCalibrationParameters.h"

#include "PixelUtilities/PixelRootUtilities/include/PixelRootDirectoryMaker.h"
#include <toolbox/convertstring.h>
#include <stdlib.h>
#include <fstream>

using namespace pos;
using namespace std;
using namespace PixelPOHBiasCalibrationParameters;


PixelPOHBiasCalibration::PixelPOHBiasCalibration(const PixelSupervisorConfiguration & tempConfiguration, SOAPCommander* mySOAPCmdr)
  : PixelCalibrationBase(tempConfiguration, *mySOAPCmdr)
{
  std::cout << "Greetings from the PixelPOHBiasCalibration copy constructor." << std::endl;
}


void PixelPOHBiasCalibration::beginCalibration() {
  PixelCalibConfiguration* tempCalibObject = dynamic_cast<PixelCalibConfiguration*>(theCalibObject_);
  assert(tempCalibObject != 0);

  AOHBiasMin = k_ScanMin_default;
  if ( tempCalibObject->parameterValue("ScanMin") != "" ) AOHBiasMin = atoi(tempCalibObject->parameterValue("ScanMin").c_str());

  AOHBiasMax = k_ScanMax_default;;
  if ( tempCalibObject->parameterValue("ScanMax") != "" ) AOHBiasMax = atoi(tempCalibObject->parameterValue("ScanMax").c_str());

  AOHBiasStepSize = k_ScanStepSize_default;
  if ( tempCalibObject->parameterValue("ScanStepSize") != "" ) AOHBiasStepSize = atoi(tempCalibObject->parameterValue("ScanStepSize").c_str());

  AOHGainMin = k_GainMin_default;
  if ( tempCalibObject->parameterValue("GainMin") != "" ) AOHGainMin = atoi(tempCalibObject->parameterValue("GainMin").c_str());

  AOHGainMax = k_GainMax_default;;
  if ( tempCalibObject->parameterValue("GainMax") != "" ) AOHGainMax = atoi(tempCalibObject->parameterValue("GainMax").c_str());

  AOHGainStepSize = k_GainStepSize_default;
  if ( tempCalibObject->parameterValue("GainStepSize") != "" ) AOHGainStepSize = atoi(tempCalibObject->parameterValue("GainStepSize").c_str());

  nTriggersPerPOHBias = k_nTriggersPerPOHBias_default;
  if ( tempCalibObject->parameterValue("nTriggersPerPOHBias") != "" ) nTriggersPerPOHBias = atoi(tempCalibObject->parameterValue("nTriggersPerPOHBias").c_str());

  AllowPlateau = k_AllowPlateau_default;
  if ( tempCalibObject->parameterValue("AllowPlateau") != "" ) AllowPlateau = atof(tempCalibObject->parameterValue("AllowPlateau").c_str());

  writeElog = tempCalibObject->parameterValue("writeElog") == "yes";


  std::cout << "[INFO] AOHBiasMin = " << AOHBiasMin << std::endl;
  std::cout << "[INFO] AOHBiasMax = " << AOHBiasMax << std::endl;
  std::cout << "[INFO] AOHBiasStepSize = " << AOHBiasStepSize << std::endl;
  std::cout << "[INFO] AOHGainMin = " << AOHGainMin << std::endl;
  std::cout << "[INFO] AOHGainMax = " << AOHGainMax << std::endl;
  std::cout << "[INFO] AOHGainStepSize = " << AOHGainStepSize << std::endl;
  std::cout << "[INFO] nTriggersPerPOHBias = " << nTriggersPerPOHBias << std::endl;
  std::cout << "[INFO] AllowPlateau = " << AllowPlateau << std::endl;
  std::cout << "[INFO] writeElog = " << writeElog << std::endl;

  tempCalibObject->writeASCII(outputDir());
  std::cout << "[INFO] OutputDir = " << outputDir() << std::endl;

  outtext.Form("%s/log.txt", outputDir().c_str());
  
  BookEm();
}


bool PixelPOHBiasCalibration::execute() {
  PixelCalibConfiguration* tempCalibObject = dynamic_cast<PixelCalibConfiguration*>(theCalibObject_);
  assert(tempCalibObject != 0);

  const bool firstOfPattern = event_ % tempCalibObject->nTriggersPerPattern() == 0;
  reportProgress(0.05);

  // Configure all TBMs and ROCs according to the PixelCalibConfiguration settings, but only when it's time for a new configuration.
  if (firstOfPattern) {
    commandToAllFECCrates("CalibRunning");
  }

  // should take this out
  commandToAllFEDCrates("JMTJunk");


  const std::set<unsigned int> fedcrates=tempCalibObject->getFEDCrates(theNameTranslation_, theFEDConfiguration_);
  const std::set<unsigned int> TKFECcrates=tempCalibObject->getTKFECCrates(thePortcardMap_, *getmapNamePortCard(), theTKFECConfiguration_);


  // Scan the AOH Bias 
  const std::set<PixelChannel>& channelsToCalibrate = tempCalibObject->channelList();
  
  for (unsigned int AOHBias = AOHBiasMin; AOHBias <= AOHBiasMax; AOHBias += AOHBiasStepSize){
      
    // Set AOH bias
    Attribute_Vector parametersToTKFEC(1);
    parametersToTKFEC[0].name_="AOHBias"; parametersToTKFEC[0].value_=itoa(AOHBias);
    commandToAllTKFECCrates("SetAOHBiasEnMass", parametersToTKFEC);
    
    //    std::cout << "[INFO] POH bias = " << AOHBias << std::endl;
    
    for (unsigned int AOHGain=AOHGainMin; AOHGain <= AOHGainMax; ++AOHGain){
      
      Attribute_Vector parametersToTKFEC_Gain(1);
      parametersToTKFEC_Gain[0].name_="AOHGain"; parametersToTKFEC_Gain[0].value_=itoa(AOHGain);
      commandToAllTKFECCrates("SetAOHGainEnMass", parametersToTKFEC_Gain);
      
      //      std::cout << "[INFO] POH gain = " << AOHGain << std::endl;
      
      for ( std::set<PixelChannel>::const_iterator channelsToCalibrate_itr = channelsToCalibrate.begin(); channelsToCalibrate_itr != channelsToCalibrate.end(); channelsToCalibrate_itr++ ){
	
	// Get info about this channel.
	const PixelHdwAddress& channelHdwAddress = theNameTranslation_->getHdwAddress(*channelsToCalibrate_itr);
	const unsigned int fednumber = channelHdwAddress.fednumber();
	const unsigned int fedcrate=theFEDConfiguration_->crateFromFEDNumber( fednumber );
	const unsigned int channel = channelHdwAddress.fedchannel();

	for (unsigned int i_event=0; i_event < nTriggersPerPOHBias; ++i_event){

	  sendTTCCalSync(); 
  
	  // Send trigger to all TBMs and ROCs.
	  
	  Attribute_Vector parametersToFED(5);
	  parametersToFED[0].name_ = "WhatToDo"; 
	  parametersToFED[1].name_ = "AOHBias"; 
	  parametersToFED[2].name_ = "AOHGain";
	  parametersToFED[3].name_ = "fednumber";
	  parametersToFED[4].name_ = "channel";
	  
	  parametersToFED[0].value_ = "RetrieveData";
	  parametersToFED[1].value_ = itoa(AOHBias);
	  parametersToFED[2].value_ = itoa(AOHGain);
	  parametersToFED[3].value_ = itoa(fednumber);
	  parametersToFED[4].value_ = itoa(channel);

	  xoap::MessageReference replyFromFED = SendWithSOAPReply(PixelFEDSupervisors_[fedcrate], "FEDCalibrations", parametersToFED);


	  Attribute_Vector returnValuesFromFED(1);
	  returnValuesFromFED[0].name_="foundTBMA";
	  Receive(replyFromFED, returnValuesFromFED);

	  Int_t found_ = (int)atoi(returnValuesFromFED[0].value_.c_str());

	  if(found_) histos[fednumber][channel][AOHGain]->Fill(AOHBias);
	}
      }
    }
  } // end of loop over AOHBias values

  

  return event_ + 1 < tempCalibObject->nTriggersTotal();
}

void PixelPOHBiasCalibration::endCalibration() {

  PixelCalibConfiguration* tempCalibObject = dynamic_cast<PixelCalibConfiguration*>(theCalibObject_);
  assert(tempCalibObject != 0);
  assert(event_ == tempCalibObject->nTriggersTotal());


  // prepare for e-log
  std::cout << "Make e-log message" << std::endl;

  std::ofstream ofs(outtext);
  ofs << std::endl;
  ofs << "AOHBias Min = " << AOHBiasMin << std::endl;
  ofs << "AOHBias Max = " << AOHBiasMax << std::endl;
  ofs << "AOHBias Stepsize = " << AOHBiasStepSize << std::endl;
  ofs << "AOHGain Min = " << AOHGainMin << std::endl;
  ofs << "AOHGain Max = " << AOHGainMax << std::endl;
  ofs << "AOHGain Stepsize = " << AOHGainStepSize << std::endl;
  ofs << "nTriggersPerPOHBias = " << nTriggersPerPOHBias << std::endl;
  ofs << "Plateau tolerance = " << AllowPlateau << std::endl;
  ofs << std::endl;

  for(int index=0; index < (int)fednumber_.size(); index++){

    int fed = fednumber_[index];
    int channel = channel_[index];
    int aoh = aohnumber_[index];
    string portcard = portcard_[index];

    Int_t firstPlateau_forPOH = -1;
    
    bool isNoPlateau = false;
	
    for(int channel_index = 0; channel_index < 2; channel_index++){
      for (unsigned int AOHGain=AOHGainMin; AOHGain <= AOHGainMax; ++AOHGain){      

	
	if(AOHGain!=2) continue;

	Int_t ch = channel + channel_index;
	
	histos[fed][ch][AOHGain]->Scale(1./nTriggersPerPOHBias);

	bool reach_plateau = false;
	Int_t firstPlateau = -1;
	Float_t plateau_eff = 0;
	Float_t nplateau = 0;

	for(int ibin=1; ibin < histos[fed][ch][AOHGain]->GetXaxis()->GetNbins()+1; ibin++){

	  Float_t yval = histos[fed][ch][AOHGain]->GetBinContent(ibin);

	  if(yval==1){
	    if(reach_plateau==false){
	      firstPlateau = histos[fed][ch][AOHGain]->GetBinLowEdge(ibin);
	      if(firstPlateau_forPOH < firstPlateau) firstPlateau_forPOH = firstPlateau;
	    }
	    reach_plateau = true;
	  }
	  

	  if(reach_plateau){
	    plateau_eff += yval;
	    nplateau+=1;
	    //	    std::cout << "eff = " << plateau_eff << " yval =" << yval << " " << nplateau << std::endl;
	  }
	}

	if(nplateau!=0){
	  plateau_eff /= nplateau;	
	}


	// First plateau
	summary[fed]->SetBinContent(ch, 1, firstPlateau);
	summary[fed]->SetBinContent(ch, 2, plateau_eff);

	if(reach_plateau==false) isNoPlateau = true;


	if(plateau_eff > AllowPlateau) b_isStable = 1;
	else b_isStable = 0;

	if(firstPlateau < 25 && firstPlateau >= 0) b_isFastPlateau = 1;
	else b_isFastPlateau = 0;

	b_isPass = (b_isStable==1 && b_isFastPlateau==1);
	b_channel = ch;
	b_plateauX = firstPlateau;
	b_fednumber = fed;
	b_plateauEff = plateau_eff;
	b_AOHGain = AOHGain;

	tree->Fill();

	ofs << "[result] FED = " << fed << ", channel = " << ch << ", first plateau = " << firstPlateau << " (isFastPlateau = " << b_isFastPlateau << "), plateau eff = " << plateau_eff << " (isStable = " << b_isStable << ") ==> isPass = " << b_isPass  << std::endl;
      }
    }

    //    std::cout << "-----------------------------------------" << std::endl;
    //    std::cout << "New settings : FED" << fed << ", AOH" << aoh << " (portcard : " << portcard << ") : bias = " << firstPlateau_forPOH +5 <<  std::endl;
    //    std::cout << "-----------------------------------------" << std::endl;

    if(firstPlateau_forPOH==-1 || isNoPlateau==true){
      bias_values_by_portcard_and_aoh_new[portcard][aoh] = 30;
      ofs << std::endl;
      ofs << "[warning] portCard = " << portcard << ", AOH number = " << aoh << " could not find optimal value (did not reach plateau) -> set to " << bias_values_by_portcard_and_aoh_new[portcard][aoh] << std::endl;
    }else{
      bias_values_by_portcard_and_aoh_new[portcard][aoh] = firstPlateau_forPOH + 5; // safety factor
    }

    ofs << "[result] portCard = " << portcard << ", AOH number = " << aoh << " -> New setting : " << bias_values_by_portcard_and_aoh_new[portcard][aoh] << std::endl;
    ofs << std::endl;
  }



	
  std::cout << "Start writing to the portcard config ..." << std::endl;
  //Write out the configs

  for (std::map<std::string, std::map<unsigned int, unsigned int> >::iterator portCardName_itr = bias_values_by_portcard_and_aoh_new.begin(); 
       portCardName_itr != bias_values_by_portcard_and_aoh_new.end(); portCardName_itr++){

    std::string portCardName = portCardName_itr->first;

    std::map<std::string, PixelPortCardConfig*>::iterator mapNamePortCard_itr = getmapNamePortCard()->find(portCardName);
    assert( mapNamePortCard_itr != getmapNamePortCard()->end());
    PixelPortCardConfig* thisPortCardConfig = mapNamePortCard_itr->second;

    for(std::map<unsigned int, unsigned int >::iterator AOHNumber_itr = portCardName_itr->second.begin(); AOHNumber_itr != portCardName_itr->second.end(); AOHNumber_itr++){
      unsigned int AOHNumber = AOHNumber_itr->first;
      unsigned int AOHBiasAddress = thisPortCardConfig->AOHBiasAddressFromAOHNumber(AOHNumber);
      thisPortCardConfig->setdeviceValues(AOHBiasAddress, bias_values_by_portcard_and_aoh_new[portCardName][AOHNumber]);      
    }
    thisPortCardConfig->writeASCII(outputDir());
    cout << "Wrote the portcard config for port card: " << portCardName << endl;
  }

  // create e-log message
  ofs.close();


  if(writeElog){

    string cmd = "/home/cmspixel/user/local/elog -h elog.physik.uzh.ch -p 8080 -s -v -u cmspixel uzh2014 -n 0 -l Pixel -a Filename=\"[POS e-log] ";
    cmd += runDir();
    cmd += " : POH Bias Scan\" -m ";
    cmd += outtext;

    for(std::map<int,std::map<int, std::map<int, TH1F* > > >::iterator it1 = histos.begin(); it1 != histos.end(); ++it1){
      
      Int_t fednumber = it1->first;
      
      TString cname = "Summary_FED";
      cname += fednumber;
      
      TCanvas *c = new TCanvas(cname, cname);
      summary[fednumber]->Draw("colz");
      
      TString filename; 
      filename.Form("%s/summary_FED%i.gif", outputDir().c_str(), fednumber);
      c->Print(filename);

      cmd += " -f ";
      cmd += filename;


      TString tname = "Turnon_FED";
      tname += fednumber;
      
      TCanvas *c2 = new TCanvas(tname, tname);


      int idraw = 0;
      for(std::map<int, std::map<int, TH1F* > >::iterator it2 = it1->second.begin(); it2 != it1->second.end(); ++it2){

	Int_t ch = it2->first;

	histos[fednumber][ch][2]->SetLineColor(idraw+1);
	
	histos[fednumber][ch][2]->GetYaxis()->SetRangeUser(0, 1.3);

	if(idraw==0) histos[fednumber][ch][2]->Draw();
	else histos[fednumber][ch][2]->Draw("same");	
      
	idraw++;
      }

      TString tfilename;
      tfilename.Form("%s/turnon_FED%i.gif", outputDir().c_str(), fednumber);
      c2->Print(tfilename);

      cmd += " -f ";
      cmd += tfilename;

    }

    std::cout << "---------------------------" << std::endl;
    std::cout << "e-log post:" << cmd << std::endl;
    system(cmd.c_str());
    //    int i = system(cmd.c_str());
    // std::cout << "returnCode = " << i << std::endl;
    std::cout << "---------------------------" << std::endl;
  }


  tempCalibObject->writeASCII(outputDir());

  CloseRootf();  


}

std::vector<std::string> PixelPOHBiasCalibration::calibrated() {
  std::vector<std::string> tmp;
  tmp.push_back("portcard");
  return tmp;
}


void PixelPOHBiasCalibration::CloseRootf() {
  if (rootf) {
    rootf->Write();
    rootf->Close();
    delete rootf;
  }
}


void PixelPOHBiasCalibration::BookEm(){

  root_fn.Form("%s/POHBias.root", outputDir().c_str());
  //  CloseRootf();

  std::cout << "writing ROOT file" << std::endl;

  rootf = new TFile(root_fn, "recreate");
  assert(rootf->IsOpen());

  tree = new TTree("tree", "tree");

  tree->Branch("channel",&b_channel,"channel/I");
  tree->Branch("fednumber",&b_fednumber,"fednumber/I");
  tree->Branch("AOHGain",&b_AOHGain,"AOHGain/I");
  tree->Branch("isStable",&b_isStable,"isStable/I");
  tree->Branch("isFastPlateau",&b_isFastPlateau,"isFastPlateau/I");
  tree->Branch("plateauX",&b_plateauX,"plateauX/I");
  tree->Branch("plateauEff",&b_plateauEff,"plateauEff/F");
  tree->Branch("isPass",&b_isPass,"isPass/I");

  std::cout << "Register tree ... end" << std::endl;

  vectorOfPortcards.clear();  
  PixelCalibConfiguration* tempCalibObject = dynamic_cast<PixelCalibConfiguration*>(theCalibObject_);
  assert(tempCalibObject != 0);

  const std::set<PixelChannel>& channelsToCalibrate = tempCalibObject->channelList();

  for ( std::set<PixelChannel>::const_iterator channelsToCalibrate_itr = channelsToCalibrate.begin(); 
	channelsToCalibrate_itr != channelsToCalibrate.end(); channelsToCalibrate_itr++ ){
	
    const PixelHdwAddress& channelHdwAddress = theNameTranslation_->getHdwAddress(*channelsToCalibrate_itr);
    const unsigned int fednumber = channelHdwAddress.fednumber();
    const unsigned int channel = channelHdwAddress.fedchannel();

    const std::pair< std::string, int > portCardAndAOH = thePortcardMap_->PortCardAndAOH(*channelsToCalibrate_itr);
    const std::string portCardName = portCardAndAOH.first;     
    const std::string tbmname = (*channelsToCalibrate_itr).TBMChannelStringFull();
    
    // This is to escape for A2/B2 channels - YT
    if(tbmname.find("2")!=std::string::npos) continue;
    assert(portCardName!="none");
    
    const int AOHNumber = portCardAndAOH.second;    

    string vec = portCardName;
    vec += "_POH";
    vec += AOHNumber;
      
    std::cout << vec << std::endl;

    vectorOfPortcards.push_back(vec);
    portcard_.push_back(portCardName);
    aohnumber_.push_back(AOHNumber);
    channel_.push_back(channel);
    fednumber_.push_back(fednumber);
      
  }

  
  rootDirs = new PixelRootDirectoryMaker(vectorOfPortcards, gDirectory);

  for(int istr=0; istr < (int)portcard_.size(); istr++){
    string dirname = portcard_[istr];
    dirname += "_POH";
    dirname += aohnumber_[istr];

    std::cout << dirname << std::endl;

    rootDirs->cdDirectory(dirname);

    for(int channel_index = 0; channel_index < 2; channel_index++){
      for (unsigned int AOHGain=AOHGainMin; AOHGain <= AOHGainMax; ++AOHGain){      
	
	Int_t channelstr = channel_[istr] + channel_index;
	
	TString hname = "h_eff_fed";
	hname += fednumber_[istr];
	hname += "_channel";
	hname += channelstr;
	hname += "_Gain";
	hname += AOHGain;
	
	int nbins = ((AOHBiasMax - AOHBiasMin)/AOHBiasStepSize)+1;

	TH1F *h_eff = new TH1F(hname, hname, 
			       nbins, AOHBiasMin, AOHBiasMax+1);

	//	h_eff->SetMinimum(0.);
	//	h_eff->SetMaximum(1.4);
	//	h_eff->GetXaxis()->SetTitle("POH bias");
	//	h_eff->GetYaxis()->SetTitle("efficiency");

	histos[fednumber_[istr]][channelstr][AOHGain] = h_eff;

      }
    }
  }



  for(std::map<int,std::map<int, std::map<int, TH1F* > > >::iterator it1 = histos.begin(); it1 != histos.end(); ++it1){

    int channelcount = 0;    
    for(std::map<int, std::map<int, TH1F* > >::iterator it2 = it1->second.begin(); it2 != it1->second.end(); ++it2){
      channelcount ++;
    }

    Int_t fednumber = it1->first;
    TString hname = "summary_X_FED";
    hname += fednumber;

    TH2F* _summary = new TH2F(hname, hname, 
			      channelcount, 0, channelcount,
			      2,0,2);

    int xcount = 0;    
    for(std::map<int, std::map<int, TH1F* > >::iterator it2 = it1->second.begin(); it2 != it1->second.end(); ++it2){
      xcount++; 
      TString label = "ch";
      label += it2->first;
      _summary->GetXaxis()->SetBinLabel(xcount, label);
    }
    _summary->GetYaxis()->SetBinLabel(1, "Plataeu X");
    _summary->GetYaxis()->SetBinLabel(2, "Plataeu eff.");

    summary[fednumber] = _summary;
  }


  rootf->cd();
}


