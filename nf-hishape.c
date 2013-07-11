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
 * File: nf-hishape.c
 * Purpose: kernel module for simple but high performance traffic shaping
 * Responsible: Matthias Reif
 * Primary Repository: https://svn.iupr.org/projects/nf-HiShape
 * Web Sites: www.iupr.org, www.dfki.de
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/netfilter_ipv4.h>
#include <linux/ip.h>
#include <linux/fs.h>
#include <linux/random.h>
#include <linux/errno.h>
#include <linux/timer.h>
#include <linux/vmalloc.h>

#include "nf-hishape.h"



MODULE_LICENSE("GPL");
MODULE_AUTHOR("Matthias Reif <matthias.reif@iupr.dfki.de>");
MODULE_VERSION("0.1");
MODULE_DESCRIPTION("simple but high performance traffic shaping");
MODULE_SUPPORTED_DEVICE(DEVICE_NAME)

/// the device name, initially set by module parameter
static char* device = NULL;
/// memory for device name if set by iocl-call
static char deviceMem[MAX_DEVICE_LENGTH];
/// the hook name, set by module parameter
static char* hook;
/// priority of the module in the netfilter list
static int priority = NF_IP_PRI_FIRST + 1;
/// number of ranges
int nRanges = 0;
/// array of all ranges
HiShapeRange* ranges = NULL;
/// array of statistics of all ranges
HiShapeStat* stats = NULL;
/// the timer
struct timer_list timer;
/// lock for mutli process safety
spinlock_t globalLock;
spinlock_t ioctlLock;

module_param(device, charp, S_IRUGO);
MODULE_PARM_DESC(device, "the network device");
module_param(hook, charp, S_IRUGO);
MODULE_PARM_DESC(hook, "position of filtering");
module_param(priority, int, S_IRUGO);
MODULE_PARM_DESC(priority, "priority");

/**
 * searches for the range containing the given ip
 * @param ip the ip to search for
 * @param result index of the found range (only valid when function returns 1)
 * @return 1 when found, 0 otherwise
 */ 
char search(u_int32_t ip, int* result) {
    static int start, end, mid, found;
    start = 0;
    end = nRanges - 1;
    mid = 0;
    found = 0;
    while(!found && start <= end) {
        mid = start + ((end - start) / 2);
        if(ip < ranges[mid].from) {
            end = mid - 1;
        } else if(ip > ranges[mid].to) {
            start = mid + 1;
        } else {
            found = 1;
        }
    }
    *result = mid;
    return found;
}

/**
 * handler for timer events.\n
 * send as much as possible packets of a ranges queue and resets its traffic
 * counter if necessary
 * @param data default parameter, unused 
 */
void timer_handler(unsigned long int data) {
    static int i;
    static Packet p;
    
    spin_lock(&globalLock);
    for(i=0; i<nRanges; i++) {
//        spin_lock(&stats[i].lock);
        stats[i].loop++;
        
        if(stats[i].loop == stats[i].loops) {
            stats[i].len = 0;
            stats[i].loop = 0;
        
            // -- while there are packets in the queue --
            while(stats[i].queue.size > 0) {
                p = stats[i].queue.packets[stats[i].queue.head];
                // --if bandwith of range is exceeded, stop --
                if(stats[i].len + p.pskb->len > stats[i].maxLen) {
                    break;
                }
                // -- deliver the next packet --
                stats[i].queue.head = (stats[i].queue.head + 1) % QUEUE_SIZE;
                stats[i].queue.size--;
                stats[i].len += p.pskb->len;
                // -- reinject packet --
                (p.okfn)(p.pskb);
//                kfree_skb(p.pskb);
            }
        }
//        spin_unlock(&stats[i].lock);
    }
    spin_unlock(&globalLock);
    timer.expires = jiffies + LOOP_JIFFIES;
    add_timer(&timer);
}


/**
 * netfilter hook for every packet
 */
