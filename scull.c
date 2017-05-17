#include <linux/fs.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>

#include <linux/cdev.h>
#include <linux/ioctl.h>

#include "scull.h"

/*数据定义*/
struct scull_qset{
    void **data;
    struct scull_qset *next;
};

struct scull_dev{
    void *data;
    int qset;
    int quantum;
    int size;
    struct cdev cdev;
};

/*函数原型*/
static void scull_exit(void);
static int scull_init(void);

static void *scull_seq_start(struct seq_file *sfile, loff_t *pos);
static void scull_seq_stop(struct seq_file *sfile, void *v);
static void *scull_seq_next(struct seq_file *sfile, void *v, loff_t *pos);
static int scull_seq_show(struct seq_file *sfile, void *v);
int scull_seq_open(struct inode *inode, struct file *filp);

int scull_open(struct inode *inode, struct file *filp);
int scull_release(struct inode *inode, struct file *filp);
ssize_t scull_read(struct file *filp, char __user *buff, size_t count, loff_t *fpos);
ssize_t scull_write(struct file *filp, const char __user *buff, size_t count, loff_t *fpos);
long scull_ioctl(struct file *, unsigned int cmd, unsigned long arg);

void scull_trim(struct scull_dev *dev);

/*全局变量*/
int scull_major = 0;
int scull_minor = 0;
int scull_dev_num = 4;
int qset = SCULL_QSET;
int quantum = SCULL_QUANTUM;
ssize_t dev_max_size = 50000;
struct scull_dev *scull_devices;

struct file_operations scull_fops = {
        .owner = THIS_MODULE,
        .open = scull_open,
        .release = scull_release,
        .read = scull_read,
        .write = scull_write,
        .unlocked_ioctl = scull_ioctl,
};

struct seq_operations scull_sops = {
        .start = scull_seq_start,
        .next = scull_seq_next,
        .stop = scull_seq_stop,
        .show = scull_seq_show,
};

struct file_operations scull_seq_fops = {
        .owner = THIS_MODULE,
        .open = scull_seq_open,
        .read = seq_read,
        .llseek = seq_lseek,
        .release = seq_release,
};

/*函数定义*/

int scull_seq_open(struct inode *inode, struct file *filp){
    printk(KERN_ALERT "zxl:open scull_seq successfully\n");
    return seq_open(filp, &scull_sops);
}

/*起始设备*/
static void *scull_seq_start(struct seq_file *sfile, loff_t *pos){
    if(*pos < scull_dev_num)
        return scull_devices + *pos;
    return NULL;
}

static void scull_seq_stop(struct seq_file *sfile, void *v){}

/*下一设备*/
static void *scull_seq_next(struct seq_file *sfile, void *v, loff_t *pos){
    if(++(*pos) < scull_dev_num)
        return scull_devices + *pos;
    return NULL;
}

/*打印一些设备信息*/
static int scull_seq_show(struct seq_file *sfile, void *v){
    struct scull_dev *dev = (struct scull_dev *)v;
    struct scull_qset *ptr = dev->data;
    int i;
    seq_printf(sfile, "device %i, qset %i, quantum %i, size %i\n", dev - scull_devices, dev->qset, dev->quantum, (int)dev->size);

    while(ptr){
        if(ptr->data){
            for(i = 0; i < dev->qset; i++){
                seq_printf(sfile, "item %p, set %i, quantum at %p\n", ptr, i, ptr->data[i]);
            }
        }
        ptr = ptr->next;
    }
    return 0;
}

/*清除设备数据*/
void scull_trim(struct scull_dev *dev){
    struct scull_qset *ptr = dev->data, *next;
    void **data;
    int i;
    while (ptr){
        if((data = ptr->data)){
            for(i = 0; i < dev->qset; i++){
                if(data[i]){
                    kfree(data[i]);
                    data[i] = NULL;
                }
            }
            kfree(data);
            ptr->data = NULL;
        }
        next = ptr->next;
        kfree(ptr);
        ptr = next;
    }
    dev->data = NULL;
    dev->size = 0;
}

/*打开设备文件*/
int scull_open(struct inode *inode, struct file *filp){
    /*文件指向inode节点*/
    struct cdev *cdev = inode->i_cdev;
    struct scull_dev *dev = container_of(cdev, struct scull_dev, cdev);
    filp->private_data = dev;

    /*若以只写方式方式打开，则清空数据*/
    if((filp->f_flags & O_ACCMODE) == O_WRONLY)
        scull_trim(dev);

    printk(KERN_ALERT "zxl:open successfully!\n");
    return 0;
}

