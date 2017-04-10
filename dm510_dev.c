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
#include <linux/uaccess.h>
#include <linux/semaphore.h>
#include <linux/cdev.h>
#include <asm/switch_to.h>
#include <linux/wait.h>

#include <linux/sched.h>


#define DM510_IOC_MAGIC 'p'
#define DM510_IOC_MAXNO 2
#define DM510_IOCBUFSIZE _IO(DM510_IOC_MAGIC,  1)
#define DM510_IOCNOREADERS _IO(DM510_IOC_MAGIC,  2)

#define init_MUTEX(LOCKNAME) sema_init(LOCKNAME,1) // From scull_pipe

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
	struct semaphore sem;
	wait_queue_head_t read_wait_queue;
	wait_queue_head_t write_wait_queue;
	int no_readers;
	int no_writers;
};

struct dm510_buffer* buffer1;
struct dm510_buffer* buffer2;

struct dm510_dev *devs;

int BUFFER_SIZE = 100;
int NO_READERS = 10;
int NO_WRITERS = 1;

/* file operations struct */
static struct file_operations dm510_fops = {
	.owner   = THIS_MODULE,
	.read    = dm510_read,
	.write   = dm510_write,
	.open    = dm510_open,
	.release = dm510_release,
    .unlocked_ioctl   = dm510_ioctl
};

void free_buffers(void)
{
	kfree(buffer1->buffer);
	kfree(buffer2->buffer);

	kfree(buffer1);
	kfree(buffer2);
}

int setup_buffers(void)
{
	buffer1 = (struct dm510_buffer *) kmalloc(sizeof(struct dm510_buffer), GFP_KERNEL);
	if (!buffer1) // Allocation failed
	{
		return -ENOMEM;
	}

	buffer1->buffer = (char *) kmalloc(sizeof(char) * BUFFER_SIZE, GFP_KERNEL);
	if (!buffer1->buffer) // Allocation failed
	{
		kfree(buffer1);
		return -ENOMEM;
	}
	buffer1->size = BUFFER_SIZE;
	buffer1->index = 0;

	init_MUTEX(&buffer1->sem);
	init_waitqueue_head(&buffer1->read_wait_queue);
	init_waitqueue_head(&buffer1->write_wait_queue);


	buffer2 = (struct dm510_buffer *) kmalloc(sizeof(struct dm510_buffer), GFP_KERNEL);
	if (!buffer2) // Allocation failed
	{
		kfree(buffer1->buffer);
		kfree(buffer1);
		return -ENOMEM;
	}
	buffer2->buffer = (char *) kmalloc(sizeof(char) * BUFFER_SIZE, GFP_KERNEL);
	if (!buffer2->buffer) // Allocation failed
	{
		kfree(buffer1->buffer);
		kfree(buffer1);
		kfree(buffer2);
		return -ENOMEM;
	}
	buffer2->size = BUFFER_SIZE;
	buffer2->index = 0;

	init_MUTEX(&buffer2->sem);
	init_waitqueue_head(&buffer2->read_wait_queue);
	init_waitqueue_head(&buffer2->write_wait_queue);

	return 0;
}

/* called when module is loaded */
int dm510_init_module( void )
{
	int setup = setup_buffers();
	if(setup < 0)
	{
		return setup;
	}
	buffer1->no_readers = 0;
	buffer1->no_writers = 0;
	buffer2->no_readers = 0;
	buffer2->no_writers = 0;

	devs = (struct dm510_dev*)kmalloc(2 * sizeof(struct dm510_dev), GFP_KERNEL);
	if (!devs) // Allocation failed
	{
		return -ENOMEM;
	}

	int devno = MKDEV(MAJOR_NUMBER, MIN_MINOR_NUMBER);

	int register_result = register_chrdev_region(devno, 1, "dm510-0");
	if (register_result < 0) // register failed
	{
		free_buffers();
		kfree(devs);
		return -EPERM;
	}

	devs[0].read_buffer = buffer1;
	devs[0].write_buffer = buffer2;

	cdev_init(&devs[0].cdev, &dm510_fops);
	devs[0].cdev.owner = THIS_MODULE;
	devs[0].cdev.ops = &dm510_fops;

	int add_result = cdev_add(&devs[0].cdev, devno, 1); //TODO: error checking
	if (add_result < 0)
	{
		free_buffers();
		kfree(devs);
		unregister_chrdev_region(devs[0].cdev.dev, 1);

		return -EPERM;
	}

	devno = MKDEV(MAJOR_NUMBER, MAX_MINOR_NUMBER);

	register_result = register_chrdev_region(devno, 1, "dm510-1");
	if (register_result < 0) // register failed
	{
		free_buffers();
		kfree(devs);
		unregister_chrdev_region(devs[0].cdev.dev, 1);
		cdev_del(&devs[0].cdev);
		return -EPERM;
	}

	devs[1].read_buffer = buffer2;
	devs[1].write_buffer = buffer1;

	cdev_init(&devs[1].cdev, &dm510_fops);
	devs[1].cdev.owner = THIS_MODULE;
	devs[1].cdev.ops = &dm510_fops;
	add_result = cdev_add(&devs[1].cdev, devno, 1);
	if (add_result < 0)
	{
		free_buffers();
		kfree(devs);
		unregister_chrdev_region(devs[0].cdev.dev, 1);
		cdev_del(&devs[0].cdev);
		unregister_chrdev_region(devs[1].cdev.dev, 1);
		return -EPERM;
	}

	return 0;
}

