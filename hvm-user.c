#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <linux/types.h>

#include "driver-api.h"

#define BUF_LEN 64

int main (int argc, char ** argv) 
{
    int driver_fd = open("/dev/hvm", O_RDWR);
    if (driver_fd < 0) {
        fprintf(stderr, "Could not open hvm-del device: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    struct hvm_del_args open_args = {
		(size_t) "/etc/hostname",
		O_RDONLY,
		0,
	};

    puts("Opening host file descriptor");
    int host_fd = ioctl(driver_fd, HVM_DEL_REQ_OPEN, &open_args);

    if (host_fd < 0) {
        fprintf(stderr, "Ioctl failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

	char buf[BUF_LEN+1] = {0};
	struct hvm_del_args read_args = {
		host_fd,
		(size_t) buf,
		BUF_LEN,
	};

	puts("Reading from host fd");
	int nbytes = ioctl(driver_fd, HVM_DEL_REQ_READ, &read_args);

	if (nbytes < 0) {
		fprintf(stderr, "Ioctl failed: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	puts(buf);

	struct hvm_del_args close_args = {
		host_fd,
	};

	puts("Closing host fd");
	int ret = ioctl(driver_fd, HVM_DEL_REQ_CLOSE, &close_args);

	if (ret != 0) {
		fprintf(stderr, "Ioctl failed: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

    close(driver_fd);

    return 0;
}
