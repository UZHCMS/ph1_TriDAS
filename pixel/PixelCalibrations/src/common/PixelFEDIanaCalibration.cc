#include "CalibFormats/SiPixelObjects/interface/PixelCalibConfiguration.h"
#include "CalibFormats/SiPixelObjects/interface/PixelDACNames.h"
#include "PixelCalibrations/include/PixelFEDIanaCalibration.h"
#include "PixelConfigDBInterface/include/PixelConfigInterface.h"
#include "PixelUtilities/PixelFEDDataTools/include/PixelFEDDataTypes.h"
#include "PixelUtilities/PixelFEDDataTools/include/ErrorFIFODecoder.h"
#include "PixelUtilities/PixelFEDDataTools/include/ColRowAddrDecoder.h"
#include "PixelUtilities/PixelFEDDataTools/include/DigScopeDecoder.h"
#include "PixelUtilities/PixelFEDDataTools/include/DigTransDecoder.h"
#include "PixelUtilities/PixelFEDDataTools/include/FIFO3Decoder.h"
#include "PixelUtilities/PixelRootUtilities/include/PixelRootDirectoryMaker.h"
#include "PixelUtilities/PixelFEDDataTools/include/DigFIFO1Decoder.h"
#include "PixelCalibrations/include/PixelIanaAnalysis.h"


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
PixelFEDIanaCalibration::PixelFEDIanaCalibration(const PixelFEDSupervisorConfiguration & tempConfiguration, SOAPCommander* mySOAPCmdr)
  : PixelFEDCalibrationBase(tempConfiguration,*mySOAPCmdr), rootf(0)
{
  std::cout << "In PixelFEDIanaCalibration copy ctor()" << std::endl;
}

///////////////////////////////////////////////////////////////////////////////////////////////
void PixelFEDIanaCalibration::initializeFED() {
  setFEDModeAndControlRegister(0x8, 0x30010);
  //setFEDModeAndControlRegister(0x8, 0x00014);
  printIfSlinkHeaderMessedup_off();
  sendResets();
  //setFEDModeAndControlRegister(0x8, 0x10015);
  
}

///////////////////////////////////////////////////////////////////////////////////////////////
xoap::MessageReference PixelFEDIanaCalibration::beginCalibration(xoap::MessageReference msg) {
  std::cout << "In PixelFEDIanaCalibration::beginCalibration()" << std::endl;

  PixelCalibConfiguration* tempCalibObject = dynamic_cast<PixelCalibConfiguration*>(theCalibObject_);
  assert(tempCalibObject != 0);

  tempCalibObject->writeASCII(outputDir());
  writeElog = tempCalibObject->parameterValue("writeElog") == "yes";

  setFIFO1Mode();//jen

  vector<PixelModuleName>::const_iterator module_name = theDetectorConfiguration_->getModuleList().begin();
  for (;module_name!=theDetectorConfiguration_->getModuleList().end();++module_name){
    // First we need to get the DAC settings for the ROCs on this module.
    PixelDACSettings *dacs=0; 
    string modulePath=module_name->modulename();
    PixelConfigInterface::get(dacs, "pixel/dac/"+modulePath, *theGlobalKey_);
    assert(dacs!=0);
    dacsettings_[*module_name]=dacs;
  }

  for (unsigned dacnum = 0; dacnum < tempCalibObject->numberOfScanVariables(); ++dacnum) {
    const std::string& dacname = tempCalibObject->scanName(dacnum);
    std::vector<unsigned int> dacvals = tempCalibObject->scanValues(dacname);
    if (dacvals.size() > 1)
      dacsToScan.push_back(dacname);

    for( unsigned int i = 0; i < dacvals.size(); ++i ) std::cout << " dac value " << i << " is " << dacvals[i] << std::endl;
  }


  outtext.Form("%s/log.txt", outputDir().c_str()); 
  BookEm("");

  xoap::MessageReference reply = MakeSOAPMessageReference("BeginCalibrationDone");
  return reply;
}

