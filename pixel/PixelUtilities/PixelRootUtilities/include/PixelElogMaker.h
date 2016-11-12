/*************************************************************************
 * XDAQ Components for Distributed Data Acquisition                      *
 * Copyright (C) 2008 Cornell.			                 *
 * All rights reserved.                                                  *
 * Authors: A. Ryd	                				 *
 *                                                                       *
 * For the licensing terms see LICENSE.		                         *
 * For the list of contributors see CREDITS.   			         *
 *************************************************************************/

#ifndef _PixelElogMaker_h_
#define _PixelElogMaker_h_

#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <vector>

using namespace std;

class PixelElogMaker {

 public:
  
  string titlename;
 
  PixelElogMaker(const std::string name);
  void post(const std::string run, const std::string logoutput, const std::string plotcommand);

};

#endif
