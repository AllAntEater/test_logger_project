
#include "freertos/FreeRTOS.h"
#include "freertos/FreeRTOSConfig.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "project_defs.h"
#include "sdkconfig.h"

#include <stdio.h>


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

const static char* tag_log_func = "Log: ";
// const static char* tag_log_task = "Log task: ";

void log_function(esp_log_level_t xLogLevel, const char* pcLogItem);
void counter_task(void* pvArg);
void logger_task(void* pvArg);

void
log_function(esp_log_level_t xLogLevel, const char* pcLogItem)
{
	if(NULL == pcLogItem)
	{
		return;
	}

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
	counter_type_t xQueueItem = {.ulValue = 0UL};
	uint8_t ucLogBuffer[20];

	log_callback_func_t log_callback = NULL;

	if(NULL != pvArg)
	{
		log_callback = (log_callback_func_t)pvArg;
	}
	else
	{
		// ESP_LOGE(tag_log_task, "log function is not provided!");
		vTaskDelete(NULL);
	}

	for(;;)
	{
		if(xQueueReceive(xLoggerQueueHandler, &xQueueItem, portMAX_DELAY))
		{
			sprintf((char*)&ucLogBuffer[0], "%lu", (unsigned long)xQueueItem.ulValue);
			log_callback(ESP_LOG_INFO, (const char*)&ucLogBuffer[0]);
		}
	}

	vTaskDelete(NULL);
}

void
counter_task(void* pvArg)
{
	counter_type_t xQueueItem = {.ulValue = 0UL};
	BaseType_t xRes = pdFALSE;

	(void)pvArg;

	for(;;)
	{
		xRes = xQueueSend(xLoggerQueueHandler, &xQueueItem, pdMS_TO_TICKS(1));

		if(pdFALSE == xRes)
		{
			// Something went wrong with logger task. Do something!
		}

		vTaskDelay(pdMS_TO_TICKS(ulLogTimeout));
		++xQueueItem.ulValue;
	}

	vTaskDelete(NULL);
}

void
app_main(void)
{
	xLoggerQueueHandler = xQueueCreateStatic(
	    LOGGER_QUEUE_SIZE, sizeof(counter_type_t), (uint8_t*)&xLoggerQueueStorage[0], &xLoggerQueueControlBlock);

	xLoggerTaskHandle = xTaskCreateStaticPinnedToCore((TaskFunction_t)logger_task,
	                                                  "logger_task",
	                                                  TASK_STACK_SIZE_LOGGER,
	                                                  &log_function,
	                                                  1,
	                                                  xLoggerTaskStack,
	                                                  &xLoggerTaskControlBlock,
	                                                  USER_TASK_PINNED_CODE);

	xCounterTaskHandle = xTaskCreateStaticPinnedToCore((TaskFunction_t)counter_task,
	                                                   "counter_task",
	                                                   TASK_STACK_SIZE_COUNTER,
	                                                   NULL,
	                                                   1,
	                                                   xCounterTaskStack,
	                                                   &xCounterTaskControlBlock,
	                                                   USER_TASK_PINNED_CODE);

	vTaskDelete(NULL);
}