///////////////////////////////////////////////////////////////////////////////////////////////
xoap::MessageReference PixelFEDIanaCalibration::execute(xoap::MessageReference msg) {
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
    cout << "ERROR: PixelFEDIanaCalibration::execute() does not understand the WhatToDo command, "<< parameters[0].value_ <<", sent to it.\n";
    assert(0);
  }

  xoap::MessageReference reply = MakeSOAPMessageReference("FEDCalibrationsDone");
  return reply;
}

///////////////////////////////////////////////////////////////////////////////////////////////
xoap::MessageReference PixelFEDIanaCalibration::endCalibration(xoap::MessageReference msg) {

  std::cout << "In PixelFEDIanaCalibration::endCalibration()" << std::endl;
  xoap::MessageReference reply = MakeSOAPMessageReference("EndCalibrationDone");
  return reply;
}

///////////////////////////////////////////////////////////////////////////////////////////////
void PixelFEDIanaCalibration::RetrieveData(unsigned state) {
  PixelCalibConfiguration* tempCalibObject = dynamic_cast<PixelCalibConfiguration*>(theCalibObject_);
  assert(tempCalibObject != 0);
 
  //lastDACValue = 0;

  std::map<std::string, unsigned int> currentDACValues;
  for (unsigned dacnum = 0; dacnum < tempCalibObject->numberOfScanVariables(); ++dacnum) {
    const std::string& dacname = tempCalibObject->scanName(dacnum);
    const unsigned dacvalue = tempCalibObject->scanValue(tempCalibObject->scanName(dacnum), state);
    currentDACValues[dacname] = dacvalue;
  }
  if(tempCalibObject->mode()=="Iana" && currentDACValues["Vana"] != lastDACValue){
   event_ = 0;
   lastDACValue = currentDACValues["Vana"];
   last_dacs.clear();
  }
  if(tempCalibObject->mode()=="Vdig" && currentDACValues["Vdd"] != lastDACValue){
   event_ = 0;
   lastDACValue = currentDACValues["Vdd"];
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
           
     if (statusFifo1[ch] > 0){ 

      for( int i = 0; i < statusFifo1[ch]; ++i ){
       uint32_t w = bufferFifo1[ch][i];
       int decCh = (w >> 26) & 0x3f;       
       if( decCh != channel ) continue;       
       uint32_t mk = (w >> 21) & 0x1f;
       uint32_t az = (w >> 8) & 0x1fff;
       uint32_t f8 = w & 0xff;
       if (az == 0 && mk != 0x1e ){
        if( mk <= 8 ){
         last_dacs[fedsAndChannels[ifed].first][channel][mk].push_back(f8);
        }
       }// close reading dac from roc header
      }// close fifo1 reading  
     }// close if status > 0
     
    }// close loop on channels

  }// close loop on feds

  if( event_ == tempCalibObject->nTriggersPerPattern()-1 ){

     if( tempCalibObject->mode()=="Iana" ){
     ReadIana();
     }
     else if( tempCalibObject->mode()=="Vdig" ) ReadVdig();
  }

  event_++;
  sendResets();
    
}

///////////////////////////////////////////////////////////////////////////////////////////////
void PixelFEDIanaCalibration::Analyze() {
  PixelCalibConfiguration* tempCalibObject = dynamic_cast<PixelCalibConfiguration*>(theCalibObject_);
  assert(tempCalibObject != 0);  
  std::string mode = tempCalibObject->mode();

  if( tempCalibObject->mode()=="Iana" ) IanaAnalysis(tempCalibObject);
  else if( tempCalibObject->mode()=="Vdig" ) VdigAnalysis(tempCalibObject);

}

