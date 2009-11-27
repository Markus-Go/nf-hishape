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
 * File: nf-hishape.h
 * Purpose: header file for the nf-HiShape kernel module and its library/userland tool
 * Responsible: Matthias Reif
 * Primary Repository: https://svn.iupr.org/projects/nf-HiShape
 * Web Sites: www.iupr.org, www.dfki.de
 */

#include <linux/ioctl.h>
//#include <linux/time.h>
//#include <linux/spinlock.h>
#include <linux/param.h>

#define MAJOR_NUM 100
#define RAND_MAX_VALUE 255
#define N_RAND 1000
#define MAX_DEVICE_LENGTH 256

// 250 - 4
// 500 - 2

#define QUEUE_SIZE 1024

#define LOOP_MSEC 100

inline static int kbs2msec(int kbs) {
//    if(kbs <= 100)
        return 500;
//    if(kbs <= 1000)
//        return (-1000/900)*kbs + 1111;
//    return 100;
}

inline static int loops(int kbs) {
    return kbs2msec(kbs) / LOOP_MSEC;
}

inline static uint32_t max_len(int kbs) {
    return (uint32_t)(kbs * 1024.f * kbs2msec(kbs) / 1000.f);
}

static const int LOOP_JIFFIES = (LOOP_MSEC * HZ / 1000);

/*
 * _IOR means that we're creating an ioctl command
 * number for passing information from a user process
 * to the kernel module.
 *
 * The first arguments, MAJOR_NUM, is the major device 
 * number we're using.
 *
 * The second argument is the number of the command 
 * (there could be several with different meanings).
 *
 * The third argument is the type we want to get from 
 * the process to the kernel.
 */

typedef struct {
    uint32_t from, to;
    uint32_t limit;
} HiShapeRange;

typedef struct {
    int (*okfn)(struct sk_buff *);
    struct sk_buff *pskb;
} Packet;

typedef struct {
    Packet packets[QUEUE_SIZE];
    int head, size;
} PacketQueue;

typedef struct {
//    spinlock_t lock;
    uint32_t len, maxLen;
    uint8_t loops, loop;
    PacketQueue queue;
} HiShapeStat;

typedef struct {
    uint32_t length;
    HiShapeRange* ranges;
} HiShapeRanges;

#define IOCTL_HISHAPE_GET_RANGES _IOR(MAJOR_NUM, 0, HiShapeRanges*)
#define IOCTL_HISHAPE_SET_RANGES _IOR(MAJOR_NUM, 1, HiShapeRanges*)
#define IOCTL_HISHAPE_GET_DEVICE _IOR(MAJOR_NUM, 2, char*)
#define IOCTL_HISHAPE_SET_DEVICE _IOR(MAJOR_NUM, 3, char*)
#define IOCTL_HISHAPE_FLUSH _IOR(MAJOR_NUM, 4, void*)
#define IOCTL_HISHAPE_RESERVE _IOR(MAJOR_NUM, 5, uint32_t)
#define IOCTL_HISHAPE_ADD _IOR(MAJOR_NUM, 6, HiShapeRange*)

#define DEVICE_NAME "nf-hishape"
#define DEVICE_FILE_NAME "/dev/nf-hishape"

#define SUCCESS 0