static unsigned int
    hishape_hook(unsigned int hook, struct sk_buff **pskb, const struct net_device *indev,
    const struct net_device *outdev, int (*okfn)(struct sk_buff *)) {
    static int rangeIndex;
    static int queueIndex;
    static int returnValue;
    static char* relevantDevice;
    
    returnValue = NF_ACCEPT;
    
    // -- device depends on position of module in packet management --
    if(hook <= NF_INET_FORWARD) {
        relevantDevice = indev->name;
    } else {
        relevantDevice = outdev->name;
    }
    
    spin_lock(&ioctlLock);
    spin_lock(&globalLock);
    // -- check for right device: no match => accept --
    if(relevantDevice != NULL && (device == NULL || strcmp(relevantDevice, device) == 0)) {
        // -- search according range --
          if(search(ip_hdr(*pskb)->saddr, &rangeIndex)) {
        //if(search((*pskb)->nh.iph->saddr, &rangeIndex)) {
//            spin_lock(&stats[rangeIndex].lock);
            // -- if there are packtes in the queue or bandwidth exceeded --
            if(stats[rangeIndex].queue.size > 0
                    || stats[rangeIndex].len + (*pskb)->len > stats[rangeIndex].maxLen) {
                // -- if queue isn't full, queue it --
                if(stats[rangeIndex].queue.size < QUEUE_SIZE) {
                    queueIndex = stats[rangeIndex].queue.head + stats[rangeIndex].queue.size;
                    queueIndex %= QUEUE_SIZE;
                    stats[rangeIndex].queue.packets[queueIndex].okfn = okfn;
                    stats[rangeIndex].queue.packets[queueIndex].pskb = *pskb;
                    stats[rangeIndex].queue.size++;
//                    spin_unlock(&stats[rangeIndex].lock);
                    returnValue = NF_STOLEN;
                // -- if the queue is full, drop the packet immediately --
                } else {
//                    spin_unlock(&stats[rangeIndex].lock);
                    returnValue = NF_DROP;
                }
            // -- no packets in the queue and bandwidth available => accept the packet --
            } else {
                stats[rangeIndex].len += (*pskb)->len;
//                spin_unlock(&stats[rangeIndex].lock);
                returnValue = NF_ACCEPT;
            }
        }
    }
    spin_unlock(&globalLock);
    spin_unlock(&ioctlLock);
    return returnValue;
}

static struct nf_hook_ops hishape_ops = {
    .hook = hishape_hook,
    .owner = THIS_MODULE,
    .pf = PF_INET,
    .hooknum = NF_INET_FORWARD,
    .priority = NF_IP_PRI_FIRST + 1
};

/**
 * allocates memory for a bunch of ranges (thread safe)
 * @param n number of ranges; 0 is valid
 * @return SUCCESS on succes, ENOMEM if out of memory
 */
static int reserve(int n) {
    HiShapeRange* rangesNew = NULL;
    HiShapeStat* statsNew = NULL;
    HiShapeRange* rangesOld = NULL;
    HiShapeStat* statsOld = NULL;
    if(n > 0) {
        rangesNew = vmalloc(n*sizeof(HiShapeRange));
        statsNew = vmalloc(n*sizeof(HiShapeStat));
        // -- on any error => abort and clean up if necessary --
        if(rangesNew == NULL || statsNew == NULL) {
            if(rangesNew != NULL)
                vfree(rangesNew);
            if(statsNew != NULL)
                vfree(statsNew);
            printk(KERN_INFO "IOCTL_HISHAPE_SET_RANGES: memory allocation of %lu bytes failed :(\n", 
                    n*sizeof(HiShapeRange) + n*sizeof(HiShapeStat));
            return ENOMEM;
        }
    }
    // -- save original pointers --
    rangesOld = ranges;
    statsOld = stats;
    // -- update pointers thread safe --
    spin_lock(&ioctlLock);
    ranges = rangesNew;
    stats = statsNew;
    nRanges = 0;
    spin_unlock(&ioctlLock);
    // -- free original pointers --
    if(rangesOld != NULL)
        vfree(rangesOld);
    if(statsOld != NULL)
        vfree(statsOld);
    return SUCCESS;
}

