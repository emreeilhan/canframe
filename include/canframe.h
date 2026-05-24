#ifndef CANFRAME_H
#define CANFRAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CANFRAME_MAX_DLC 8
#define CANFRAME_IFNAME_MAX 16

typedef enum {
  CANFRAME_OK = 0,
  CANFRAME_ERR_INVALID_ID,
  CANFRAME_ERR_INVALID_DLC,
  CANFRAME_ERR_INVALID_DATA,
  CANFRAME_ERR_INVALID_FORMAT,
  CANFRAME_ERR_UNSUPPORTED
} canframe_status_t;

typedef struct {
  uint32_t id;
  uint8_t dlc;
  uint8_t data[CANFRAME_MAX_DLC];
  bool is_extended;
  bool is_rtr;
  bool has_timestamp;
  double timestamp;
  char ifname[CANFRAME_IFNAME_MAX];
} can_frame_t;

typedef enum {
  CANFRAME_ENDIAN_LITTLE = 0,
  CANFRAME_ENDIAN_BIG
} can_endian_t;

typedef struct {
  char name[32];
  uint8_t start_bit;
  uint8_t bit_length;
  can_endian_t endian;
  bool is_signed;
  double scale;
  double offset;
  char unit[16];
} can_signal_t;

typedef struct {
  uint32_t id;
  char name[32];
  can_signal_t signals[16];
  size_t signal_count;
} can_message_def_t;

#endif

