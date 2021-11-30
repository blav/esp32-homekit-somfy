#ifndef __mutex_h
#define __mutex_h

#include <esp_log.h>

#define ESP_ERROR_CHECK_NOTNULL(x) do {     \
  void * __rc = (x);                        \
  if (__rc == NULL)                         \
    ESP_ERROR_CHECK(ESP_ERR_NO_MEM);        \
} while(0)

#define MUTEX_TIMEOUT 100

#define MUTEX_TAKE(sem) do {                                          \
  ESP_LOGI("mutex", "take %16lx", (unsigned long) sem);                  \
  while (xSemaphoreTake(sem, (TickType_t) MUTEX_TIMEOUT) != pdTRUE);  \
} while (0)

#define MUTEX_GIVE(sem) do {                                          \
  ESP_LOGI("mutex", "give %16lx", (unsigned long) sem);                  \
  xSemaphoreGive(sem);                                                \
} while (0)

#endif//__mutex_h