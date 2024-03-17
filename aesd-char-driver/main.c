/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include <linux/slab.h> // kmalloc
#include "aesdchar.h"

int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Dmitry Yurov");
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");

    struct aesd_dev* dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = dev;
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");

    // nothing to do here?
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    
    struct aesd_dev* dev = container_of(filp->private_data, struct aesd_dev, cdev);
    if (!dev) return 0;

    ssize_t retval = mutex_lock_interruptible(&dev->mu);
    if (retval != 0) return -EINTR;

    size_t offset;
    struct aesd_buffer_entry* entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->circ_buf, *f_pos, &offset);
    if (!entry) {
        mutex_unlock(&dev->mu);
        return 0;
    }
    
    ssize_t read_bytes = entry->size - offset;
    if (read_bytes <= 0) {
        mutex_unlock(&dev->mu);
        return -EFAULT;
    }
    unsigned long cnbc = copy_to_user(buf, entry->buffptr, read_bytes);
    if (cnbc > 0) read_bytes -= cnbc;

    mutex_unlock(&dev->mu);
    return read_bytes;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);

    if (count == 0) return 0;
    
    struct aesd_dev* dev = container_of(filp->private_data, struct aesd_dev, cdev);
    if (!dev) return 0;

    ssize_t retval = mutex_lock_interruptible(&dev->mu);
    if (retval != 0) return -EINTR;

    char* re_buf = kmalloc(dev->curr_entry.size + count, GFP_KERNEL);
    if (!re_buf) {
        mutex_unlock(&dev->mu);    
        return -ENOMEM;
    }

    memcpy(re_buf, dev->curr_entry.buffptr, dev->curr_entry.size);
    retval = count - copy_from_user(re_buf + dev->curr_entry.size, buf, count);

    const char* old_buf = dev->curr_entry.buffptr;
    dev->curr_entry.buffptr = re_buf;
    dev->curr_entry.size += retval;
    if (old_buf) kfree(old_buf);

    if (dev->curr_entry.buffptr[dev->curr_entry.size - 1] == '\n') {
        old_buf = aesd_circular_buffer_add_entry(&dev->circ_buf, &dev->curr_entry);
        if (old_buf) kfree(old_buf);
        memset(&dev->curr_entry, 0, sizeof(struct aesd_buffer_entry));
    }

    mutex_unlock(&dev->mu);
    return retval;
}

struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    aesd_circular_buffer_init(&aesd_device.circ_buf);
    memset(&aesd_device.curr_entry, 0, sizeof(struct aesd_buffer_entry));
    mutex_init(&aesd_device.mu);

    result = aesd_setup_cdev(&aesd_device);
    if( result ) {
        mutex_destroy(&aesd_device.mu);
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);
    
    int rc = 0;
    while ((rc = mutex_trylock(&aesd_device.mu)) != 1);
    uint8_t entry_index;
    struct aesd_buffer_entry* entry;
    AESD_CIRCULAR_BUFFER_FOREACH(entry,&aesd_device.circ_buf,entry_index) {
         if (entry->buffptr != NULL) kfree(entry->buffptr);
    }
    if (entry->buffptr != NULL) kfree(aesd_device.curr_entry.buffptr);
    mutex_unlock(&aesd_device.mu);

    mutex_destroy(&aesd_device.mu);

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
