#include <linux/fs.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/slab.h>
#include "scull.h"

/*全局变量*/
int scull_major = 0;
int scull_minor = 0;
int scull_dev_num = 4;
int qset = 10;
int scull_quantum = 500;
struct scull_dev *scull_devices;

struct file_operations scull_fops = {
        .owner = THIS_MODULE,
};

/*清除设备数据*/
void scull_trim(struct scull_dev *dev){
    struct scull_qset *ptr = dev->data, *next;
    void **data;
    int i;
    while (ptr){
       if(data = ptr->data){
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
        if (result = cdev_add(&scull_devices[i].cdev, devt, 1)) {         //添加设备失败则要做撤销操作
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
