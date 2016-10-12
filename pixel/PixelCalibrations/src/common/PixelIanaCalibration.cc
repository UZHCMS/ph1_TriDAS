#include "CalibFormats/SiPixelObjects/interface/PixelCalibConfiguration.h"
#include "CalibFormats/SiPixelObjects/interface/PixelDACNames.h"
#include "PixelCalibrations/include/PixelIanaCalibration.h"
#include "PixelConfigDBInterface/include/PixelConfigInterface.h"
//#include <toolbox/convertstring.h>

using namespace pos;
using namespace std;

PixelIanaCalibration::PixelIanaCalibration(const PixelSupervisorConfiguration & tempConfiguration, SOAPCommander* mySOAPCmdr)
  : PixelCalibrationBase(tempConfiguration, *mySOAPCmdr)
{
  std::cout << "Greetings from the PixelIanaCalibration copy constructor." << std::endl;
}

void PixelIanaCalibration::beginCalibration() {
  PixelCalibConfiguration* tempCalibObject = dynamic_cast<PixelCalibConfiguration*>(theCalibObject_);
  assert(tempCalibObject != 0);
  
}

bool PixelIanaCalibration::execute() {
  PixelCalibConfiguration* tempCalibObject = dynamic_cast<PixelCalibConfiguration*>(theCalibObject_);
  assert(tempCalibObject != 0);

  const vector<PixelROCName>& rocs=tempCalibObject->rocList();
  const bool firstOfPattern = event_ % tempCalibObject->nTriggersPerPattern() == 0;
  const unsigned state = event_/(tempCalibObject->nTriggersPerPattern());
  reportProgress(0.05);

  // Configure all TBMs and ROCs according to the PixelCalibConfiguration settings, but only when it's time for a new configuration.
  if (firstOfPattern) commandToAllFECCrates("CalibRunning");

  if( tempCalibObject->mode()=="Iana" && firstOfPattern ){
   for (unsigned int i=0;i<rocs.size();i++) setDAC(rocs[i], pos::k_DACAddress_Readback, 12);
  }

  if( tempCalibObject->mode()=="Vdig" && (event_ % 16 == 0) ){
   for (unsigned int i=0;i<rocs.size();i++) setDAC(rocs[i], pos::k_DACAddress_Readback, 8);
  }

  sendTTCCalSync();
  usleep(1000);

  // Read out data from each FED.
  Attribute_Vector parametersToFED(2);
  parametersToFED[0].name_ = "WhatToDo"; parametersToFED[0].value_ = "RetrieveData";
  parametersToFED[1].name_ = "StateNum"; parametersToFED[1].value_ = itoa(state);
  commandToAllFEDCrates("FEDCalibrations", parametersToFED);
    
  return event_ + 1 < tempCalibObject->nTriggersTotal();

}

void PixelIanaCalibration::endCalibration() {
  PixelCalibConfiguration* tempCalibObject = dynamic_cast<PixelCalibConfiguration*>(theCalibObject_);
  assert(tempCalibObject != 0);
  assert(event_ == tempCalibObject->nTriggersTotal());
	
  Attribute_Vector parametersToFED(2);
  parametersToFED[0].name_ = "WhatToDo"; parametersToFED[0].value_ = "Analyze";
  parametersToFED[1].name_ = "StateNum"; parametersToFED[1].value_ = "0";
  commandToAllFEDCrates("FEDCalibrations", parametersToFED);

}

std::vector<std::string> PixelIanaCalibration::calibrated() {
  std::vector<std::string> tmp;
  tmp.push_back("dac");
  return tmp;
}
