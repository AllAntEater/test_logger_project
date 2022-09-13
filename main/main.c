
#include "freertos/FreeRTOS.h"
#include "freertos/FreeRTOSConfig.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "project_defs.h"
#include "sdkconfig.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>


#if(portNUM_PROCESSORS > 1)
#define USER_TASK_PINNED_CODE (1)
#else
#define USER_TASK_PINNED_CODE (0)
#endif


TaskHandle_t xCounterTaskHandle = NULL;
StaticTask_t xCounterTaskControlBlock;
StackType_t xCounterTaskStack[TASK_STACK_SIZE_COUNTER];

TaskHandle_t xLoggerTaskHandle = NULL;
StaticTask_t xLoggerTaskControlBlock;
StackType_t xLoggerTaskStack[TASK_STACK_SIZE_LOGGER];

QueueHandle_t xLoggerQueueHandler = NULL;
StaticQueue_t xLoggerQueueControlBlock;
counter_type_t xLoggerQueueStorage[LOGGER_QUEUE_SIZE];

// Period for creating Log item in ms
const uint32_t ulLogTimeout = 5000;

const static char* tag_log_func = "Log";

void log_function(esp_log_level_t xLogLevel, const char* pcLogItem);
void counter_task(void* pvArg);
void logger_task(void* pvArg);


void
log_function(esp_log_level_t xLogLevel, const char* pcLogItem)
{
	assert(NULL != pcLogItem);

	switch(xLogLevel)
	{
	case ESP_LOG_ERROR: {
		ESP_LOGE(tag_log_func, "%s", pcLogItem);
		break;
	}

	case ESP_LOG_WARN: {
		ESP_LOGW(tag_log_func, "%s", pcLogItem);
		break;
	}

	case ESP_LOG_INFO: {
		ESP_LOGI(tag_log_func, "%s", pcLogItem);
		break;
	}

	default:
		ESP_LOGV(tag_log_func, "Unsupported log type %d with: %s", (int)xLogLevel, pcLogItem);
		break;
	}
}

void
logger_task(void* pvArg)
{
	assert(NULL != pvArg);

	log_callback_func_t log_callback = (log_callback_func_t)pvArg;
	counter_type_t xQueueItem = {.ulValue = 0UL};
	TickType_t xLastTickCount = 0UL;
	TickType_t xCurrentTickCount = 0UL;

	uint8_t ucLogBuffer[128];
	memset(&ucLogBuffer[0], 0x00, sizeof(ucLogBuffer));

	for(;;)
	{
		if(xQueueReceive(xLoggerQueueHandler, &xQueueItem, portMAX_DELAY))
		{
			xCurrentTickCount = xTaskGetTickCount();
			sprintf((char*)&ucLogBuffer[0],
			        "Counter: %lu, Ticks diff.: %lu",
			        (unsigned long)xQueueItem.ulValue,
			        (unsigned long)(xCurrentTickCount - xLastTickCount));
			xLastTickCount = xCurrentTickCount;

			log_callback(ESP_LOG_INFO, (const char*)&ucLogBuffer[0]);
		}
	}

	vTaskDelete(NULL);
}

void
counter_task(void* pvArg)
{
	(void)pvArg;

	counter_type_t xQueueItem = {.ulValue = 0UL};
	BaseType_t xRes = pdFALSE;

	// Variables for compensation delay drift, caused by code execution
	TickType_t xLogTicksTimeout = pdMS_TO_TICKS(ulLogTimeout);
	TickType_t xLastIncrementTime = xTaskGetTickCount();

	for(;;)
	{
		xRes = xQueueSend(xLoggerQueueHandler, &xQueueItem, pdMS_TO_TICKS(1));

		if(pdFALSE == xRes)
		{
			// Something went wrong with logger task.
			assert(false);
		}

		vTaskDelayUntil(&xLastIncrementTime, xLogTicksTimeout);
		++xQueueItem.ulValue;
	}

	vTaskDelete(NULL);
}

void
app_main(void)
{
	xLoggerQueueHandler = xQueueCreateStatic(
	    LOGGER_QUEUE_SIZE, sizeof(counter_type_t), (uint8_t*)&xLoggerQueueStorage[0], &xLoggerQueueControlBlock);
	assert(NULL != xLoggerQueueHandler);

	xLoggerTaskHandle = xTaskCreateStaticPinnedToCore((TaskFunction_t)logger_task,
	                                                  "logger_task",
	                                                  TASK_STACK_SIZE_LOGGER,
	                                                  &log_function,
	                                                  1,
	                                                  xLoggerTaskStack,
	                                                  &xLoggerTaskControlBlock,
	                                                  USER_TASK_PINNED_CODE);
	assert(NULL != xLoggerTaskHandle);

	xCounterTaskHandle = xTaskCreateStaticPinnedToCore((TaskFunction_t)counter_task,
	                                                   "counter_task",
	                                                   TASK_STACK_SIZE_COUNTER,
	                                                   NULL,
	                                                   1,
	                                                   xCounterTaskStack,
	                                                   &xCounterTaskControlBlock,
	                                                   USER_TASK_PINNED_CODE);
	assert(NULL != xCounterTaskHandle);

	vTaskDelete(NULL);
}
