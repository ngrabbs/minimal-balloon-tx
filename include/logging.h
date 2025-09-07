#pragma once
#include <stdio.h>
#define LOG_TAG(tag, fmt, ...) printf("[%s] " fmt "\r\n", tag, ##__VA_ARGS__)
#define LOGI(fmt, ...) LOG_TAG("INFO", fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) LOG_TAG("WARN", fmt, ##__VA_ARGS__)
#define LOGE(fmt, ...) LOG_TAG("ERR",  fmt, ##__VA_ARGS__)
