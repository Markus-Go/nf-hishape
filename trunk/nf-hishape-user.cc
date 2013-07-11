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
 * File: nf-hishape-user.c
 * Purpose: userland tool for the nf-HiShape kernel module
 * Responsible: Matthias Reif
 * Primary Repository: https://svn.iupr.org/projects/nf-HiShape
 * Web Sites: www.iupr.org, www.dfki.de
 */


#include <getopt.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string>

#include "nf-hishape_util.h"

void printUsage(int argc, char *argv[]);
long getLong(char* optarg);
void test(char* optarg);

int main(int argc,char *argv[]){
    if(argc==1){
        printUsage(argc, argv);
        return EXIT_FAILURE;
    }
    
    static struct option long_options[] = {
        {"list", no_argument, 0, 'L'},
        {"flush", no_argument, 0, 'F'},
        {"set", required_argument, 0, 'S'},
        {"interface", required_argument, 0, 'i'},
        {"any_device", no_argument, 0, 'a'},
        {"print_interface", no_argument, 0, 'p'},
        {"limit", required_argument, 0, 'l'},
        {"from", required_argument, 0, 'f'},
        {"to", required_argument, 0, 't'},
        {"help", no_argument, 0, 'h'},
        {"test", required_argument, 0, 'x'},
        {0, 0, 0, 0}
    };
   
    char device[MAX_DEVICE_LENGTH];
    int c = 0;
    list<HiShapeRange> list_from_file;
    HiShapeRange hs;
    int n;
    int handle;
    int option_index = 0;
    struct in_addr from, to;
    int limit;
    bool fromSet = false;
    bool toSet = false;
    bool limitSet = false;
    while ((c = getopt_long(argc, argv, "LFiahpS:l:f:t:x:",long_options, &option_index)) != -1){
        switch(c){
            // -- list the ranges --
            case 'L':
                print_elements(getHiShapeRanges());
                break;
            // -- flush the ranges --
            case 'F':
                setHiShapeRanges(list_from_file);
                break;
            // -- set the ranges --
            case 'S':
                FILE *fp;
                if((fp=fopen(optarg,"r"))==0){
                    perror("Could not open file");
                    break;
                }
           
                fscanf(fp,"%d",&n);
               // printf("n = %d",n);
                for(int j=0;j<n;j++){
                    fscanf(fp,"%d %d %d",&hs.from,&hs.to,&hs.limit);
                    list_from_file.push_back(hs);
                }
                n = setHiShapeRanges(list_from_file);
                //print_elements(list_from_file);
                fclose(fp);
                break;
            // -- set the interface --
            case 'i':
                handle = open(DEVICE_FILE_NAME, 0);
                if(handle < 0) {
                    perror("could not open device");
                } else {
                    if ((errno = ioctl(handle, IOCTL_HISHAPE_SET_DEVICE, optarg)) < 0) {
                        perror("IOCTL_HISHAPE_SET_DEVICE failed");
                    }
                    close(handle);
                }
                break;
            // -- unset the interface (set to 'any') --
            case 'a':
                handle = open(DEVICE_FILE_NAME, 0);
                if(handle < 0) {
                    perror("could not open device");
                } else {
                    if ((errno = ioctl(handle, IOCTL_HISHAPE_SET_DEVICE, NULL)) < 0) {
                        perror("IOCTL_HISHAPE_SET_DEVICE failed");
                    }
                    close(handle);
                }
                break;
            // -- print the interface --
            case 'p':
                handle = open(DEVICE_FILE_NAME, 0);
                if(handle < 0) {
                    perror("could not open device");
                } else {
                    if ((errno = ioctl(handle, IOCTL_HISHAPE_GET_DEVICE, device)) < 0) {
                        perror("IOCTL_HISHAPE_GET_DEVICE failed");
                    } else {
                        printf("interface is %s\n", device[0]!='\0'?device:"any device");
                    }
                    close(handle);
                }
                break;
            // -- parse 'from-ip' -- 
            case 'f':
                if(!inet_aton(optarg, &from)) {
                    from.s_addr = getLong(optarg);
                }
                fromSet = true;
                break;
            // -- parse 'to-ip' -- 
            case 't':
                if(!inet_aton(optarg, &to)) {
                    to.s_addr = getLong(optarg);
                }
                toSet = true;
                break;
            // -- parse 'limit' -- 
            case 'l':
                limit = getLong(optarg);
                limitSet = true;
                break;
            // -- print usage --
            case 'h':
                printUsage(argc, argv);
                break;
            case 'x':
                test(optarg);
                break;
        }
    }
    
    
    // -- adding a new range if possible (no overlap) --
    if(fromSet && toSet && limitSet) {
        list<HiShapeRange> ranges = getHiShapeRanges();
        HiShapeRange newRange;
        newRange.from = (uint32_t)from.s_addr;
        newRange.to = (uint32_t)to.s_addr;
        newRange.limit = limit;
        ranges.push_back(newRange);
        setHiShapeRanges(ranges);
    }

    return EXIT_SUCCESS;
}

/**
 * parses a string for a non-negativ number. exits the programm on parsing error
 * or if the number is smaller than zero
 * @param optarg the string to be parsed
 * @return the number
 */
long getLong(char* optarg) {
    char* endptr;
    errno = 0;
    long dummy = strtol(optarg, &endptr, 10);
    if(errno != 0 || endptr == optarg || dummy < 0) {
        fprintf(stderr, "%s is not valid\n", optarg);
        exit(EXIT_FAILURE);
    } else {
        return dummy;
    }
}

void printUsage(int argc, char *argv[]) {
    printf("Usage: %s [OPTION...]\n\n", argv[0]);
    printf(" Options:\n\n");
    printf("  -L, --list               List the ranges\n");
    printf("  -F, --flush              Flush the ranges\n");
    printf("  -S, --set=FILE           Load ranges from FILE\n");
    printf("  -i, --interface=DEVICE   Set interface to DEVICE\n");
    printf("  -a, --any_device         Unset interface\n");
    printf("  -p, --print_interface    Print interface\n");
    printf("  -f, --from               Start ip address for a new range (integer or dotted notation)\n");
    printf("  -t, --to                 End ip address for a new range (integer or dotted notation)\n");
    printf("  -l, --limit              Limit of the new range in kbyte/s\n");
    printf("  -h, --help               Print this message and exit\n");
    printf("\n");
}

void test(char* optarg) {
    int handle = open(DEVICE_FILE_NAME, 0);
    if(handle < 0) {
        perror("could not open device");
    } else {
        long n = atol(optarg);
        if ((errno = ioctl(handle, IOCTL_HISHAPE_RESERVE, n)) != SUCCESS) {
            perror("IOCTL_HISHAPE_RESERVE failed");
        } else {
            HiShapeRange newRange;
            for(uint32_t i=0; i<n; i++) {
                newRange.from = i << 16;
                newRange.to = (i << 16) + 255 * 256 + 255;
                newRange.limit = i%2?500:100;
                if ((errno=ioctl(handle, IOCTL_HISHAPE_ADD, &newRange)) != SUCCESS) {
                    perror("IOCTL_HISHAPE_ADD failed");
                    break;
                }
            }
        }
        close(handle);
    }
}
