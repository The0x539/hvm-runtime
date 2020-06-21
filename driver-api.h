#ifndef __DRIVER_API__
#define __DRIVER_API__

#define HOST_MAX_PATH 512 // whatever

#define HVM_DEL_MAGIC_PORT 0x7C4

#define HVM_DEL_REQ_OPEN 0x1
#define HVM_DEL_REQ_READ 0x2
#define HVM_DEL_REQ_WRITE 0x3
#define HVM_DEL_REQ_CLOSE 0x4

struct hvm_del_args {
	size_t arg1;
	size_t arg2;
	size_t arg3;
	size_t arg4;
};

#endif
