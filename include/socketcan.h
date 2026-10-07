#ifndef SOCKETCAN_H
#define SOCKETCAN_H
#include "canframe.h"
/* descriptor is caller-owned; only Linux implements this Classical CAN API. */
int socketcan_open(const char *interface, char *reason, size_t capacity);
/* 1 frame, 0 poll timeout, -1 error, -2 interrupted */
int socketcan_receive(int fd, const char *interface, int timeout_ms, can_frame_t *frame, char *reason, size_t capacity);
void socketcan_close(int fd);
bool socketcan_supported(void);
uint64_t socketcan_monotonic_ns(void);
#endif
