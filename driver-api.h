#ifndef __DRIVER_API__
#define __DRIVER_API__

#define HVM_DEL_MAGIC_PORT 0x7C4

#define HVM_DEL_REQ_FOO 0x1

struct hvm_del_req {
    char *foo;
    int bar;
    char *baz;
};

#endif