///////////////////////////////////////////////////////////////////////////////////////////////
void PixelFEDIanaCalibration::IanaAnalysis(PixelCalibConfiguration* tmpCalib) {

  ofstream out((outputDir()+"/iana.dat").c_str()); //leave the file method intact for now
  assert(out.good()); //file method

  std::ofstream ofs(outtext);
  ofs << std::endl;
  
  branch theBranch;
  branch_sum theBranch_sum;
  TDirectory* dirSummaries = gDirectory->mkdir("SummaryTrees","SummaryTrees");
  dirSummaries->cd();

  string cmd = "/home/cmspixel/user/local/elog -h elog.physik.uzh.ch -p 8080 -s -v -u cmspixel uzh2014 -n 0 -l Pixel -a Filename=\"[POS e-log] ";
  cmd += runDir();
  cmd += " : Iana Scan\" -m ";
  cmd += outtext;

  TTree* tree = new TTree("PassState","PassState");
  TTree* tree_sum =new TTree("SummaryInfo","SummaryInfo");
  
  tree->Branch("PassState",&theBranch,"pass/F:rocName/C",4096000);
  //tree_sum->Branch("SummaryInfo",&theBranch_sum,"deltaVana/F:newVana/F:newIana/F:maxIana/F:fitChisquare/F:rocName/C",4096000);
  tree_sum->Branch("SummaryInfo",&theBranch_sum,"deltaVana/F:newVana/F:newIana/F:fitChisquare/F:rocName/C",4096000);

  rootf->cd();

  PixelRootDirectoryMaker rootDirs(tmpCalib->rocList(),gDirectory);

  for( std::map<int,std::map<int,std::map<int,std::map<int,int> > > >::iterator it = values.begin(); it!=values.end(); ++it ){
   
   for( std::map<int,std::map<int,std::map<int,int> > >::iterator it2 = (it->second).begin(); it2!=(it->second).end(); ++it2 ){

    const std::vector<PixelROCName>& rocs = theNameTranslation_->getROCsFromFEDChannel(it->first, it2->first);
    PixelChannel theChannel = theNameTranslation_->ChannelFromFEDChannel(it->first, it2->first);

    static std::string directory;
    directory=getenv("PIXELCONFIGURATIONBASE");
    std::ifstream in(directory+"iana/0/ROC_Iana_module_"+theChannel.modulename()+".dat");
   
    for( std::map<int,std::map<int,int> >::iterator it3 = (it2->second).begin(); it3!=(it2->second).end(); ++it3 ){
    
       if( theNameTranslation_->ROCNameFromFEDChannelROCExists(it->first,it2->first,it3->first-1) ){
       
        PixelROCName theROC = theNameTranslation_->ROCNameFromFEDChannelROC(it->first,it2->first,it3->first-1);
        PixelModuleName theModule(theROC.rocname());
        
        std::string tmp;
        std::string tag;
        float par0ia, par1ia, par2ia;
        in >> tag;//ROC
        in >> tmp;
        //std::cout << " ROC: " << tmp;
        in >> tag;//par0vd
        in >> tmp;
        in >> tag;//par1vd
        in >> tmp;
        in >> tag;//par0va
        in >> tmp;
        in >> tag;//par1va
        in >> tmp; 
        in >> tag;//par0rbia
        in >> tmp;
        in >> tag;//par1rbia
        in >> tmp;
        in >> tag;//par0tbia
        in >> tmp;
        in >> tag;//par1tbia
        in >> tmp;       
        in >> tag;//par2tbia
        in >> tmp;
        in >> tag;//par0ia
        in >> tmp; par0ia = stof(tmp);
        in >> tag;//par1ia
        in >> tmp; par1ia = stof(tmp);
        in >> tag;//par2ia
        in >> tmp; par2ia = stof(tmp);
        //std::cout << " par0ia " << par0ia << " par1ia " << par1ia << " par2ia " << par2ia;

        theBranch.pass = 0;
        strcpy(theBranch.rocName, theROC.rocname().c_str());
        strcpy(theBranch_sum.rocName, theROC.rocname().c_str());
 
        TF1* froc = new TF1("froc", "[0] + [1]*x + [2]*x*x",0,200);
        froc->SetParameters(par0ia,par1ia,par2ia);
       
        int npoints = it3->second.size();
        std::vector<double> x(npoints+1), y(npoints+1), ey(npoints+1);

        int j = 0;
        for( std::map<int,int>::iterator it4 = (it3->second).begin(); it4!=(it3->second).end(); ++it4 ){

         if( rocids[it->first][it2->first][it3->first][it4->first] == rocs[it3->first-1].roc() && 
             rbreg[it->first][it2->first][it3->first][it4->first] == 12  ){
          y[j] = froc->GetX(it4->second);//(par2ia*it4->second*it4->second+par1ia*it4->second+par0ia)/100.;//0.25*it4->second;
	  x[j] = it4->first;
	  ey[j] = 1.;
	  j++;
          //std::cout << " ROC#" << rocs[it3->first-1].roc() << " par0 " << par0ia << " par1 " << par1ia << " par2 " << par2ia;
          //std::cout << " vana " << it4->first << std::endl;
          //std::cout << " iana(ADC) " << it4->second << " iana(mA) " << froc->GetX(it4->second) << std::endl;
         }

        }//close loop on vana values
        npoints = j;

        const int oldVana = dacsettings_[theModule]->getDACSettings(theROC)->getVana();

        rootDirs.cdDirectory(theROC);
        PixelIanaAnalysis analysis(true);
	
	string filename = outputDir().c_str();
	filename += "/";
	filename += theROC.rocname();
	filename += ".gif";

        analysis.goIana(theROC.rocname(),oldVana,npoints,x, y, ey,out, filename.c_str());

	if(npoints!=0){
	  cmd += " -f ";
	  cmd += filename;
	}else{
	  ofs << "[warning] no points available ... skip to upload graphs to e-log" << std::endl;
	}

        //theBranch_sum.maxIana = analysis.maxIana;
        theBranch_sum.fitChisquare = analysis.fitChisquare;
        theBranch_sum.newVana = analysis.newVana;
        theBranch_sum.deltaVana = analysis.newVana - oldVana;
        theBranch_sum.newIana = analysis.newIana;
        theBranch.pass = analysis.pass;

	ofs << "[result] ROC = " << theROC.rocname() << ", fit chi2 = " << analysis.fitChisquare << ", old Vana = " << analysis.oldVana << ", new Vana = " << analysis.newVana << ", delta Vana = " << analysis.newVana - oldVana << ", new Iana = " << analysis.newIana << ", Pass = " << analysis.pass << std::endl;

        tree->Fill();
        tree_sum->Fill();

        unsigned int newdac = analysis.newVana;
        dacsettings_[theModule]->getDACSettings(theROC)->setVana(newdac);
              
       }//close if roc exists
				      
    }//close loop on rocs
 
    in.close();  

   }//close for2   
  }//close for 1    


  if(writeElog){
    std::cout << "---------------------------" << std::endl;
    std::cout << "e-log post:" << cmd << std::endl;
    system(cmd.c_str());
    std::cout << "---------------------------" << std::endl;
  }

  for (std::map<PixelModuleName,PixelDACSettings*>::const_iterator idacs = dacsettings_.begin(); idacs != dacsettings_.end(); ++idacs)
    idacs->second->writeASCII(outputDir());
  
  CloseRootf();

}

