#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

void testNoWriters()
{
	int i;
	int no_writers = 1;
	int extra_writers = 3;
	int no_opend = 0;

	int fileDescriptors[no_writers];
	for (i = 0; i < (no_writers + extra_writers); i++)
	{
		fileDescriptors[i] = open("/dev/dm510-0", O_WRONLY);
		if (fileDescriptors[i] >= 0)
		{
			no_opend++;
		}
	}

	for (i = 0; i < (no_writers + extra_writers); i++)
	{
		if (fileDescriptors[i] >= 0)
		{
			close(fileDescriptors[i]);
		}
	}

	if (no_opend == no_writers)
	{
		printf("%-20s -> success\n","no writers");
	}
	else
	{
		printf("%-20s -> fail\n","no writers");
	}
}

void testNoReaders()
{
	int fd = open("/dev/dm510-0", O_RDWR);
	int no_readers = 5;
	ioctl(fd, 28674, no_readers); // set the number of readers
	close(fd);

	int i;
	int extra_readers = 3;
	int no_opend = 0;

	// Open the file for each reader that is allowed + extra readers
	// The module should disallow additional readers
	int fileDescriptors[no_readers];
	for (i = 0; i < (no_readers + extra_readers); i++)
	{
		fileDescriptors[i] = open("/dev/dm510-0", O_RDONLY); // TODO: flag is wrong
		// printf("file open result %d\n", fileDescriptors[i]);
		if (fileDescriptors[i] >= 0)
		{
			no_opend++;
		}
	}

	//
	for (i = 0; i < (no_readers + extra_readers); i++)
	{
		if (fileDescriptors[i] >= 0)
		{
			close(fileDescriptors[i]);
		}
	}

	if (no_opend == no_readers)
	{
		printf("%-20s -> success\n","no readers");
	}
	else
	{
		printf("%-20s -> fail\n","no readers");
	}
}

void readZero()
{
	int fd = open("/dev/dm510-0", O_RDWR);
	char *buf;
	int count = 0;
	int result = read(fd, buf, count);

	close(fd);
	if (result < 0)
	{
		printf("%-20s -> success\n","read nothing");
	}
	else
	{
		printf("%-20s -> fail\n","write nothing");
	}
}

void writeZero()
{
	int fd = open("/dev/dm510-0", O_RDWR);
	char *buf;
	int count = 0;
	int result = write(fd, buf, count);
	close(fd);

	if (result < 0)
	{
		printf("%-20s -> success\n","write nothing ");
	}
	else
	{
		printf("%-20s -> fail\n","write nothing");
	}
}

void writeWhenFull()
{
	int i;
	int fd = open("/dev/dm510-0", O_RDWR);
	int buffer_size = 10;
	ioctl(fd, 28673, buffer_size); // Set the size of the buffers
	close(fd);

	int fd2 = open("/dev/dm510-0", O_RDWR | O_NONBLOCK);
	char *buf = "123456789";
	int count = 10;
	int result = 0;

	for (i = 0; i < 2; i++)
	{
		result = write(fd2, buf, count);
	}

	close(fd2);

	if (result < 0)
	{
		printf("%-20s -> success\n","write when full");
	}
	else
	{
		printf("%-20s -> fail\n","write when full");
	}
}

void emptyBuffers()
{
	int fd = open("/dev/dm510-0", O_RDWR | O_NONBLOCK);
	char *buf;
	int count = 10;
	int result = 0;

	while (result >= 0)
	{
		result = read(fd, buf, count);
	}
	close(fd);

	fd = open("/dev/dm510-1", O_RDWR | O_NONBLOCK);
	result = 0;
	while (result >= 0)
	{
		result = read(fd, buf, count);
	}
	close(fd);
}

int main(int argc, char *argv[])
{
	// int fd = open("/dev/dm510-0", O_RDWR);
	// int buffer_size = 1;
	//ioctl(fd, 28673, buffer_size);
	// printf("set buffer size result: %d\n", ioctl(fd, 28673, buffer_size));
	// int no_readers = 10;
	// printf("set readers result: %d\n", ioctl(fd, 28674, no_readers));
	// // close(fd);
	emptyBuffers();
	testNoWriters();
	testNoReaders();
	readZero();
	writeZero();
	writeWhenFull();
	emptyBuffers();
	return 0;
}