/* Called when module is unloaded */
void dm510_cleanup_module( void )
{
	// Free the buffers
	free_buffers();

	unregister_chrdev_region(devs[0].cdev.dev, 1);
	cdev_del(&devs[0].cdev);

	unregister_chrdev_region(devs[1].cdev.dev, 1);
	cdev_del(&devs[1].cdev);

	// De-allocate the our device structs
	kfree(devs);
}

// sets the number of readers and writers, accroding to diff
int set_readers_and_writers(struct inode *inode, struct file *filp, int diff)
{
	struct dm510_dev *dev;
	dev = container_of(inode->i_cdev, struct dm510_dev, cdev);
	filp->private_data = dev;

	if (filp->f_mode & (FMODE_READ | FMODE_WRITE))
	{
		if (down_interruptible(&dev->read_buffer->sem))
		{
			return -ERESTARTSYS;
		}

		if (down_trylock(&dev->write_buffer->sem))
		{
			up(&dev->read_buffer->sem);
			return -ERESTARTSYS;
		}

		// Deny access if already at max readers or max writers
		if (dev->read_buffer->no_readers + diff > NO_READERS ||
			dev->write_buffer->no_writers + diff > NO_WRITERS)
		{
			up(&dev->read_buffer->sem);
			up(&dev->write_buffer->sem);

			return -EAGAIN;
		}
		else
		{
			dev->read_buffer->no_readers += diff;
			dev->write_buffer->no_writers += diff;

			up(&dev->read_buffer->sem);
			up(&dev->write_buffer->sem);
		}
	}
	else if (filp->f_mode & FMODE_READ)
	{
		if (down_interruptible(&dev->read_buffer->sem))
		{
			return -ERESTARTSYS;
		}

		// Deny access if already at max readers
		if (dev->read_buffer->no_readers + diff > NO_READERS)
		{
			up(&dev->read_buffer->sem);

			return -EAGAIN;
		}
		else
		{
			dev->read_buffer->no_readers += diff;
			up(&dev->read_buffer->sem);
		}
	}
	else if (filp->f_mode & FMODE_WRITE)
	{
		if (down_interruptible(&dev->write_buffer->sem))
		{
			return -ERESTARTSYS;
		}

		// Deny access if already at max writers
		if (dev->write_buffer->no_writers + diff > NO_WRITERS)
		{
			up(&dev->write_buffer->sem);

			return -EAGAIN;
		}
		else
		{
			dev->write_buffer->no_writers += diff;
			up(&dev->write_buffer->sem);
		}
	}

	return 0;
}

/* Called when a process tries to open the device file */
static int dm510_open( struct inode *inode, struct file *filp )
{
	return set_readers_and_writers(inode, filp, 1);
}


/* Called when a process closes the device file. */
static int dm510_release( struct inode *inode, struct file *filp )
{
	return set_readers_and_writers(inode, filp, -1);
}

