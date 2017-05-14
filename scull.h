//
// Created by cathy on 17-5-14.
//

#ifndef SCULL_ZXL_SCULL_H
#define SCULL_ZXL_SCULL_H

#endif //SCULL_ZXL_SCULL_H

#include <linux/cdev.h>

/*数据定义*/
struct scull_dev{
    void *data;
    int qset;
    int quantum;
    int size;
    struct cdev cdev;
};

struct scull_qset{
    void **data;
    struct scull_qset *next;
};

/*函数定义*/
void scull_trim(struct scull_dev *dev);

static void scull_exit(void);
static int scull_init(void);

int scull_open(struct inode *inode, struct file *filp);
int scull_release(struct inode *inode, struct file *filp);
ssize_t scull_read(struct file *filp, char __user *buff, size_t count, loff_t *fpos);
ssize_t scull_write(struct file *filp, const char __user *buff, size_t count, loff_t *fpos);

static void *scull_seq_start(struct seq_file *sfile, loff_t *pos);
static void scull_seq_stop(struct seq_file *sfile, void *v);
static void *scull_seq_next(struct seq_file *sfile, void *v, loff_t *pos);
static int scull_seq_show(struct seq_file *sfile, void *v);
int scull_seq_open(struct inode *inode, struct file *filp);

