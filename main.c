#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/ioctl.h>
#include "scull.h"

#define FILE_PATH   "/dev/scull0"

int main(void)
{
    int fd, quant, i;
    if ((fd = open(FILE_PATH, O_RDWR )) < 0) {
        printf("open error\n");
        exit(-1);
    } else {
        printf("open success\n");
    }
    quant = 2;
    i = ioctl(fd, SCULL_IOCSQUANTUM, &quant);

    return 0;
}