/*关闭设备文件*/
int scull_release(struct inode *inode, struct file *filp){
    /*文件释放inode节点*/
    filp->private_data = NULL;
    printk(KERN_ALERT "zxl:release successfully!\n");
    return 0;
}

/*读设备文件*/
ssize_t scull_read(struct file *filp, char __user *buff, size_t count, loff_t *fpos){
    long int item, spos, qpos;
    ssize_t itemsize, remain;
    struct scull_dev *dev = filp->private_data;
    struct scull_qset *ptr = dev->data;
    itemsize = dev->qset * dev->quantum;

    /*到达文件尾,返回 0 字节*/
    if(*fpos >= dev->size)
        return 0;

    /*超出可读数量，截断*/
    if(*fpos + count > dev->size)
        count = dev->size - *fpos;

    /*找到读取的位置， 最多读一个量子*/
    item = *fpos / itemsize;
    remain = *fpos % itemsize;
    spos = remain / dev->quantum;
    qpos = remain % dev->quantum;
    while(item--){
        ptr = ptr->next;
    }

    if(qpos + count > dev->quantum)
        count = dev->quantum - qpos;

    if(copy_to_user(buff, ptr->data[spos] + qpos, count))        //拷贝不成功则返回错误码
        return -EFAULT;

    printk(KERN_ALERT "zxl:读取%i字节!\n", (int)count);
    *fpos += count;

    return count;
}

/*写设备文件*/
ssize_t scull_write(struct file *filp, const char __user *buff, size_t count, loff_t *fpos){
    long int item, spos, qpos, i;
    ssize_t itemsize, remain;
    struct scull_dev *dev = filp->private_data;
    struct scull_qset *ptr = dev->data;
    itemsize = dev->qset * dev->quantum;

    /*超过文件尾,返回 0 字节*/
    if(*fpos > dev->size)
        return 0;

    /*超出可写数量，截断*/
    if(*fpos + count > dev_max_size)
        count = dev_max_size - *fpos;

    /*找到写入的位置， 最多写一个量子*/
    item = *fpos / itemsize;
    remain = *fpos % itemsize;
    spos = remain / dev->quantum;
    qpos = remain % dev->quantum;
    while(item--)
        ptr = ptr->next;

    if(!dev->data){
        ptr = dev->data = kmalloc(sizeof(struct scull_qset), GFP_KERNEL);
        ptr->data = kmalloc(dev->qset * sizeof(char *), GFP_KERNEL);
        for(i = 0; i < dev->qset; i++)
            ptr->data[i] = NULL;
        ptr->next = NULL;
    }
    if(!ptr){
        ptr = kmalloc(sizeof(struct scull_qset), GFP_KERNEL);
        ptr->data = kmalloc(dev->qset * sizeof(char *), GFP_KERNEL);
        for(i = 0; i < dev->qset; i++)
            ptr->data[i] = NULL;
        ptr->next = NULL;
    }
    if(!ptr->data[spos])
        ptr->data[spos] = kmalloc(dev->quantum * sizeof(char), GFP_KERNEL);

    if(qpos + count > dev->quantum)
        count = dev->quantum - qpos;

    if(copy_from_user(ptr->data[spos] + qpos, buff, count))        //拷贝不成功则返回错误码
        return -EFAULT;

    printk(KERN_ALERT "zxl:写入%i字节!\n", (int)count);
    *fpos += count;
    if(dev->size < *fpos)
        dev->size = *fpos;

    return count;
}