/* Called when a process, which already opened the dev file, attempts to read from it. */
static ssize_t dm510_read( struct file *filp,
    char *buf,      /* The buffer to fill with data     */
    size_t count,   /* The max number of bytes to read  */
    loff_t *f_pos )  /* The offset in the file           */
{
	if (count < 1)
	{
		return -EPERM;
	}

	struct dm510_dev *dev = filp->private_data;

	if (down_interruptible(&dev->read_buffer->sem))
	{
		return -ERESTARTSYS;
	}

	while (dev->read_buffer->index == 0) // Buffer is empty
	{
		up(&dev->read_buffer->sem); // Release the lock
		if (filp->f_flags & O_NONBLOCK)
		{
			return -EAGAIN;
		}

		if (wait_event_interruptible(dev->read_buffer->read_wait_queue, (dev->read_buffer->index > 0)))
		{
			return -ERESTARTSYS; // Restart the system call
		}

		if (down_interruptible(&dev->read_buffer->sem))
		{
			return -ERESTARTSYS;
		}
	}

	int max_read = min(count, dev->read_buffer->index);

	// Access check, read from buffer
	if (!access_ok(VERIFY_WRITE, buf, max_read))
	{
		up(&dev->read_buffer->sem);
		return -EACCES;
	}

	char *new_buffer = kmalloc(sizeof(char) * dev->read_buffer->size, GFP_KERNEL);
	memcpy(new_buffer, dev->read_buffer->buffer + max_read, dev->read_buffer->size - max_read);

	char *old_buffer = dev->read_buffer->buffer;
	dev->read_buffer->buffer = new_buffer;
	dev->read_buffer->index -= max_read;

	up(&dev->read_buffer->sem);
	wake_up_interruptible(&dev->read_buffer->write_wait_queue);

	int copy_res = copy_to_user(buf, old_buffer, max_read);

	kfree(old_buffer);

	return max_read - copy_res; //return number of bytes read
}


/* Called when a process writes to dev file */
static ssize_t dm510_write( struct file *filp,
    const char *buf,/* The buffer to get data from      */
    size_t count,   /* The max number of bytes to write */
    loff_t *f_pos )  /* The offset in the file           */
{
	if (count < 1)
	{
		return -EPERM;
	}
	struct dm510_dev *dev = filp->private_data;

	if (down_interruptible(&dev->write_buffer->sem))
	{
		return -ERESTARTSYS;
	}

	// if non-blocking and buffer is full then leave
	while (dev->write_buffer->index == dev->write_buffer->size)
	{
		up(&dev->write_buffer->sem); // Release the lock

		if (filp->f_flags & O_NONBLOCK)
		{
			// Cancel operation if non-blocking
			return -EAGAIN;
		}

		if (wait_event_interruptible(dev->write_buffer->write_wait_queue, (dev->write_buffer->index < dev->write_buffer->size)))
		{
			return -ERESTARTSYS;
		}

		if (down_interruptible(&dev->write_buffer->sem))
		{
			return -ERESTARTSYS;
		}
	}

	int max_write = min(count, dev->write_buffer->size - dev->write_buffer->index);

	// Access check, read from buffer
	if (!access_ok(VERIFY_READ, buf, max_write))
	{
		up(&dev->write_buffer->sem);
		return -EACCES;
	}

	int copy_res = copy_from_user(dev->write_buffer->buffer + dev->write_buffer->index, buf, max_write);

	// copy_res is the number of chars not copied to user
	dev->write_buffer->index += max_write - copy_res;

	up(&dev->write_buffer->sem); // Release the lock
	wake_up_interruptible(&dev->write_buffer->read_wait_queue);

	return max_write - copy_res; //return number of bytes written
}

/* called by system call icotl */
long dm510_ioctl(
    struct file *filp,
    unsigned int cmd,   /* command passed from the user */
    unsigned long arg ) /* argument of the command */
{

	if (_IOC_TYPE(cmd) != DM510_IOC_MAGIC) return -ENOTTY;
	if (_IOC_NR(cmd) > DM510_IOC_MAXNO) return -ENOTTY;

	switch(cmd)
	{
		case DM510_IOCBUFSIZE:
			if (arg < 1)
			{
				return -EPERM;
			}
			BUFFER_SIZE = arg;
			free_buffers();
			setup_buffers();
			break;
		case DM510_IOCNOREADERS:
			if (arg < 1)
			{
				return -EPERM;
			}
			NO_READERS = arg;
			break;
		default:
	  		return -ENOTTY;
	}

	return 0;
}

module_init( dm510_init_module );
module_exit( dm510_cleanup_module );

MODULE_AUTHOR( "Kristoffer Abell, Steffen Klenow, Mikkel Pilegaard" );
MODULE_LICENSE( "GPL" );
