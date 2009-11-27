/*
 * Copyright 2007-2008 Deutsches Forschungszentrum fuer Kuenstliche Intelligenz 
 * or its licensors, as applicable.
 * 
 * You may not use this file except under the terms of the accompanying license.
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * 
 * Project: nf-HiShape
 * File: nf-hishape_util.h
 * Purpose: header file containing functions for interacting with the nf-hishape kernel module 
 * Responsible: Matthias Reif
 * Primary Repository: https://svn.iupr.org/projects/nf-HiShape
 * Web Sites: www.iupr.org, www.dfki.de
 */
 
#ifndef TEST_H_
#define TEST_H_

#include <sys/ioctl.h>
#include <inttypes.h>
#include <list>
#include <iomanip> 
#include <algorithm>
#include <fcntl.h> /* für open(..) */
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>

#include "nf-hishape.h"

using namespace std;

//compare two HiShapeRange according to their "uint32_t from"
//the function will later be used to sort a list of HiShapeRange 
bool compare_HiShapeRange(const HiShapeRange &a, const HiShapeRange &b);

//this function checks of overlapping on the elements of the list l, 
//returns true in case of overlapping, otherwise false 
bool check_overlapping(list<HiShapeRange*> l);



//copies HiSHapeRange  
inline void copy(HiShapeRange &target, const HiShapeRange &source);

//this function sorts the list and check for overlapping. If the list 
//doesn't overlapp, its entries are copied to dst and true is returned.
//otherwise nothing is done and false is returned
bool getHiShapeArray(HiShapeRanges* dst, list<HiShapeRange> src);

//this function interacts with the nf-hishape kernel module. The list's entries
//are copied to the module 
int setHiShapeRanges(list<HiShapeRange> ranges);

//this function interacts with the nf-hishape kernel module. The entire module
//contain is returned in a list
list<HiShapeRange>  getHiShapeRanges();

void print_elements(list<HiShapeRange> list1);

////prints the -H option
//void print_help();
//
////executes the user command
//void do_command(int argc,char *argv[]);

typedef list<HiShapeRange>::iterator iter;

#endif /*TEST_H_*/
