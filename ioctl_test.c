#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>




//#define DM510_IOCBUFSIZE _IO(DM510_IOC_MAGIC,  1) // == 28673
//#define DM510_IOCNOREADERS _IO(DM510_IOC_MAGIC,  2) // == 28674

/*int main(int argc, char *argv[])
{
	int fd = open("/dev/dm510-0", O_RDWR);
	//ioctl(fd, 28673, buffer_size);
	return ioctl(fd, 28673, argv[1]);
}*/

int main(int argc, char *argv[])
{
	int fd = open("/dev/dm510-0", O_RDWR);
	int buffer_size = 1;
	//ioctl(fd, 28673, buffer_size);
	printf("set buffer size result: %d\n", ioctl(fd, 28673, buffer_size));
	int no_readers = 10;
	printf("set readers result: %d\n", ioctl(fd, 28674, no_readers));
	return 0;
}