///////////////////////////////////////////////////////////////////////////////////////////////
void PixelFEDIanaCalibration::VdigAnalysis(PixelCalibConfiguration* tmpCalib) {  

  std::ofstream ofs(outtext);
  ofs << std::endl;
  
  string cmd = "/home/cmspixel/user/local/elog -h elog.physik.uzh.ch -p 8080 -s -v -u cmspixel uzh2014 -n 0 -l Pixel -a Filename=\"[POS e-log] ";
  cmd += runDir();
  cmd += " : Vdigi Scan\" -m ";
  cmd += outtext;


  branch_sum_vdig theBranch;
  TDirectory* dirSummaries = gDirectory->mkdir("SummaryTrees","SummaryTrees");
  dirSummaries->cd();

  TTree* tree = new TTree("Summary","Summary");  
  tree->Branch("Summary",&theBranch,"Mean/F:RMS/F:rocName/C",4096000);

  rootf->cd();

  PixelRootDirectoryMaker rootDirs(tmpCalib->rocList(),gDirectory);

  for( std::map<int,std::map<int,std::map<int,std::map<int,int> > > >::iterator it = values.begin(); it!=values.end(); ++it ){
   
   for( std::map<int,std::map<int,std::map<int,int> > >::iterator it2 = (it->second).begin(); it2!=(it->second).end(); ++it2 ){

    const std::vector<PixelROCName>& rocs = theNameTranslation_->getROCsFromFEDChannel(it->first, it2->first);
    PixelChannel theChannel = theNameTranslation_->ChannelFromFEDChannel(it->first, it2->first);

    static std::string directory;
    directory=getenv("PIXELCONFIGURATIONBASE");
    std::ifstream in(directory+"iana/0/ROC_Iana_module_"+theChannel.modulename()+".dat");
   
    for( std::map<int,std::map<int,int> >::iterator it3 = (it2->second).begin(); it3!=(it2->second).end(); ++it3 ){
 
     if( theNameTranslation_->ROCNameFromFEDChannelROCExists(it->first,it2->first,it3->first-1) ){

        PixelROCName theROC = theNameTranslation_->ROCNameFromFEDChannelROC(it->first,it2->first,it3->first-1);
        PixelModuleName theModule(theROC.rocname());
 
        std::string tmp;
        std::string tag;
        float par0vd, par1vd;
        in >> tag;//ROC
        in >> tmp;
        //std::cout << " ROC: " << tmp;
        in >> tag;//par0vd
        in >> tmp; par0vd = stof(tmp);
        in >> tag;//par1vd
        in >> tmp; par1vd = stof(tmp);
        in >> tag;//par0va
        in >> tmp;
        in >> tag;//par1va
        in >> tmp; 
        in >> tag;//par0rbia
        in >> tmp;
        in >> tag;//par1rbia
        in >> tmp;
        in >> tag;//par0tbia
        in >> tmp;
        in >> tag;//par1tbia
        in >> tmp;       
        in >> tag;//par2tbia
        in >> tmp;
        in >> tag;//par0ia
        in >> tmp;
        in >> tag;//par1ia
        in >> tmp;
        in >> tag;//par2ia
        in >> tmp;
        //std::cout << " par0vd " << par0vd << " par1vd " << par1vd << std::endl;  

        strcpy(theBranch.rocName, theROC.rocname().c_str());
        rootDirs.cdDirectory(theROC);
        TH1F* hVdig = new TH1F(theROC.rocname().c_str(), theROC.rocname().c_str(), 1000, 0, 10);
        TF1* froc = new TF1("froc", "[0] + [1]*x",0,200);
        froc->SetParameters(par0vd,par1vd);
        for( std::map<int,int>::iterator it4 = (it3->second).begin(); it4!=(it3->second).end(); ++it4 ){

         if( rocids[it->first][it2->first][it3->first][it4->first] == rocs[it3->first-1].roc() &&
             rbreg[it->first][it2->first][it3->first][it4->first] == 8 ){

            hVdig->Fill( froc->GetX(it4->second) );

         }

        }//close loop on vdig values

        delete froc;

	TString cname = "Summary_";
	cname += theROC.rocname();

	TCanvas *can = new TCanvas(cname, cname);

        hVdig->Draw("HIST");
        hVdig->Write();

        theBranch.Mean = hVdig->GetMean();
        theBranch.RMS = hVdig->GetRMS();

	ofs << "[result] Mean = " << hVdig->GetMean() << ", RMS = " << hVdig->GetRMS() << " for " << theROC.rocname().c_str() << std::endl;

	TString filename = outputDir().c_str();
	filename += "/";
	filename += theROC.rocname().c_str();
	filename += ".gif";
	
	can->Print(filename);

	cmd += " -f ";
	cmd += filename;

        tree->Fill();

     }            
            				      
    }//close loop on rocs per fed channel
    in.close();  
   }//close loop on fed channels  
  }//close loop on feds  

  if(writeElog){
    std::cout << "---------------------------" << std::endl;
    std::cout << "e-log post:" << cmd << std::endl;
    system(cmd.c_str());
    std::cout << "---------------------------" << std::endl;
  }


  CloseRootf();

}

