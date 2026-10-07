#ifndef DEFFILE_H
#define DEFFILE_H
#include "canframe.h"
#define CFD_MAX_BYTES 65536
#define CFD_MAX_LINE 1024
typedef struct { canframe_status_t status; size_t line; char reason[96]; } cfd_error_t;
canframe_status_t deffile_parse_bytes(const uint8_t *data, size_t size, can_message_def_t *definitions,
                                     size_t capacity, size_t *count, cfd_error_t *error);
canframe_status_t deffile_load_ex(const char *path, can_message_def_t *definitions,
                                size_t capacity, size_t *count, cfd_error_t *error);
canframe_status_t deffile_load(const char *path, can_message_def_t *definitions,
                             size_t capacity, size_t *count);
#endif
