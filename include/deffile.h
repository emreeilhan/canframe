#ifndef DEFFILE_H
#define DEFFILE_H

#include "canframe.h"

canframe_status_t deffile_load(const char *path, can_message_def_t *definitions,
                               size_t max_definitions, size_t *loaded_count);

#endif