long scull_ioctl(struct file *filp, unsigned int cmd, unsigned long arg){
    int err, result = 0, tmp;

    /*检查命令类型和编号范围合法性*/
    if(_IOC_TYPE(cmd) != SCULL_IOC_MAGIC) return -ENOTTY;
    if(_IOC_NR(cmd) > SCULL_IOC_MAXNUM) return  -ENOTTY;

    /*检查读/写地址合法性*/
    if(_IOC_DIR(cmd) & _IOC_READ)
        err = !access_ok(VERIFY_WRITE, (void *)arg, _IOC_SIZE(cmd));
    if(_IOC_DIR(cmd) & _IOC_WRITE)
        err = !access_ok(VERIFY_READ, (void *)arg, _IOC_SIZE(cmd));
    if(err)
        return -EFAULT;

    /*命令的实现*/
    switch(cmd){
        case SCULL_IOCRESET:
            qset = SCULL_QSET;
            quantum = SCULL_QUANTUM;
            break;

        case SCULL_IOCTQUANTUM:
            if(!capable(CAP_SYS_ADMIN))
                return -EPERM;
            quantum = arg;
            break;

        case SCULL_IOCTQSET:
            if(!capable(CAP_SYS_ADMIN))
                return -EPERM;
            qset = arg;
            break;

        case SCULL_IOCSQUANTUM:
            if(!capable(CAP_SYS_ADMIN))
                return -EPERM;
            result = __get_user(quantum, (int *)arg);
            break;

        case SCULL_IOCSQSET:
            if(!capable(CAP_SYS_ADMIN))
                return -EPERM;
            result = __get_user(qset, (int *)arg);
            break;

        case SCULL_IOCQQUANTUM:
            return quantum;

        case SCULL_IOCQQSET:
            return qset;

        case SCULL_IOCGQUANTUM:
            result = __put_user(quantum, (int *)arg);
            break;

        case SCULL_IOCGQSET:
            result = __put_user(qset, (int *)arg);
            break;

        case SCULL_IOCHQUANTUM:
            if(!capable(CAP_SYS_ADMIN))
                return -EPERM;
            tmp = quantum;
            quantum = arg;
            return tmp;

        case SCULL_IOCHQSET:
            if(!capable(CAP_SYS_ADMIN))
                return -EPERM;
            tmp = qset;
            qset = arg;
            return tmp;

        case SCULL_IOCXQUANTUM:
            if(!capable(CAP_SYS_ADMIN))
                return -EPERM;
            tmp = quantum;
            result = __get_user(quantum, (int *)arg);
            if(!result)
                result = __put_user(tmp, (int *)arg);
            break;

        case SCULL_IOCXQSET:
            if(!capable(CAP_SYS_ADMIN))
                return -EPERM;
            tmp = qset;
            result = __get_user(qset, (int *)arg);
            if(!result)
                result = __put_user(tmp, (int *)arg);
            break;

        default:
            return -ENOTTY;
    }

    return result;
}

/*清除模块*/
static void scull_exit(void){
    int i;
    dev_t devt = MKDEV(scull_major, scull_minor);

    /*清除数据结构*/
    if(scull_devices) {
        for (i = 0; i < scull_dev_num; i++) {
            scull_trim(&scull_devices[i]);
            cdev_del(&scull_devices[i].cdev);
        }
        kfree(scull_devices);
    }

    /*释放设备号*/
    unregister_chrdev_region(devt, scull_dev_num);
    remove_proc_entry("scullseq", NULL);
    printk(KERN_ALERT "zxl:exit successfully!\n");
}

/*模块初始化*/
static int scull_init(void){
    dev_t devt;
    int result, i;

    /*分配设备号*/
    if(scull_major){
        devt = MKDEV(scull_major, scull_minor);
        result = register_chrdev_region(devt, scull_dev_num, "scull");
    }
    else{
        result = alloc_chrdev_region(&devt, scull_minor, scull_dev_num, "scull");
        scull_major = MAJOR(devt);
    }
    if(result){         //分配设备号失败了则直接返回错误码
        printk(KERN_ALERT "zxl:can't register a device number\n");
        return result;
    }

    /*初始化数据结构*/
    scull_devices = kmalloc(scull_dev_num * sizeof(struct scull_dev), GFP_KERNEL);
    if(!scull_devices){         //分配内存失败则要做撤销工作
        printk(KERN_ALERT "zxl:can't allocate memory for devices\n");
        result = ENOMEM;
        goto fail;
    }
    for(i = 0; i < scull_dev_num; i++) {
        scull_devices[i].data = NULL;
        scull_devices[i].qset = qset;
        scull_devices[i].quantum = quantum;
        scull_devices[i].size = 0;
        cdev_init(&scull_devices[i].cdev, &scull_fops);
        if ((result = cdev_add(&scull_devices[i].cdev, devt, 1))) {         //添加设备失败则要做撤销操作
            printk(KERN_ALERT "zxl:can't add device%i\n", i);
            goto fail;
        }
    }

    proc_create("scullseq", 0, NULL, &scull_seq_fops);

    printk(KERN_ALERT "zxl:init successfully!\n");
    return 0;

  fail:
    scull_exit();
return result;
}

module_init(scull_init);
module_exit(scull_exit);