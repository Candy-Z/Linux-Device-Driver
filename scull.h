//
// Created by cathy on 17-5-14.
//

#ifndef SCULL_ZXL_SCULL_H
#define SCULL_ZXL_SCULL_H

#endif //SCULL_ZXL_SCULL_H

#include <linux/cdev.h>

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

