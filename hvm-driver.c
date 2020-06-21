#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/refcount.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/cdev.h>

#include "driver-api.h"

#define DEBUG_ENABLE 1
#define VERSION "0.1"
#define CHRDEV_NAME "hvm"

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Kyle C. Hale and Conghao Liu");
MODULE_DESCRIPTION("HVM delegator module");
MODULE_VERSION(VERSION);

#define ERROR(fmt, args...) printk(KERN_ERR "HVM-DEL: " fmt, ##args)
#define INFO(fmt, args...) printk(KERN_INFO "HVM-DEL: " fmt, ##args)

#if DEBUG_ENABLE==1
#define DEBUG(fmt, args...) printk(KERN_DEBUG "HVM-DEL: " fmt, ##args)
#else 
#define DEBUG(fmt, args...)
#endif

struct hvm_del_info {
    struct mutex lock;
    dev_t chrdev;
    refcount_t refcnt;
};

static struct hvm_del_info * glob_info = NULL;

static ssize_t
hvm_del_read (struct file * file,
        char __user * buffer,
        size_t length,
        loff_t * offset)
{
    DEBUG("Read\n");
    return -EINVAL;
}


static ssize_t
hvm_del_write (struct file * file,
        const char __user * buffer,
        size_t len,
        loff_t * offset)
{
    DEBUG("Write\n");
    return -EINVAL;
}


static int
hvm_del_open (struct inode * inode,
        struct file * file)
{
    DEBUG("Delegator OPEN\n");
    refcount_inc(&glob_info->refcnt);
    return 0;
}


static int
hvm_del_release (struct inode * inode,
        struct file * file)
{
    DEBUG("Delegator RELEASE\n");
    refcount_dec(&glob_info->refcnt);
    return 0;
}


static long
hvm_del_ioctl (struct file * file,
               unsigned int ioctl,
               unsigned long arg)
{
    void __user * argp = (void __user*)arg;
    struct hvm_del_args * user_args = NULL;
	struct hvm_del_args * args = NULL;
    const uint16_t port = HVM_DEL_MAGIC_PORT;
	size_t kernel_args_phys_addr = 0;
	size_t path_len = 0;

	volatile ssize_t retval = 0;

	char *buf = NULL;

    INFO("IOCTL %d\n", ioctl);

    user_args = kzalloc(sizeof(*user_args), GFP_KERNEL);
    if (copy_from_user(user_args, argp, sizeof(*user_args)) != 0) {
        ERROR("Could not copy request from user\n");
		kfree(user_args);
        return -EFAULT;
    }

	args = kzalloc(sizeof(*args), GFP_KERNEL);
	kernel_args_phys_addr = virt_to_phys(args);

	INFO("hvm_del_req 0x%X 0x%016lX\n", ioctl, kernel_args_phys_addr);

	switch (ioctl) {
	case HVM_DEL_REQ_OPEN:
		path_len = strnlen_user((char __user*)user_args->arg1, HOST_MAX_PATH);
		if (path_len == 0) {
			ERROR("Could not determine length of host filepath\n");
			kfree(user_args);
			kfree(args);
			return -EFAULT;
		} else if (path_len > HOST_MAX_PATH) {
			ERROR("Host filepath was too long\n");
			kfree(user_args);
			kfree(args);
			return -ENAMETOOLONG;
		}

		buf = kzalloc(path_len, GFP_KERNEL);
		if (copy_from_user((char __user*)user_args->arg1, buf, path_len) != 0) {
			ERROR("Could not copy host filepath from user\n");
			kfree(user_args);
			kfree(args);
			kfree(buf);
			return -EFAULT;
		}

		args->arg1 = (size_t) buf;
		args->arg2 = user_args->arg2;
		args->arg3 = user_args->arg3;
		break;

	case HVM_DEL_REQ_READ:
		buf = kzalloc(user_args->arg3, GFP_KERNEL);
		
		args->arg1 = user_args->arg1;
		args->arg2 = (size_t) buf;
		args->arg3 = user_args->arg3;
		break;

	case HVM_DEL_REQ_WRITE:
		buf = kzalloc(user_args->arg3, GFP_KERNEL);
		if (copy_from_user((void __user*)user_args->arg2, buf, user_args->arg3) != 0) {
			ERROR("Could not copy buffer contents from user\n");
			kfree(user_args);
			kfree(args);
			kfree(buf);
		}
		
		args->arg1 = user_args->arg1;
		args->arg2 = (size_t) buf;
		args->arg3 = user_args->arg3;
		break;

	case HVM_DEL_REQ_CLOSE:
		args->arg1 = user_args->arg1;
		break;

	default:
		return -EINVAL;
	}

	asm volatile (
		"movq %1, %%r8;"
		"out %2, %3;"
		"movq %%r8, %0;"

		: "=r" (retval)
		: "r" (kernel_args_phys_addr), "a" (ioctl), "d" (port)
		: "%r8"
	);

	switch (ioctl) {

	case HVM_DEL_REQ_OPEN:
		kfree(buf);
		break;

	case HVM_DEL_REQ_READ:
		if (copy_to_user((void __user*)user_args->arg2, buf, args->arg3) != 0) {
			ERROR("Could not copy buffer contents to user\n");
			retval = -EFAULT;
		}
		kfree(buf);
		break;
	
	case HVM_DEL_REQ_WRITE:
		kfree(buf);
		break;

	case HVM_DEL_REQ_CLOSE:
		// TODO: Some kind of host-to-guest errno
		break;
	
	default:
		break;
	}

	kfree(user_args);
	kfree(args);

    return retval;
}


static struct file_operations fops = {
    .read           = hvm_del_read,
    .write          = hvm_del_write,
    .unlocked_ioctl = hvm_del_ioctl,
    .open           = hvm_del_open,
    .release        = hvm_del_release,
};


static int __init
hvm_del_init (void)
{
    INFO("HVM Delegator module initializing\n");

    glob_info = kzalloc(sizeof(*glob_info), GFP_KERNEL);
    if (!glob_info) {
        ERROR("Could not allocate global info\n");
        return -1;
    }

    mutex_init(&glob_info->lock);

    refcount_set(&glob_info->refcnt, 1);

    glob_info->chrdev = register_chrdev(244, CHRDEV_NAME, &fops);

    INFO("<maj,min> = <%u,%u>\n", MAJOR(glob_info->chrdev), MINOR(glob_info->chrdev));

    return 0;
}


static void __exit
hvm_del_exit (void)
{
    INFO("HVM Delegator module cleaning up\n");
    unregister_chrdev(MAJOR(glob_info->chrdev), CHRDEV_NAME);
    mutex_destroy(&glob_info->lock);
    kfree(glob_info);
}

module_init(hvm_del_init);
module_exit(hvm_del_exit);
