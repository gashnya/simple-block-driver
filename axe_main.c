#include <linux/init.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/fs.h>
#include <linux/vmalloc.h>
#include <linux/blk-mq.h>
#include <linux/bio.h>

#include "axe_common.h"

#define AXE_KMAP_ATOMIC(bio, iter)                            \
    	(kmap_atomic(bio_iter_iovec((bio), (iter)).bv_page) + \
                bio_iter_iovec((bio), (iter)).bv_offset)

static int axe_blk_major = 0;
static char *axe_blk_name = "axe_test";

module_param(axe_blk_name, charp, 0);

struct axe_device *axe_dev = NULL;

static const int nsectors = 204800;
static const int hardsect_size = 512;

static const struct block_device_operations axe_blk_fops = {
        .owner = THIS_MODULE,
};

MODULE_LICENSE("Dual BSD/GPL");

static void axe_copy_data(sector_t sector, unsigned long nsect,
                          char *buffer, int is_write)
{
        unsigned long offset = sector * hardsect_size;
        unsigned long nbytes = nsect * hardsect_size;

        if ((offset + nbytes) > axe_dev->size) {
                pr_err("beyond-end write\n");
                return;
        }
        if (is_write) {
                memcpy(axe_dev->data + offset, buffer, nbytes);
        } else {
                memcpy(buffer, axe_dev->data + offset, nbytes);
        }
}

static int _axe_make_request(struct bio *bio)
{
        struct bio_vec bvec;
        struct bvec_iter iter;
        sector_t sector = bio->bi_iter.bi_sector;

        bio_for_each_segment(bvec, bio, iter) {
                char *buffer = AXE_KMAP_ATOMIC(bio, iter);

                axe_copy_data(sector, (bio_cur_bytes(bio) / hardsect_size),
                              buffer, bio_data_dir(bio) == WRITE);
                sector += (bio_cur_bytes(bio) / hardsect_size);
                kunmap_atomic(buffer);
        }

        return 0;
}

static blk_qc_t axe_make_request(struct request_queue *q, struct bio *bio)
{
        bio->bi_status = _axe_make_request(bio);
        bio_endio(bio);

        return BLK_STS_OK;
}

static int axe_dev_add_device(void)
{
        int ret = -ENOMEM;

        axe_dev = kzalloc(sizeof(struct axe_device), GFP_KERNEL);
        if (!axe_dev) {
                pr_err("axe: cannot kzalloc\n");
                goto out;
        }
        axe_dev->size = nsectors * hardsect_size;
        axe_dev->data = vmalloc(axe_dev->size);

        if (!axe_dev->data) {
                pr_err("axe: cannot vmalloc\n");
                goto out_free_data;
        }

        axe_dev->gd = alloc_disk(1);
        pr_warn("axe: disk allocated\n");
        if (!axe_dev->gd) {
                pr_err("axe: cannot allocate gendisk\n");
                goto out_free_device;
        }

        axe_dev->q = blk_alloc_queue(GFP_KERNEL);
        if (!axe_dev->q) {
                pr_err("axe: cannot allocate queue\n");
                goto out_free_device;
        }
        blk_queue_make_request(axe_dev->q, axe_make_request);

        axe_dev->gd->queue = axe_dev->q;
        axe_dev->gd->queue->queuedata = axe_dev;
        axe_dev->gd->private_data = axe_dev;
        axe_dev->gd->major = axe_blk_major;
        axe_dev->gd->fops = &axe_blk_fops;
        axe_dev->gd->first_minor = 0;
        set_capacity(axe_dev->gd, (nsectors * hardsect_size) >> 9);
        snprintf(axe_dev->gd->disk_name, 32, "_%s", axe_blk_name);
        add_disk(axe_dev->gd);

        pr_info("axe: disk added\n");

        ret = 0;
        goto out;

out_free_data:
        vfree(axe_dev->data);
out_free_device:
        kzfree(axe_dev);
out:
        return ret;
}

static int __init axe_init(void)
{
        int ret = 0;
        axe_blk_major = register_blkdev(axe_blk_major, axe_blk_name);
        if (axe_blk_major < 1) {
                pr_error("axe: cannot register device\n");

                return -EBUSY;
        }

        pr_info("axe: axe %s registered\n", axe_blk_name);

        ret = axe_dev_add_device();

        return ret;
}

static void __exit axe_exit(void)
{
        if (axe_dev->gd) {
                del_gendisk(axe_dev->gd);
        }

        if (axe_dev->q) {
                blk_cleanup_queue(axe_dev->q);
        }

        if (axe_dev->gd) {
                put_disk(axe_dev->gd);
        }

        vfree(axe_dev->data);

        unregister_blkdev(axe_blk_major, axe_blk_name);
        pr_info("axe: axe %s unregistered\n", axe_blk_name);

        kfree(axe_dev);
        pr_info("axe: axe %s removed\n", axe_blk_name);
}

module_init(axe_init);
module_exit(axe_exit);

