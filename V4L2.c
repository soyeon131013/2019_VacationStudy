#include <stdio.h>
#include <stdlib.h>  // exit
#include <string.h>  // strerror
#include <errno.h>  // errno
#include <fcntl.h>  // open
#include <unistd.h>  // close, select
#include <sys/mman.h>  // mmap
#include <sys/ioctl.h>  // ioctl
#include <sys/time.h> //select
#include <sys/types.h>
#include <sys/stat.h> //stat
#include <assert.h> //assert
#include <linux/videodev2.h>  //v4l2
#include <include/media/v4l2-mediabus.h>

#define CLEAR(x) memset(&(x), 0, sizeof(x))

struct buffer 
{
	void 	*start;
	size_t	 length;
};

static char			 *dev_name;
static int 		 	 fd = -1;
struct buffer		 *buffers;
static unsigned int	 nbuffers;
unsigned int		 loop_exit = 1;
static int			 frame_number = 0;

static int xioctl(int fd, int request, void *arg)
{
	int r;
		
	do r = ioctl(fd, request, arg);
	while(-1 == r && EINTR == errno);

	return r;
}

int open_device(void)
{
	struct stat sta;

	if(-1 == stat(dev_name, &sta))
	{
		fprintf(stderr, "Cannot identify %s\n", dev_name);
		exit(EXIT_FAILURE);
	}
	
	if(!S_ISCHR(sta.st_mode))
	{
		fprintf(stderr, "%s is no device\n", dev_name);
		exit(EXIT_FAILURE);
	}
	
	fd = open(dev_name, O_RDWR | O_NONBLOCK, 0);

	if(fd == -1)
	{
		fprintf(stderr, "Cannot open %s\n", dev_name);
		exit(EXIT_FAILURE);
	}
	return fd;
	printf("int open_device(void)\n");
}

void init_mmap(void)
{
	struct v4l2_requestbuffers req;
	struct v4l2_buffer buf;

	CLEAR(req);

	req.count	= 4;
	req.type	= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory	= V4L2_MEMORY_MMAP;

	if(-1 == xioctl(fd, VIDIOC_REQBUFS, &req))
	{
		if(EINVAL == errno)
		{
			perror("It is not support memory mapping");
			exit(EXIT_FAILURE);
		}
		else
		{
			perror("Requesting Buffer");
			exit(EXIT_FAILURE);
		}
	}

	if(req.count < 2)
	{
		fprintf(stderr, "Insufficient buffer memoryy on %s\n", dev_name);
		exit(EXIT_FAILURE);
	}

	buffers = (struct buffer *)calloc(req.count, sizeof(*buffers));
	if(!buffers)
	{
		perror("Out of memory");
		exit(EXIT_FAILURE);
	}

	for(nbuffers = 0; nbuffers < req.count; ++nbuffers)
	{
		CLEAR(buf);

		buf.type	= V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory	= V4L2_MEMORY_MMAP;
		buf.index	= nbuffers;

		if(-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf))
		{
			perror("Querying Buffer");
			exit(EXIT_FAILURE);
		}

		buffers[nbuffers].length	= buf.length;
		buffers[nbuffers].start
			= mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);

		if(MAP_FAILED == buffers[nbuffers].start)
		{
			perror("Memory Mapping");
			exit(EXIT_FAILURE);
		}
	}

	printf("int init_mmap(void)\n");
}

void init_device(void)
{
	struct v4l2_capability capa;
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;
	struct v4l2_format fmt;
	struct v4l2_input input;
	enum v4l2_mbus_type type;

	if(-1 == xioctl(fd, VIDIOC_QUERYCAP, &capa))
	{
		if(EINVAL == errno)
	 	{
	 		perror("It is no V4L2 device");
	 		exit(EXIT_FAILURE);
	 	}
	 	else
	 	{
	 		perror("Querying Capabilities");
	 		exit(EXIT_FAILURE);
	 	}
	 }

	if(!(capa.capabilities & V4L2_CAP_VIDEO_CAPTURE))
	{
		perror("It is no video capture device");	
		exit(EXIT_FAILURE);
	}

	printf( "Driver Caps:\n"
			"  Driver: \"%s\"\n"
			"  Card: \"%s\"\n"
			"  Bus: \"%s\"\n"
			"  Version: %d\n"
			"  Capabilities: %08x\n",
			capa.driver,
			capa.card,
			capa.bus_info,
			capa.version,
			capa.capabilities);

	CLEAR(input);
    if(xioctl(fd, VIDIOC_ENUMINPUT, &input) < 0)
	{
        perror("VIDIOC_ENUMINPUT ");
    }	

	printf("Name : %s\n", input.name);
	printf("Input type : 0x%x\n", input.type);
	printf("Audio set : 0x%x\n", input.audioset);
	printf("tuner : 0x%x\n", input.tuner);
	printf("std : 0x%llx\n", input.std);
	printf("status : 0x%x\n", input.status);
	printf("cap : 0x%x\n", input.capabilities);

	int index = 0;
	if(ioctl(fd, VIDIOC_S_INPUT, &index) == -1)
	{
        perror("VIDIOC_S_INPUT");
    }

	//image cropping
	CLEAR(cropcap);

	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if(-1 == xioctl(fd, VIDIOC_CROPCAP, &cropcap))
	{
		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	}

	CLEAR(fmt);

	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width		= 640;
	fmt.fmt.pix.height		= 480;
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV422P;
	fmt.fmt.pix.field		= V4L2_FIELD_ANY;

	char fourcc[6];
    strncpy(fourcc, (char *)&fmt.fmt.pix.pixelformat, 4);
	printf("pixel format: %s\n", fourcc);

	if(-1 == xioctl(fd, VIDIOC_S_FMT, &fmt))
	{
		perror("Setting Pixel Format");
		exit(EXIT_FAILURE);
	}
	
	type = V4L2_MBUS_CSI2;

	init_mmap();

	printf("static void init_device(void)\n");
}

