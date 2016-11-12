/*************************************************************************
 * XDAQ Components for Distributed Data Acquisition                      *
 * Copyright (C) 2008 Cornell.			                 *
 * All rights reserved.                                                  *
 * Authors: A. Ryd	                				 *
 *                                                                       *
 * For the licensing terms see LICENSE.		                         *
 * For the list of contributors see CREDITS.   			         *
 *************************************************************************/


#include "PixelUtilities/PixelRootUtilities/include/PixelElogMaker.h"
//#include <sstream>


PixelElogMaker::PixelElogMaker(const std::string name){
  titlename = name;
}


void PixelElogMaker::post(const std::string run, const std::string logoutput, const std::string plotcommand){
  
  string cmd = "/home/cmspixel/user/local/elog -h elog.physik.uzh.ch -p 8080 -s -u cmspixel uzh2014 -n 0 -l PixelPOS -a Filename=\"";
  cmd += run;
  cmd += " : ";
  cmd += titlename;
  cmd += "\" -m ";
  cmd += logoutput;
  cmd += plotcommand;

  //  cout << "---------------------------" << endl;
  //  cout << "e-log post:" << cmd << endl;
  system(cmd.c_str());
  //  std::cout << "---------------------------" << std::endl;


}
