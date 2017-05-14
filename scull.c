#include <linux/fs.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include "scull.h"

/*全局变量*/
int scull_major = 0;
int scull_minor = 0;
int scull_dev_num = 4;
int qset = 10;
int scull_quantum = 500;
ssize_t dev_max_size = 50000;
struct scull_dev *scull_devices;

int scull_open(struct inode *inode, struct file *filp);
int scull_release(struct inode *inode, struct file *filp);
ssize_t scull_read(struct file *filp, char __user *buff, size_t count, loff_t *fpos);
ssize_t scull_write(struct file *filp, const char __user *buff, size_t count, loff_t *fpos);

struct file_operations scull_fops = {
        .owner = THIS_MODULE,
        .open = scull_open,
        .release = scull_release,
        .read = scull_read,
        .write = scull_write,
};

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
    printk(KERN_ALERT "56!\n");
    struct cdev *cdev = inode->i_cdev;
    struct scull_dev *dev = container_of(cdev, struct scull_dev, cdev);
    filp->private_data = dev;
    printk(KERN_ALERT "59!\n");
    /*若以只写方式方式打开，则清空数据*/
    if((filp->f_flags & O_ACCMODE) == O_WRONLY){
        scull_trim(dev);
    }
    printk(KERN_ALERT "open successfully!\n");
    return 0;
}

/*关闭设备文件*/
int scull_release(struct inode *inode, struct file *filp){
    /*文件释放inode节点*/
    filp->private_data = NULL;
    printk(KERN_ALERT "release successfully!\n");
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

    if(copy_to_user(buff, ptr->data[spos] + qpos, count)){        //拷贝不成功则返回错误码
        return -EFAULT;
    }
    printk(KERN_ALERT "读取%i字节!\n", (int)count);
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
    while(item--){
        ptr = ptr->next;
    }
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
    if(!ptr->data[spos]){
        ptr->data[spos] = kmalloc(dev->quantum * sizeof(char), GFP_KERNEL);
    }
    if(qpos + count > dev->quantum)
        count = dev->quantum - qpos;

    if(copy_from_user(ptr->data[spos] + qpos, buff, count)){        //拷贝不成功则返回错误码
        return -EFAULT;
    }
    printk(KERN_ALERT "写入%i字节!\n", (int)count);
    *fpos += count;
    if(dev->size < *fpos)
        dev->size = *fpos;

    return count;

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
    printk(KERN_ALERT "exit successfully!\n");
}

/*模块初始化*/
static int scull_init(void){
    /*分配设备号*/
    dev_t devt;
    int result, i;
    if(scull_major){
        devt = MKDEV(scull_major, scull_minor);
        result = register_chrdev_region(devt, scull_dev_num, "scull");
    }
    else{
        result = alloc_chrdev_region(&devt, scull_minor, scull_dev_num, "scull");
        scull_major = MAJOR(devt);
    }
    if(result){         //分配设备号失败了则直接返回错误码
        printk(KERN_ALERT "can't register a device number\n");
        return result;
    }

    /*初始化数据结构*/
    scull_devices = kmalloc(scull_dev_num * sizeof(struct scull_dev), GFP_KERNEL);
    if(!scull_devices){         //分配内存失败则要做撤销工作
        printk(KERN_ALERT "can't allocate memory for devices\n");
        result = ENOMEM;
        goto fail;
    }
    for(i = 0; i < scull_dev_num; i++) {
        scull_devices[i].data = NULL;
        scull_devices[i].qset = qset;
        scull_devices[i].quantum = scull_quantum;
        scull_devices[i].size = 0;
        cdev_init(&scull_devices[i].cdev, &scull_fops);
        if ((result = cdev_add(&scull_devices[i].cdev, devt, 1))) {         //添加设备失败则要做撤销操作
            printk(KERN_ALERT "can't add device%i\n", i);
            goto fail;
        }
    }
    printk(KERN_ALERT "init successfully!\n");
    return 0;
  fail:
    scull_exit();
    return result;
}

module_init(scull_init);
module_exit(scull_exit);
