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
#include "aesd_ioctl.h"

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
    PDEBUG("Release the aesdchar device");

    // nothing to do here?
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    
    ssize_t retval = 0;
    struct aesd_dev* dev = filp->private_data;
    if (!dev) return -EFAULT;

    retval = mutex_lock_interruptible(&dev->mu);
    if (retval != 0) return -EINTR;

    size_t offset;
    struct aesd_buffer_entry* entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->circ_buf, *f_pos, &offset);
    if (!entry) goto finalize_read;
    
    retval = (ssize_t)entry->size - (ssize_t)offset;
    if (retval <= 0) {
        retval = -EFAULT;
        goto finalize_read;
    }

    unsigned long cnbc = copy_to_user(buf, entry->buffptr + offset, retval);
    retval -= cnbc;
    if (retval <= 0) {
        retval = -EFAULT;
        goto finalize_read;
    }
    *f_pos += retval;

finalize_read:
    mutex_unlock(&dev->mu);
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    PDEBUG("Write %zu bytes to aesdchar device", count);

    ssize_t retval = 0;
    if (count == 0) return 0;
    
    struct aesd_dev* dev = filp->private_data;
    if (!dev) return -EFAULT;

    retval = mutex_lock_interruptible(&dev->mu);
    if (retval != 0) return -EINTR;

    char* re_buf = kmalloc(dev->curr_entry.size + count, GFP_KERNEL);
    if (!re_buf) {
        retval = -ENOMEM;
        goto finalize_write;
    }
    memset(re_buf, '\0', dev->curr_entry.size + count);

    if (dev->curr_entry.size > 0) memcpy(re_buf, dev->curr_entry.buffptr, dev->curr_entry.size);
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

finalize_write:
    mutex_unlock(&dev->mu);
    return retval;
}

loff_t aesd_llseek(struct file* filp, loff_t offset, int whence)
{
    PDEBUG("Calling llseek");

    struct aesd_dev* dev = filp->private_data;
    if (mutex_lock_interruptible(&dev->mu)) {
        printk(KERN_ERR "llseek: locking device interrupted");
        return -EINTR;
    }

    loff_t buf_size = 0;
    uint8_t entry_index;
    struct aesd_buffer_entry* entry;
    AESD_CIRCULAR_BUFFER_FOREACH(entry, &aesd_device.circ_buf, entry_index) {
         if (entry->buffptr != NULL) buf_size += entry->size;
    }
    loff_t retval = fixed_size_llseek(filp, offset, whence, buf_size);

    mutex_unlock(&dev->mu);
    return retval;
}

long aesd_ioctl(struct file* filp, unsigned int cmd, unsigned long data)
{
    if (cmd != AESDCHAR_IOCSEEKTO) return -ENOTTY;

    PDEBUG("IOCTL command AESDCHAR_IOCSEEKTO received");

    struct aesd_seekto seekto_data;
    if(copy_from_user(&seekto_data, (const void*)data, sizeof(struct aesd_seekto)) > 0) {
        printk("ioctl: failed to copy input argument from user space");
        return -EFAULT;
    }

    struct aesd_dev* dev = filp->private_data;
    if (mutex_lock_interruptible(&dev->mu)) {
        printk("Couldn't lock device: interruption requested");
        return -EINTR;
    }

    long retval = -EINVAL;
    loff_t new_f_pos = 0;
    loff_t n_items = dev->circ_buf.full
        ? AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED
        : dev->circ_buf.in_offs - dev->circ_buf.out_offs;
    if (seekto_data.write_cmd >= n_items) goto ioctl_unlock_device;

    int i;
    for (i = 0; i < seekto_data.write_cmd; ++i) {
        const loff_t pos = (dev->circ_buf.out_offs + i) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
        new_f_pos += dev->circ_buf.entry[pos].size;
    }
    const loff_t pos = (dev->circ_buf.out_offs + i) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    if (dev->circ_buf.entry[pos].size <= seekto_data.write_cmd_offset) goto ioctl_unlock_device;

    new_f_pos += seekto_data.write_cmd_offset;
    retval = 0;

ioctl_unlock_device:
    mutex_unlock(&dev->mu);
    if (retval) return retval;

    filp->f_pos = new_f_pos;
    return retval;
}

struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
    .llseek =   aesd_llseek,
    .unlocked_ioctl = aesd_ioctl,
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
    while ((rc = mutex_trylock(&aesd_device.mu)) != 1); // waiting for operations to finish (is it necessary?)
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
