#ifndef AXE_BLOCK_DRIVER_AXE_COMMON_H
#define AXE_BLOCK_DRIVER_AXE_COMMON_H

struct axe_device {
        u8 *data;
        struct request_queue *q;
        struct gendisk *gd;
        unsigned int size;
};

#endif

