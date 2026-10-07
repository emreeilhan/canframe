#ifndef CANFRAME_H
#define CANFRAME_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define CANFRAME_MAX_DLC 8
#define CANFRAME_IFNAME_MAX 16
#define CANFRAME_MAX_SIGNALS 16
#define CANFRAME_MAX_DEFINITIONS 64
#define CANFRAME_MAX_LINE 4096

typedef enum {
  CANFRAME_OK = 0, CANFRAME_ERR_INVALID_ID, CANFRAME_ERR_INVALID_DLC,
  CANFRAME_ERR_INVALID_DATA, CANFRAME_ERR_INVALID_FORMAT, CANFRAME_ERR_UNSUPPORTED,
  CANFRAME_ERR_IO, CANFRAME_ERR_BOUNDS, CANFRAME_ERR_CAPACITY,
  CANFRAME_ERR_NO_DEFINITION, CANFRAME_ERR_NOT_APPLICABLE
} canframe_status_t;
typedef enum { CANFRAME_TIME_NONE, CANFRAME_TIME_SOURCE, CANFRAME_TIME_USERSPACE } canframe_time_source_t;
typedef struct {
  uint32_t id;
  uint8_t dlc;
  uint8_t data[CANFRAME_MAX_DLC];
  bool is_extended, is_rtr, is_error, has_timestamp;
  double timestamp; /* compatibility display; diagnostics use exact timestamp_ns */
  uint64_t timestamp_ns;
  canframe_time_source_t time_source;
  char ifname[CANFRAME_IFNAME_MAX];
} can_frame_t;
typedef enum { CANFRAME_ENDIAN_LITTLE = 0, CANFRAME_ENDIAN_BIG } can_endian_t;
typedef struct {
  char name[32];
  uint8_t start_bit, bit_length;
  can_endian_t endian;
  bool is_signed;
  double scale, offset;
  char unit[16];
  bool has_range;
  double range_min, range_max;
} can_signal_t;
typedef struct {
  uint32_t id;
  bool is_extended;
  uint8_t expected_dlc;
  char name[32];
  can_signal_t signals[CANFRAME_MAX_SIGNALS];
  size_t signal_count;
  uint32_t period_ms, tolerance_ms, timeout_ms; /* zero = not configured */
} can_message_def_t;
#endif
