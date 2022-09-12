#ifndef _PROJECT_DEFS_H
#define _PROJECT_DEFS_H

#include "esp_log.h"

#include <stdint.h>


#define TASK_STACK_SIZE_COUNTER (2048)
#define TASK_STACK_SIZE_LOGGER  (2048)
#define LOGGER_QUEUE_SIZE       (10)

typedef void (*log_callback_func_t)(esp_log_level_t, const char*);

typedef struct
{
	uint32_t ulValue;
} counter_type_t;

#endif