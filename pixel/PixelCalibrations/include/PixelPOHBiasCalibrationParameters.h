/*************************************************************************
 * Component of XDAQ Application for Pixel Online Software               *
 * Copyright (C) 2007, Cornell University	                         *
 * All rights reserved.                                                  *
 * Authors: Souvik Das and Anders Ryd                                    *
 *************************************************************************/

#ifndef _PixelPOHBiasCalibrationParameters_h_
#define _PixelPOHBiasCalibrationParameters_h_

namespace PixelPOHBiasCalibrationParameters {

	// Parameter defaults
	const unsigned int k_ScanMin_default = 20;
	const unsigned int k_ScanMax_default = 25;
	const unsigned int k_ScanStepSize_default = 1;
	const unsigned int k_GainMin_default = 1;
	const unsigned int k_GainMax_default = 3;
	const unsigned int k_GainStepSize_default = 1;
	const unsigned int k_nTriggersPerPOHBias_default = 8;
	const float k_AllowPlateau_default = 0.95;
	//	const unsigned int k_TargetBMin_default = 512-100;
	//	const unsigned int k_TargetBMax_default = 512+100;
	//	const int k_SaturationPointOffset_default = 4; // = 5 degrees x (22.5 ADC counts at 2Vpp / degree) / (29 ADC counts at 2Vpp / POH bias count) = 3.9 POH bias counts ~= 4 POH bias counts
	//	const std::string  k_MaxFEDReceiverInputOffset_default = "8";
	//	const std::string  k_SetAnalogInputBias_default  = "200";
	//	const std::string  k_SetAnalogOutputBias_default = "120";
//	const std::string  k_SetAnalogOutputGain_default = "200";
//	const bool k_printFEDRawData_default = false;
//	const bool k_printFEDOffsetAdjustments_default = false;
//	const bool k_printPOHBiasAdjustments_default = false;
	
	// enum for parameters passed to FED supervisors
	//	enum FEDParameters {k_WhatToDo, k_POHBias, k_FEDNumber, k_FEDChannel, k_NumVars};

};

#endif
