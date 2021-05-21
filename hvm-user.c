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

#include <sys/time.h>

#include "driver-api.h"

#define BUF_LEN 512

static inline uint64_t timestamp(void) {
	uint32_t lo, hi;
	asm volatile("rdtscp" : "=a"(lo), "=d"(hi));
	return lo | ((uint64_t)(hi) << 32);
}

uint64_t benchmark_write(const char *path, size_t len) {
	int driver_fd = open("/dev/hvm", O_RDWR);
    if (driver_fd < 0) {
        fprintf(stderr, "Could not open hvm-del device: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

	// Open
	struct hvm_del_req req = {
		HVM_DEL_REQ_OPEN,
		(uint64_t) path,
		O_WRONLY | O_CREAT | O_SYNC,
		0600,
	};

    int host_fd = ioctl(driver_fd, 1, &req);
	//fprintf(stderr, "host_fd=%d\n", host_fd);
    if (host_fd < 0) {
        fprintf(stderr, "Ioctl failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

	// Allocate a buffer and populate it with "random" data
	uint8_t *buf = malloc(len);
	for (size_t i = 0; i < len; i++) {
		uint8_t byte = (i * 37) % 0xFF;
		buf[i] = byte;
	}

	// Write
	req = (struct hvm_del_req) {
		HVM_DEL_REQ_WRITE,
		host_fd,
		(uint64_t) buf,
		len,
	};

	// The actual call we're benchmarking
	uint64_t before = timestamp();
	uint64_t bytes_written = ioctl(driver_fd, 1, &req);
	uint64_t after = timestamp();

	if (bytes_written < len) {
		// TODO: do more write calls if necessary and benchmark that?
		fprintf(stderr, "only wrote %ld bytes, expected to write %ld bytes\n", bytes_written, len);
		exit(EXIT_FAILURE);
	}

	free(buf);

	// Close
	req = (struct hvm_del_req) {
		HVM_DEL_REQ_CLOSE,
		host_fd,
		0,
		0,
	};
	ioctl(driver_fd, 1, &req);
	close(driver_fd);

	return after - before;
}

int main(int argc, char *argv[]) {
	size_t len = 4096;
	char filepath[64];
	for (int trial = 0; trial < 4096; trial++) {
		sprintf(&filepath[0], "/home/aneth/tmp/trial%04d.bin", trial);
		uint64_t dur = benchmark_write(filepath, len);
		printf("write,%ld,%ld\n", len, dur);
	}
	return 0;
}

int gettimeofday_main(int argc, char *argv[]) {
    int driver_fd = open("/dev/hvm", O_RDWR);
    if (driver_fd < 0) {
        fprintf(stderr, "Could not open hvm-del device: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

	struct timeval tv;
	struct hvm_del_req time_req = {
		HVM_DEL_REQ_GETTIMEOFDAY,
		(uint64_t) &tv,
		0,
		0,
	};
	//uint64_t *times = calloc(4096, sizeof(uint64_t));
	uint64_t total = 0;
	for (int i = 0; i < 4096; i++) {
		uint64_t t_a = timestamp();
		ioctl(driver_fd, 1, &time_req);
		uint64_t t_b = timestamp();
		total += t_b - t_a;
	}
	printf("%ld\n", total / 4096);
	return 0;
}

int lessoldmainmain(int argc, char *argv[]) {
    int driver_fd = open("/dev/hvm", O_RDWR);
    if (driver_fd < 0) {
        fprintf(stderr, "Could not open hvm-del device: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

	struct timeval tv;
	struct hvm_del_req time_req = {
		HVM_DEL_REQ_GETTIMEOFDAY,
		(uint64_t) &tv,
		0,
		0,
	};
	int ret = ioctl(driver_fd, 1, &time_req);
	printf("ioctl returned %d\n", ret);

	printf("time: %ld secs, %ld ns\n", tv.tv_sec, tv.tv_usec);

	return 0;
}

int oldmain (int argc, char ** argv) {
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

	fprintf(stderr, "host_fd=%d\n", host_fd);

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
