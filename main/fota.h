#ifndef __FOTA_H__
#define __FOTA_H__

// Includes
#include <stdint.h>
#include "esp_ota_ops.h"

/*
 * Start firmware over the air upgrade
 * char *url : Url of the new firmware
 * return: FOTA state
 */
uint8_t fota_update(char *url);

/*
 * Read buffer by byte still delim ,return read bytes counts
 */
int fota_read_until(char *buffer, char delim, int len);

/*
 * resolve a packet from http socket
 * return true if packet including \r\n\r\n that means http packet header finished,start to receive packet body
 * otherwise return false
 */
bool fota_read_past_http_header(char text[], int total_len, esp_ota_handle_t update_handle);

/*
 * Connect to HTTP server and prepare HTTP request
 */
bool fota_connect_to_http_server();

/*
 * Error handler
 */
void __attribute__((noreturn)) fota_task_fatal_error();

/*
 * Example from ESP-IDF for now
 */
void fota_update_task( void );

#endif