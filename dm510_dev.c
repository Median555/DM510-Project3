/* Prototype module for third mandatory DM510 assignment */
#ifndef __KERNEL__
#  define __KERNEL__
#endif
#ifndef MODULE
#  define MODULE
#endif

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <asm/uaccess.h>
#include <linux/semaphore.h>
#include <linux/cdev.h>
/* #include <asm/system.h> */
#include <asm/switch_to.h>

/* Prototypes - this would normally go in a .h file */
static int dm510_open( struct inode*, struct file* );
static int dm510_release( struct inode*, struct file* );
static ssize_t dm510_read( struct file*, char*, size_t, loff_t* );
static ssize_t dm510_write( struct file*, const char*, size_t, loff_t* );
long dm510_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);

#define DEVICE_NAME "dm510_dev" /* Dev name as it appears in /proc/devices */
#define MAJOR_NUMBER 254
#define MIN_MINOR_NUMBER 0
#define MAX_MINOR_NUMBER 1

#define DEVICE_COUNT 2
/* end of what really should have been in a .h file */

struct dm510_dev {
	struct dm510_buffer* read_buffer;
	struct dm510_buffer* write_buffer;
	int size;
	struct cdev cdev;
};

struct dm510_buffer {
	char *buffer;
	int index;
	int size;
};

struct dm510_buffer* buffer1;
struct dm510_buffer* buffer2;

struct dm510_dev *devs;

/* file operations struct */
static struct file_operations dm510_fops = {
	.owner   = THIS_MODULE,
	.read    = dm510_read,
	.write   = dm510_write,
	.open    = dm510_open,
	.release = dm510_release,
    .unlocked_ioctl   = dm510_ioctl
};

/* called when module is loaded */
int dm510_init_module( void ) {

	// TODO make function... setup_buffers()
	buffer1 = (struct dm510_buffer *) kmalloc(sizeof(struct dm510_buffer), GFP_KERNEL);
	buffer2 = (struct dm510_buffer *) kmalloc(sizeof(struct dm510_buffer), GFP_KERNEL);

	buffer1->buffer = (char *) kmalloc(sizeof(char) * 100, GFP_KERNEL);
	buffer2->buffer = (char *) kmalloc(sizeof(char) * 100, GFP_KERNEL);

	buffer1->size = 100;
	buffer2->size = 100;

	buffer1->index = 0;
	buffer2->index = 0;

	/* initialization code belongs here */

	devs = (struct dm510_dev*)kmalloc(2 * sizeof(struct dm510_dev), GFP_KERNEL);

	/* kmalloc buffers */

	int devno = MKDEV(MAJOR_NUMBER, MIN_MINOR_NUMBER);
	printk("devno: %d\n", devno);

	printk("%d\n", register_chrdev_region(devno, 1, "dm510-0"));

	devs[0].read_buffer = buffer1;
	devs[0].write_buffer = buffer2;

	cdev_init(&devs[0].cdev, &dm510_fops);
	devs[0].cdev.owner = THIS_MODULE;
	devs[0].cdev.ops = &dm510_fops;
	cdev_add(&devs[0].cdev, devno, 1); //TODO: error checking


	devno = MKDEV(MAJOR_NUMBER, MAX_MINOR_NUMBER);
	printk("devno: %d\n", devno);

	printk("%d\n", register_chrdev_region(devno, 1, "dm510-1"));

	devs[1].read_buffer = buffer2;
	devs[1].write_buffer = buffer1;

	cdev_init(&devs[1].cdev, &dm510_fops);
	devs[1].cdev.owner = THIS_MODULE;
	devs[1].cdev.ops = &dm510_fops;
	cdev_add(&devs[1].cdev, devno, 1); //TODO: error checking

	printk(KERN_INFO "DM510: Hello from your device!\n");
	//printk(KERN_INFO "dev0 %p, cdev %p\n", &devs[0], &devs[0].cdev);
	return 0;
}

/* Called when module is unloaded */
void dm510_cleanup_module( void ) {

	/* kfree buffers */
	kfree(buffer1->buffer);
	kfree(buffer2->buffer);

	kfree(buffer1);
	kfree(buffer2);


	unregister_chrdev_region(devs[0].cdev.dev, 1);
	cdev_del(&devs[0].cdev);

	unregister_chrdev_region(devs[1].cdev.dev, 1);
	cdev_del(&devs[1].cdev);

	/* clean up code belongs here */

	//void unregister_chrdev_region(dev_t first, unsigned int count);

	printk(KERN_INFO "DM510: Module unloaded.\n");
}


/* Called when a process tries to open the device file */
static int dm510_open( struct inode *inode, struct file *filp ) {

	/* device claiming code belongs here */
	struct dm510_dev *dev;
	dev = container_of(inode->i_cdev, struct dm510_dev, cdev);
	filp->private_data = dev;

	//int minor = MINOR(dev->cdev.dev);


	return 0;
}


/* Called when a process closes the device file. */
static int dm510_release( struct inode *inode, struct file *filp ) {

	/* device release code belongs here */


	return 0;
}

/* Called when a process, which already opened the dev file, attempts to read from it. */
static ssize_t dm510_read( struct file *filp,
    char *buf,      /* The buffer to fill with data     */
    size_t count,   /* The max number of bytes to read  */
    loff_t *f_pos )  /* The offset in the file           */
{

	struct dm510_dev *dev = filp->private_data;
	printk(KERN_INFO "dev0 %p, cdev %p\n", dev, &dev->cdev);
	//printk(KERN_INFO "Message is \"%s\" with length %d\n", dev->read_buffer, dev->size);

	int max_read = min(count, dev->read_buffer->index);

	copy_to_user(buf, dev->read_buffer->buffer, max_read);

	char * new_buffer = kmalloc(sizeof(char) * dev->read_buffer->size, GFP_KERNEL);

	memcpy(new_buffer, dev->read_buffer->buffer + max_read, dev->read_buffer->size - max_read);

	dev->read_buffer->buffer = new_buffer;
	dev->read_buffer->index -= max_read;

	*f_pos += max_read;
	return max_read; //return number of bytes read
}


/* Called when a process writes to dev file */
static ssize_t dm510_write( struct file *filp,
    const char *buf,/* The buffer to get data from      */
    size_t count,   /* The max number of bytes to write */
    loff_t *f_pos )  /* The offset in the file           */
{

	struct dm510_dev *dev = filp->private_data;

	// Check that we have enough space in the buffer
	if (dev->write_buffer->size - dev->write_buffer->index < count)
	{
		// TODO: proper error code
		return -1;
	}

	copy_from_user(dev->write_buffer->buffer + dev->write_buffer->index, buf, count);

	*f_pos += count;

	dev->write_buffer->index += count;

	return count; //return number of bytes written
}

/* called by system call icotl */
long dm510_ioctl(
    struct file *filp,
    unsigned int cmd,   /* command passed from the user */
    unsigned long arg ) /* argument of the command */
{
	/* ioctl code belongs here */
	printk(KERN_INFO "DM510: ioctl called.\n");

	return 0; //has to be changed
}

module_init( dm510_init_module );
module_exit( dm510_cleanup_module );

MODULE_AUTHOR( "Kristoffer Abell, Steffen Klenow, Mikkel Pilegaard" );
MODULE_LICENSE( "GPL" );
