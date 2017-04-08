#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>



#define DM510_IOC_MAGIC 'p'
#define DM510_IOC_MAXNO 2
#define DM510_IOCBUFSIZE _IO(DM510_IOC_MAGIC,  1)
#define DM510_IOCNOREADERS _IO(DM510_IOC_MAGIC,  2)


int main(int argc, char *argv[])
{
	int fd = open("/dev/dm510-0", O_RDWR);
	int buffer_size = 5000;
	ioctl(fd, DM510_IOCBUFSIZE, buffer_size);
	int no_readers = 10;
	ioctl(fd, DM510_IOCNOREADERS, no_readers);
	return 0;
}
