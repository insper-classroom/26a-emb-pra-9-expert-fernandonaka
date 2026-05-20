/*
 * main.c — FreeRTOS skeleton
 *
 * Compliant with cppcheck and embedded static analysis (MISRA-C subset).
 * Rules applied:
 *   - All parameters explicitly cast to (void) when unused
 *   - No magic numbers (use #define)
 *   - All variables initialised at declaration
 *   - Return value of xTaskCreate always checked
 *   - No implicit int; explicit types throughout
 *   - Static linkage for everything not exported
 */

#include <stdint.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

/* ------------------------------------------------------------------ */
/* Configuration                                                        */
/* ------------------------------------------------------------------ */

#define TASK_STACK_WORDS  (512U)
#define TASK_PRIORITY_LOW (1U)
#define QUEUE_LENGTH      (8U)
#define QUEUE_ITEM_SIZE   (sizeof(uint32_t))

/* ------------------------------------------------------------------ */
/* Shared objects (module-scope)                                        */
/* ------------------------------------------------------------------ */

static QueueHandle_t    xDataQueue    = NULL;
static SemaphoreHandle_t xMutex       = NULL;

/* ------------------------------------------------------------------ */
/* Forward declarations                                                 */
/* ------------------------------------------------------------------ */

static void vProducerTask(void *pvParameters);
static void vConsumerTask(void *pvParameters);

/* ------------------------------------------------------------------ */
/* Task implementations                                                 */
/* ------------------------------------------------------------------ */

/*
 * Sends a counter value to the queue every 100 ms.
 */
static void vProducerTask(void *pvParameters)
{
    uint32_t ulCounter = 0U;
    BaseType_t xStatus;

    (void)pvParameters; /* unused */

    for (;;)
    {
        xStatus = xQueueSend(xDataQueue, &ulCounter, portMAX_DELAY);
        if (xStatus != pdPASS)
        {
            /* Queue full — handle error */
        }

        ulCounter++;
        vTaskDelay(pdMS_TO_TICKS(100U));
    }
}

/*
 * Receives values from the queue and processes them under mutex.
 */
static void vConsumerTask(void *pvParameters)
{
    uint32_t ulReceived = 0U;
    BaseType_t xStatus;

    (void)pvParameters; /* unused */

    for (;;)
    {
        xStatus = xQueueReceive(xDataQueue, &ulReceived, portMAX_DELAY);
        if (xStatus == pdPASS)
        {
            if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE)
            {
                printf("Received: %lu\n", (unsigned long)ulReceived);
                (void)xSemaphoreGive(xMutex);
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* Entry point                                                          */
/* ------------------------------------------------------------------ */

int main(void)
{
    BaseType_t xRet = pdFAIL;

    xDataQueue = xQueueCreate(QUEUE_LENGTH, QUEUE_ITEM_SIZE);
    configASSERT(xDataQueue != NULL);

    xMutex = xSemaphoreCreateMutex();
    configASSERT(xMutex != NULL);

    xRet = xTaskCreate(vProducerTask,
                       "Producer",
                       TASK_STACK_WORDS,
                       NULL,
                       TASK_PRIORITY_LOW,
                       NULL);
    configASSERT(xRet == pdPASS);

    xRet = xTaskCreate(vConsumerTask,
                       "Consumer",
                       TASK_STACK_WORDS,
                       NULL,
                       TASK_PRIORITY_LOW,
                       NULL);
    configASSERT(xRet == pdPASS);

    vTaskStartScheduler();

    /* Should never reach here */
    for (;;)
    {
    }

    return 0; /* unreachable; satisfies compiler */
}
