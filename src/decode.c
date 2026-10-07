#include "decode.h"
#include "numbers.h"
#include <math.h>
#include <string.h>
static bool named(const char *s, size_t size) { return memchr(s, 0, size) && number_name(s, size - 1); }
canframe_status_t validate_definition(const can_message_def_t *d) {
  uint64_t used = 0;
  if (!d || !named(d->name, sizeof(d->name))) return CANFRAME_ERR_INVALID_FORMAT;
  if (d->id > (d->is_extended ? 0x1fffffffU : 0x7ffU)) return CANFRAME_ERR_INVALID_ID;
  if (d->expected_dlc > 8) return CANFRAME_ERR_INVALID_DLC;
  if (d->signal_count > CANFRAME_MAX_SIGNALS) return CANFRAME_ERR_CAPACITY;
  if ((!d->period_ms && d->tolerance_ms) || (d->period_ms && d->tolerance_ms >= d->period_ms) ||
      (d->timeout_ms && d->period_ms && (uint64_t)d->timeout_ms <= (uint64_t)d->period_ms + d->tolerance_ms)) return CANFRAME_ERR_BOUNDS;
  for (size_t i = 0; i < d->signal_count; i++) {
    const can_signal_t *s = &d->signals[i];
    if (!named(s->name, sizeof(s->name)) || !named(s->unit, sizeof(s->unit)) ||
        !isfinite(s->scale) || !isfinite(s->offset) ||
        (s->endian != CANFRAME_ENDIAN_BIG && s->endian != CANFRAME_ENDIAN_LITTLE)) return CANFRAME_ERR_INVALID_FORMAT;
    if (!s->bit_length || s->bit_length > 64 || (unsigned)s->start_bit + s->bit_length > (unsigned)d->expected_dlc * 8) return CANFRAME_ERR_BOUNDS;
    if (s->has_range && (!isfinite(s->range_min) || !isfinite(s->range_max) || s->range_min > s->range_max)) return CANFRAME_ERR_BOUNDS;
    for (size_t j = 0; j < i; j++) if (!strcmp(s->name, d->signals[j].name)) return CANFRAME_ERR_INVALID_FORMAT;
    for (unsigned j = 0; j < s->bit_length; j++) {
      unsigned p = s->start_bit + j;
      unsigned physical = (p / 8) * 8 + (s->endian == CANFRAME_ENDIAN_BIG ? 7 - p % 8 : p % 8);
      uint64_t bit = UINT64_C(1) << physical;
      if (used & bit) return CANFRAME_ERR_BOUNDS;
      used |= bit;
    }
  }
  return CANFRAME_OK;
}
const can_message_def_t *find_definition(const can_message_def_t *d, size_t count, uint32_t id, bool extended) {
  if (!d || count > CANFRAME_MAX_DEFINITIONS) return NULL;
  for (size_t i = 0; i < count; i++) if (d[i].id == id && d[i].is_extended == extended) return &d[i];
  return NULL;
}
static int64_t signed_value(uint64_t raw, unsigned width) {
  uint64_t sign = UINT64_C(1) << (width - 1);
  uint64_t mask = width == 64 ? UINT64_MAX : (UINT64_C(1) << width) - 1;
  if (!(raw & sign)) return (int64_t)raw;
  uint64_t magnitude = ((~raw) & mask) + 1;
  if (magnitude == (UINT64_C(1) << 63)) return INT64_MIN;
  return -(int64_t)magnitude;
}
canframe_status_t decode_signals(const can_message_def_t *d, const can_frame_t *frame,
                                canframe_signal_result_t *results, size_t capacity, size_t *count) {
  canframe_signal_result_t candidate[CANFRAME_MAX_SIGNALS];
  canframe_status_t status;
  if (count) *count = 0;
  if (!count || !frame || !results) return CANFRAME_ERR_INVALID_FORMAT;
  status = validate_definition(d);
  if (status != CANFRAME_OK) return status;
  if (frame->id != d->id || frame->is_extended != d->is_extended) return CANFRAME_ERR_NO_DEFINITION;
  if (frame->is_rtr || frame->is_error) return CANFRAME_ERR_NOT_APPLICABLE;
  if (frame->dlc != d->expected_dlc) return CANFRAME_ERR_INVALID_DLC;
  if (capacity < d->signal_count) return CANFRAME_ERR_CAPACITY;
  for (size_t i = 0; i < d->signal_count; i++) {
    const can_signal_t *s = &d->signals[i];
    canframe_signal_result_t r = {0};
    r.definition = s;
    for (unsigned j = 0; j < s->bit_length; j++) {
      unsigned p = s->start_bit + j;
      uint64_t bit = (frame->data[p / 8] >> (s->endian == CANFRAME_ENDIAN_BIG ? 7 - p % 8 : p % 8)) & 1U;
      if (s->endian == CANFRAME_ENDIAN_BIG) r.raw_unsigned = (r.raw_unsigned << 1) | bit;
      else r.raw_unsigned |= bit << j;
    }
    if (s->is_signed) r.raw_signed = signed_value(r.raw_unsigned, s->bit_length);
    r.precision_warning = s->is_signed ? (r.raw_signed > INT64_C(9007199254740992) || r.raw_signed < -INT64_C(9007199254740992)) : r.raw_unsigned > UINT64_C(9007199254740992);
    r.physical = (s->is_signed ? (double)r.raw_signed : (double)r.raw_unsigned) * s->scale + s->offset;
    if (!isfinite(r.physical)) r.quality = CANFRAME_SIGNAL_NUMERIC_ERROR;
    else if (s->has_range && (r.physical < s->range_min || r.physical > s->range_max)) r.quality = CANFRAME_SIGNAL_OUT_OF_RANGE;
    else r.quality = CANFRAME_SIGNAL_VALID;
    candidate[i] = r;
  }
  memcpy(results, candidate, d->signal_count * sizeof(*results));
  *count = d->signal_count;
  return CANFRAME_OK;
}