static int device_open(struct inode *inode, struct file *file) {
#ifdef DEBUG
    printk(KERN_INFO "nf-HiShape: deviceOpen(%p)\n", file);
#endif
    try_module_get(THIS_MODULE);
    return SUCCESS;
}

static int device_release(struct inode *inode, struct file *file) {
#ifdef DEBUG
    printk(KERN_INFO "nf-HiShape: device_release(%p,%p)\n", inode, file);
#endif
    module_put(THIS_MODULE);
    return SUCCESS;
}

int device_ioctl(struct inode *inode, struct file *file, unsigned int ioctl_num, unsigned long ioctl_param) {
    int i;
    int returnValue = SUCCESS;
#ifdef DEBUG
    printk(KERN_INFO "nf-HiShape: ioctl %d\n", ioctl_num);
#endif
    switch (ioctl_num) {
        // -- return current ranges --
        case IOCTL_HISHAPE_GET_RANGES:
            i = nRanges;
            if(i > ((HiShapeRanges*)ioctl_param)->length)
                i = ((HiShapeRanges*)ioctl_param)->length;
	    	
            ((HiShapeRanges*)ioctl_param)->length = i;
            copy_to_user(((HiShapeRanges*)ioctl_param)->ranges, ranges, i * sizeof(HiShapeRange));
            break;
        // -- set new ranges --
        case IOCTL_HISHAPE_SET_RANGES:
            returnValue = reserve(((HiShapeRanges*)ioctl_param)->length);
            if(returnValue == SUCCESS) {
                spin_lock(&ioctlLock);
                nRanges = ((HiShapeRanges*)ioctl_param)->length;
                if(nRanges > 0) {
                    copy_from_user(ranges, ((HiShapeRanges*)ioctl_param)->ranges, nRanges * sizeof(HiShapeRange));
                    for(i=0; i<nRanges; i++) {
                        stats[i].len = 0;
                        stats[i].maxLen = max_len(ranges[i].limit);
                        stats[i].loop = 0;
                        stats[i].loops = loops(ranges[i].limit);
                        stats[i].queue.head = 0;
                        stats[i].queue.size = 0;
//                      spin_lock_init(&stats[i].lock);
                    }
                }
                spin_unlock(&ioctlLock);
            }
            break;
        // -- clear all ranges --
        case IOCTL_HISHAPE_FLUSH:
            returnValue = reserve(0);
            break;
        // -- allocate memory for defined number of ranges --
        case IOCTL_HISHAPE_RESERVE:
            returnValue = reserve(ioctl_param);
            break;
        case IOCTL_HISHAPE_ADD:
            if(nRanges > 0) {
                if(((HiShapeRange*)ioctl_param)->from <= ranges[nRanges-1].to) {
                    return EINVAL;
                }
            }
            copy_from_user(ranges + nRanges, ((HiShapeRange*)ioctl_param), sizeof(HiShapeRange));
            stats[nRanges].len = 0;
            stats[nRanges].maxLen = max_len(ranges[nRanges].limit);
            stats[nRanges].loop = 0;
            stats[nRanges].loops = loops(ranges[nRanges].limit);
            stats[nRanges].queue.head = 0;
            stats[nRanges].queue.size = 0;
//            spin_lock_init(&stats[nRanges].lock);
            spin_lock(&ioctlLock);
            nRanges++;
            spin_unlock(&ioctlLock);
            break;
        // -- return the device name --
        case IOCTL_HISHAPE_GET_DEVICE:
            if(device==NULL || device[0] == '\0') {
                copy_to_user((char*)ioctl_param, "\0", 1);
            } else {
                copy_to_user((char*)ioctl_param, device, strlen(device)+1);
            }
            break;
        // -- set the device name --
        case IOCTL_HISHAPE_SET_DEVICE:
            spin_lock(&ioctlLock);
            if((char*)ioctl_param == NULL || ((char*)ioctl_param)[0] == '\0') {
                device = NULL;
            } else if(strlen((char*)ioctl_param)+1 > MAX_DEVICE_LENGTH) {
                returnValue = EMSGSIZE;
            } else {
                copy_from_user(deviceMem, (char*)ioctl_param, strlen((char*)ioctl_param)+1);
                device = deviceMem;
            }
            spin_unlock(&ioctlLock);
            break;
    }
    return returnValue;
}

