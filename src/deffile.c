#include "deffile.h"

canframe_status_t deffile_load(const char *path, can_message_def_t *definitions,
                               size_t max_definitions, size_t *loaded_count) {
  (void)path;
  (void)definitions;
  (void)max_definitions;
  if (loaded_count != NULL) {
    *loaded_count = 0;
  }
  return CANFRAME_ERR_UNSUPPORTED;
}