void start_capturing(void)
{
	enum v4l2_buf_type type;
	struct v4l2_buffer buf;
	
	int i;

	for(i = 0; i < nbuffers; ++i)
	{
		CLEAR(buf);

		buf.type	= V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory	= V4L2_MEMORY_MMAP;
		buf.index	= i;

		if(-1 == xioctl(fd, VIDIOC_QBUF, &buf))
		{
			perror("Query BUffer");
			exit(EXIT_FAILURE);
		}
	}

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if(-1 == xioctl(fd, VIDIOC_STREAMON, &type))
	{
		perror("Start Capture");
		exit(EXIT_FAILURE);
	}

	printf("void start_capturing(void)\n");
}

void process_image(void *start, u_int32_t bytesused)
{
	frame_number++;
	char file[15];
//	int fp;

	printf("bytesused: %d\n", bytesused);

	sprintf(file, "test-%d.raw", frame_number);
	printf("make file %d\n", frame_number);
/*
	fp = open(file, O_RDWR | O_CREAT, 755);
	if(fp == -1)
	{
		perror("Process Image");
		exit(EXIT_FAILURE);
	}

	write(fp, start, bytesused);
	close(fp);
*/
	FILE *fp;
	fp = fopen(file, "w+");

	fwrite(start, bytesused, 1, fp);

	fflush(fp);
	fclose(fp);
	printf("void process_image(void *start, u_int32_t bytesused)\n");
}

int read_frame(void)
{
	struct v4l2_buffer buf;

	CLEAR(buf);

	buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;
	if(-1 == xioctl(fd, VIDIOC_DQBUF, &buf))
	{
		perror("Retrieving Frame");
		exit(EXIT_FAILURE);
	}
	assert(buf.index < nbuffers);

	process_image(buffers[buf.index].start, buf.bytesused);
	if(-1 == xioctl(fd, VIDIOC_QBUF, &buf))
	{
		perror("Query Buffer");
		exit(EXIT_FAILURE);
	}
	printf("int read_frame(void)\n");
	return 1;
}

void mainloop(void)
{
	fd_set fds;
	struct timeval tv;
	int r;

	while(loop_exit)
	{
		FD_ZERO(&fds);
		FD_SET(fd, &fds);

		tv.tv_sec	 = 2;
		tv.tv_usec	 = 0;

		r = select(fd+1, &fds, NULL, NULL, &tv);

		if(r == -1)
		{
			if(errno == EINTR)
				continue;
			perror("Waiting for Frame");
		}
		if(r == 0)
		{
			perror("Timeout");
			continue;
		}
		read_frame();
	}
	printf("void mainloop(void)\n");
}

void stop_capturing(void)
{
	enum v4l2_buf_type type;

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if(-1 == xioctl(fd, VIDIOC_STREAMOFF, &type))
	{
		perror("Stop Capture");
		exit(EXIT_FAILURE);
	}
	printf("void stop_capturing(void)\n");
}

void uninit_device(void)
{
	for(int i = 0; i < nbuffers; ++i)
	{
		if(-1 == munmap(buffers[i].start, buffers[i].length))
		{
			perror("Memory Unmapping");
			exit(EXIT_FAILURE);
		}
	}
	free(buffers);
	printf("void uninit_device(void)\n");
}

void close_device(void)
{
	if(-1 == close(fd))
	{
		perror("Close Device");
		exit(EXIT_FAILURE);
	}
	fd = -1;
	printf("void close_device(void)\n");
}

int main()
{
	printf("value: %x\n", v4l2_fourcc('B', 'G', '1', '0'));
	dev_name = "/dev/video0";
	
	printf("It is main\n");

	open_device();
	init_device();
	start_capturing();
	mainloop();
	stop_capturing();
	uninit_device();
	close_device();

	printf("main end\n");
}
 