///////////////////////////////////////////////////////////////////////////////////////////////
void PixelFEDIanaCalibration::CloseRootf() {
  if (rootf) {
    rootf->Write();
    rootf->Close();
    delete rootf;
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////
void PixelFEDIanaCalibration::BookEm(const TString& path) {

  values.clear();

  PixelCalibConfiguration* tempCalibObject = dynamic_cast<PixelCalibConfiguration*>(theCalibObject_);
  assert(tempCalibObject != 0);  
  
  TString root_fn;
  if( tempCalibObject->mode()=="Iana" )
   root_fn.Form("%s/Iana.root", outputDir().c_str());
  else if( tempCalibObject->mode()=="Vdig" )
   root_fn.Form("%s/Vd.root", outputDir().c_str());
  cout << "writing histograms to file " << root_fn << endl;
  CloseRootf();
  rootf = new TFile(root_fn, "create");
  assert(rootf->IsOpen());
  rootf->cd(0);

}
  
///////////////////////////////////////////////////////////////////////////////////////////////
void PixelFEDIanaCalibration::ReadIana( void ) {
  
  //std::cout << " **************** VANA = " << lastDACValue << std::endl;
  for( std::map<int,std::map<int,std::map<int,std::vector<int> > > >::iterator it = last_dacs.begin(); it!=last_dacs.end(); ++it ){
   
   for( std::map<int,std::map<int,std::vector<int> > >::iterator it2 = (it->second).begin(); it2!=(it->second).end(); ++it2 ){
   
    for( std::map<int,std::vector<int> >::iterator it3 = (it2->second).begin(); it3!=(it2->second).end(); ++it3 ){

      int start = -1;
      int stop = -1;
      int rbvalue = -1;
      int rocid = -1;
      int rbregvalue = -1;
      std::vector<int> bits;
       
      for( unsigned int i=0; i<(it3->second).size(); ++i ){

      //std::cout << " - channel#" << it2->first << " ROC#" << it3->first << " - dac " << i << ":" << (it3->second)[i] << std::endl;
       if( (((it3->second)[i])&(0x2)) == 2 && start == -1 ){start = i;}    
       else if( (((it3->second)[i])&(0x2)) == 2 && start != -1 ){stop = i;}         
      }
      
      //std::cout << "    channel " << it2->first << ": (found start =  " << start << " and stop = " << stop << " ) " << std::endl;     
      if( start != stop && (stop-start)==16){
   
       //now put together the 16 bits
       for( int i = start+1; i <= stop; ++i ){ bits.push_back((it3->second)[i]&0x1);}
    
       //now get Iana
       //for( unsigned int b=0; b<bits.size(); ++b ){ std::cout << bits[b];}
       //std::cout << std::endl;
       rbvalue = 0;
       for( unsigned int b=8; b<bits.size(); ++b ) rbvalue+=(bits[b]<<(15-b)); //std::cout << bits[b];
       rocid=0;
       for( unsigned int b=0; b<4; ++b ) rocid+=(bits[b]<<(3-b));      
       rbregvalue=0;
       for( unsigned int b=4; b<8; ++b ) rbregvalue+=(bits[b]<<(7-b));

      }
   
      //std::cout << std::endl;
      //std::cout << " **************** VANA = " << lastDACValue << " FOUND IANA: " << rbvalue << std::endl;
      //std::cout << " **************** VANA = " << lastDACValue << " rbreg " << rb << std::endl;
      //std::cout << " **************** Vdig = " << lastDACValue << " FOUND Vd: " << rbvalue;
      //std::cout << " rocid " << rocid << " rbregvalue " << rbregvalue << std::endl;
       
      values[it->first][it2->first][it3->first][lastDACValue] = rbvalue;            
      rocids[it->first][it2->first][it3->first][lastDACValue] = rocid;            
      rbreg[it->first][it2->first][it3->first][lastDACValue] = rbregvalue;            
      
    }      
   }      
  }//close loop

}

///////////////////////////////////////////////////////////////////////////////////////////////
void PixelFEDIanaCalibration::ReadVdig( void ) {
  
  //std::cout << " **************** VANA = " << lastDACValue << std::endl;
  //std::cout << " **************** JENDEBUG::ReadIana Vdd = " << lastDACValue << std::endl;
  for( std::map<int,std::map<int,std::map<int,std::vector<int> > > >::iterator it = last_dacs.begin(); it!=last_dacs.end(); ++it ){
   
   for( std::map<int,std::map<int,std::vector<int> > >::iterator it2 = (it->second).begin(); it2!=(it->second).end(); ++it2 ){
   
    for( std::map<int,std::vector<int> >::iterator it3 = (it2->second).begin(); it3!=(it2->second).end(); ++it3 ){

      std::vector<int> start; start.push_back(-1);
      std::vector<int> stop; stop.push_back(-1);

      int ii=0; 
      for( unsigned int i=0; i<(it3->second).size(); ++i ){

       if( (((it3->second)[i])&(0x2)) == 2 && start[ii] == -1 ){
         start[ii]=i;
       }    
       else if( (((it3->second)[i])&(0x2)) == 2 && start[ii] != -1 ){
        stop[ii] = i;
        start.push_back(i);
        stop.push_back(i);
        ii++;
       }         
      }

      start.pop_back(); stop.pop_back();      
      //std::cout << " - channel#" << it2->first << " ROC#" << it3->first << std::endl;
      //std::cout << "*************** start size " << start.size() << " stop size " << stop.size() << std::endl;
      for( unsigned int i = 0; i<start.size(); ++i){

        //std::cout << "    (found start =  " << start[i] << " and stop = " << stop[i] << " ) " << std::endl;
        int rbvalue = -1;
        int rocid = -1;
        int rbregvalue = -1;
        std::vector<int> bits;

        if( start[i] != stop[i] && (stop[i]-start[i])>13){
   
         //now put together the 16 bits
         for( int b = start[i]+1; b <= stop[i]; ++b ){ bits.push_back((it3->second)[b]&0x1);}
    
         //now get Iana
         //for( unsigned int b=0; b<bits.size(); ++b ){ std::cout << bits[b];}
         //std::cout << std::endl;
         rbvalue = 0;
         for( unsigned int b=8; b<bits.size(); ++b ) rbvalue+=(bits[b]<<(15-b)); //std::cout << bits[b];
         rocid=0;
         for( unsigned int b=0; b<4; ++b ) rocid+=(bits[b]<<(3-b));      
         rbregvalue=0;
         for( unsigned int b=4; b<8; ++b ) rbregvalue+=(bits[b]<<(7-b));

        }
   
        //std::cout << " **************** Vdig = " << lastDACValue << " FOUND Vd: " << rbvalue;
        //std::cout << " rocid " << rocid << " rbregvalue " << rbregvalue << std::endl;
       
        values[it->first][it2->first][it3->first][i] = rbvalue;            
        rocids[it->first][it2->first][it3->first][i] = rocid;            
        rbreg[it->first][it2->first][it3->first][i] = rbregvalue; 
        
      }//close loop on starts and stops          
      
    }      
   }      
  }//close loop

}
