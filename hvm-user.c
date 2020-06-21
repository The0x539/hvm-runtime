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

#define BUF_LEN 512

int main (int argc, char ** argv) {
	if (argc == 1) {
		return 0;
	}

    int driver_fd = open("/dev/hvm", O_RDWR);
    if (driver_fd < 0) {
        fprintf(stderr, "Could not open hvm-del device: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

	char *filename = argv[1];

    struct hvm_del_req open_req = {
		HVM_DEL_REQ_OPEN,
		(size_t) filename,
		O_RDONLY,
		0,
	};

    //puts("Opening host file descriptor");
    int host_fd = ioctl(driver_fd, 1, &open_req);

    if (host_fd < 0) {
        fprintf(stderr, "Ioctl failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

	char buf[BUF_LEN] = {0};
	struct hvm_del_req read_req = {
		HVM_DEL_REQ_READ,
		(size_t) host_fd,
		(size_t) &buf[0],
		BUF_LEN,
	};

	int nbytes = 0;

	do {
		//puts("Reading from host fd");
		nbytes = ioctl(driver_fd, 1, &read_req);

		if (nbytes < 0) {
			fprintf(stderr, "Ioctl failed: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}

		write(fileno(stdout), buf, nbytes);
	} while (nbytes == BUF_LEN);

	struct hvm_del_req close_req = {
		HVM_DEL_REQ_CLOSE,
		host_fd,
	};

	//puts("Closing host fd");
	int ret = ioctl(driver_fd, 1, &close_req);

	if (ret != 0) {
		fprintf(stderr, "Ioctl failed: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

    close(driver_fd);

    return 0;
}