struct file_operations ops = {
    .read = NULL,
    .write = NULL,
    //.ioctl = device_ioctl,
    .unlocked_ioctl = device_ioctl,
    .open = device_open,
    .release = device_release,
};

int init_module(void) {
    int ret;
    if(device != NULL && strlen(device) + 1 > MAX_DEVICE_LENGTH)
        return EMSGSIZE;
    
    // -- determine hook num --
    if(hook != NULL) {
        if(!strnicmp(hook,"pre",3))
            hishape_ops.hooknum = NF_INET_PRE_ROUTING;
        else if(!strnicmp(hook,"in",2))
            hishape_ops.hooknum = NF_INET_LOCAL_IN;
        else if(!strnicmp(hook,"for",3))
            hishape_ops.hooknum = NF_INET_FORWARD;
        else if(!strnicmp(hook,"post",4))
            hishape_ops.hooknum = NF_INET_POST_ROUTING;
        else if(!strnicmp(hook,"out",3))
            hishape_ops.hooknum = NF_INET_LOCAL_OUT;
        else
            printk(KERN_INFO "Unknown hook: %s, using NF_INET_FORWARD\n", hook);
    }
    
    hishape_ops.priority = priority;
    
    nRanges = 0;
    
    ret = register_chrdev(MAJOR_NUM, DEVICE_NAME, &ops);
    if (ret < 0) {
        printk(KERN_ALERT "registering the character device failed with %d\n", ret);
        return ret;
    }
    printk(KERN_INFO "nf-HiShape network device is %s\n", device);
    printk(KERN_INFO "nf-HiShape priority is %d\n", hishape_ops.priority);
    printk(KERN_INFO "The major device number is %d.\n", MAJOR_NUM);
    printk(KERN_INFO "If you want to talk to the device driver,\n");
    printk(KERN_INFO "you'll have to create a device file:\n");
    printk(KERN_INFO "mknod %s c %d 0\n", DEVICE_FILE_NAME, MAJOR_NUM);
    
    spin_lock_init(&globalLock);
    spin_lock_init(&ioctlLock);
    
    init_timer(&timer);
    timer.expires = jiffies + LOOP_JIFFIES;
    timer.data = 0;
    timer.function = timer_handler;
    add_timer(&timer);
    
    return nf_register_hook(&hishape_ops);
}

void cleanup_module(void) {
    //int ret;
    del_timer(&timer);
    nf_unregister_hook(&hishape_ops);
    unregister_chrdev(MAJOR_NUM, DEVICE_NAME);
    
    /*ret = unregister_chrdev(MAJOR_NUM, DEVICE_NAME);
    if (ret < 0)
        printk(KERN_ALERT "nf-HiShape: unregister_chrdev failed: %d\n", ret);
    */
    if(nRanges > 0) {
        vfree(ranges);
        vfree(stats);
    }
}

//int randCount = N_RAND;
//unsigned char r[N_RAND];
//
//if(randCount == N_RAND) {
//    get_random_bytes(r, N_RAND);
//    randCount = 0;
//}
//
//if(r[randCount++] < RAND_MAX_VALUE * 0.9) {
//    break;
//}
