#define _POSIX_C_SOURCE 200809L
#include "socketcan.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#ifdef __linux__
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
uint64_t socketcan_monotonic_ns(void) {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts)) return 0;
  return (uint64_t)ts.tv_sec * UINT64_C(1000000000) + (uint64_t)ts.tv_nsec;
}
bool socketcan_supported(void) {
#ifdef __linux__
  return true;
#else
  return false;
#endif
}
int socketcan_open(const char *interface, char *reason, size_t capacity) {
#ifdef __linux__
  struct sockaddr_can address = {0};
  unsigned index;
  int fd;
  if (!interface || !*interface || strlen(interface) >= CANFRAME_IFNAME_MAX) { snprintf(reason, capacity, "invalid interface"); return -1; }
  index = if_nametoindex(interface);
  if (!index) { snprintf(reason, capacity, "interface lookup: %s", strerror(errno)); return -1; }
  fd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
  if (fd < 0) { snprintf(reason, capacity, "CAN socket: %s", strerror(errno)); return -1; }
  address.can_family = AF_CAN; address.can_ifindex = (int)index;
  if (bind(fd, (struct sockaddr *)&address, sizeof(address))) {
    snprintf(reason, capacity, "CAN bind: %s", strerror(errno)); close(fd); return -1;
  }
  /* Default RAW socket: Classical CAN only, error frames disabled. */
  return fd;
#else
  (void)interface; snprintf(reason, capacity, "SocketCAN live input is supported only on Linux"); return -1;
#endif
}
int socketcan_receive(int fd, const char *interface, int timeout_ms, can_frame_t *out, char *reason, size_t capacity) {
#ifdef __linux__
  struct pollfd pfd = {fd, POLLIN, 0};
  struct can_frame wire;
  can_frame_t frame = {0};
  int ready = poll(&pfd, 1, timeout_ms);
  ssize_t size;
  if (ready < 0) {
    if (errno == EINTR) return -2;
    snprintf(reason, capacity, "CAN poll: %s", strerror(errno)); return -1;
  }
  if (!ready) return 0;
  if (!(pfd.revents & POLLIN)) { snprintf(reason, capacity, "CAN socket closed or poll error"); return -1; }
  size = read(fd, &wire, sizeof(wire));
  if (size < 0 && errno == EINTR) return -2;
  if (size != CAN_MTU || wire.can_dlc > 8) { snprintf(reason, capacity, "invalid Classical CAN record"); return -1; }
  if (wire.can_id & CAN_ERR_FLAG) return 0; /* not enabled; defensive exclusion */
  frame.is_extended = (wire.can_id & CAN_EFF_FLAG) != 0;
  frame.is_rtr = (wire.can_id & CAN_RTR_FLAG) != 0;
  frame.id = wire.can_id & (frame.is_extended ? CAN_EFF_MASK : CAN_SFF_MASK);
  frame.dlc = wire.can_dlc;
  if (!frame.is_rtr) memcpy(frame.data, wire.data, frame.dlc);
  frame.timestamp_ns = socketcan_monotonic_ns(); frame.has_timestamp = true;
  frame.timestamp = (double)frame.timestamp_ns / 1e9; frame.time_source = CANFRAME_TIME_USERSPACE;
  snprintf(frame.ifname, sizeof(frame.ifname), "%s", interface); *out = frame;
  return 1;
#else
  (void)fd; (void)interface; (void)timeout_ms; (void)out;
  snprintf(reason, capacity, "SocketCAN live input is supported only on Linux"); return -1;
#endif
}
void socketcan_close(int fd) {
#ifdef __linux__
  if (fd >= 0) close(fd);
#else
  (void)fd;
#endif
}
