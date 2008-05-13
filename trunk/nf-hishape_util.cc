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
 * File: nf-hishape_util.c
 * Purpose: library of the userland tool for the nf-hishape kernel module
 * Responsible: Matthias Reif
 * Primary Repository: https://svn.iupr.org/projects/nf-HiShape
 * Web Sites: www.iupr.org, www.dfki.de
 */


#include <iostream>
#include <stdio.h>

#include "nf-hishape_util.h"

using namespace std;

bool compare_HiShapeRange(const HiShapeRange &a,const HiShapeRange &b) {
    return a.from < b.from;
}


bool check_overlapping(list<HiShapeRange> l) {
    bool overlap = false;
    HiShapeRange tmp;
    
    l.sort(compare_HiShapeRange);
    
    iter i = l.begin();
    while(i!=l.end()){
        if((*i).to <= (*i).from){
            overlap = true;
            break;
        }
        
        tmp = *i;
        
        if(++i==l.end()){
            break;
        }
        
        if((*i).from <=tmp.to ){
            overlap = true;
            break;
        }
    }
    return overlap;
}


inline void copy(HiShapeRange &dst, const HiShapeRange &source) {
    dst.from = source.from;
    dst.to = source.to;
    dst.limit = source.limit;
    return;
}

bool getHiShapeArray(HiShapeRanges *dst, list<HiShapeRange> src) {
    if(check_overlapping(src)){ // does the list overlap? list ist sorted before beeing checked 
        cerr << "the source list overlaps \n";
        return false;
    }
    dst->length = src.size();
    uint32_t n =  dst->length; 
    dst->ranges = (HiShapeRange*) malloc(n*sizeof(HiShapeRange));
    
    unsigned int i = 0;
    iter it = src.begin();
    
    do{
        copy(dst->ranges[i],(*it++));
    }while(++i < n);
    return true;
}


int setHiShapeRanges(list<HiShapeRange> ranges) {
    HiShapeRanges dst;
    if(getHiShapeArray(&dst,ranges)){
        int h = open(DEVICE_FILE_NAME, 0);
        if(h < 0) {
            perror("could not open device");
            return -1;
        }
        if ((errno = ioctl(h, IOCTL_HISHAPE_SET_RANGES, &dst)) < 0){
            perror("IOCTL_HISHAPE_SET_RANGES failed");
            close(h);
            return -1;
        }
        close(h);
        return 1;
    }
    cout << "IOCTL_HISHAPE_SET_RANGES failed, the list may be overlaping\n";
  
    return -1;
}

list<HiShapeRange> getHiShapeRanges() {
    list<HiShapeRange> list;
    int h = open(DEVICE_FILE_NAME, 0);
    if(h < 0) {
        //perror("could not open device,the list returned is empty");
        cerr << "could not open device,the list returned is empty\n";
        return list;
    }
    
    HiShapeRanges dst;
    dst.ranges = (HiShapeRange*)malloc(65535*sizeof(HiShapeRange));
    dst.length = 65535; 
    if ((errno = ioctl(h, IOCTL_HISHAPE_GET_RANGES, &dst)) < 0){
        //perror("IOCTL_HISHAPE_SET_RANGES failed, the returned list is empty");
        perror("IOCTL_HISHAPE_SET_RANGES failed, the returned list is empty");
    } else {
        uint32_t i = 0;
        for(i=0; i<dst.length; i++) {
            list.push_back(dst.ranges[i]);
        }
    }
    close(h);
    return list;
}


//prints the list
void print_elements(list<HiShapeRange> list1) {
    cout << " " << setw(15) << left << "from" << " | " << setw(15) << left << "to" << " | " << left << "limit" << endl;
    cout << "----------------------------------------------\n";
    char from[16];
    char to[16];
    struct in_addr ipFrom, ipTo;
    for(iter it = list1.begin(); it != list1.end(); it++) {
        ipFrom.s_addr = htonl((*it).from);
        ipTo.s_addr = htonl((*it).to);
        strcpy(from, inet_ntoa(ipFrom));
        strcpy(to, inet_ntoa(ipTo));
//        cout << setw(15) << left << (*it).from << " | " << setw(15) << left << (*it).to << " | " << setw(15) << left << (*it).limit <<endl;
        cout << " " << setw(15) << left << from << " | " << setw(15) << left << to << " | " << left << (*it).limit << "kb/s" <<endl;
    }
    return;
}
