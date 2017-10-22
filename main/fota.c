#include <esp_log.h>
#include <string.h>
#include <sys/socket.h>
#include "fota.h"


// Contants
#define TAG_FOTA "ota"
#define BUFFSIZE 1024

// Variables & buffers
char writeDataBuff[BUFFSIZE + 1] = { 0 }; // Fota data write buffer ready to write to the flash
char packet[BUFFSIZE + 1] = { 0 };         // Packet receive buffer
int binaryFileLength = 0;                  // Image total length
int socketId = -1;
char http_request[64] = {0};


uint8_t fota_update(char *url) {
    ESP_LOGI(TAG_FOTA, "Start updating with %s", url);
    xTaskCreatePinnedToCore(&fota_update_task, "fota_update_task", 8192, NULL, 4, NULL, 1);
    return 0;
}

void fota_update_task(void *pvParameter) {
    esp_err_t err;
    /* update handle : set by esp_ota_begin(), must be freed via esp_ota_end() */
    esp_ota_handle_t update_handle = 0 ;
    const esp_partition_t *update_partition = NULL;

    ESP_LOGI(TAG_FOTA, "Starting OTA example...");

    const esp_partition_t *configured = esp_ota_get_boot_partition();
    const esp_partition_t *running = esp_ota_get_running_partition();

    if (configured != running) {
        ESP_LOGW(TAG_FOTA, "Configured OTA boot partition at offset 0x%08x, but running from offset 0x%08x",
                 configured->address, running->address);
        ESP_LOGW(TAG_FOTA, "(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)");
    }
    ESP_LOGI(TAG_FOTA, "Running partition type %d subtype %d (offset 0x%08x)",
             running->type, running->subtype, running->address);

    /* Wait for the callback to set the CONNECTED_BIT in the
       event group.
    */
    /*
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                        false, true, portMAX_DELAY);
    ESP_LOGI(TAG_FOTA, "Connect to Wifi ! Start to Connect to Server....");
    TODO: Might not be required as triggered by MQTT
    */

    /*connect to http server*/
    if (fota_connect_to_http_server()) {
        ESP_LOGI(TAG_FOTA, "Connected to http server");
    } else {
        ESP_LOGE(TAG_FOTA, "Connect to http server failed!");
        fota_task_fatal_error();
    }

    int res = -1;
    /*send GET request to http server*/
    res = send(socketId, http_request, strlen(http_request), 0);
    if (res == -1) {
        ESP_LOGE(TAG_FOTA, "Send GET request to server failed");
        fota_task_fatal_error();
    } else {
        ESP_LOGI(TAG_FOTA, "Send GET request to server succeeded");
    }

    update_partition = esp_ota_get_next_update_partition(NULL);
    ESP_LOGI(TAG_FOTA, "Writing to partition subtype %d at offset 0x%x",
             update_partition->subtype, update_partition->address);
    assert(update_partition != NULL);

    err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_FOTA, "esp_ota_begin failed, error=%d", err);
        fota_task_fatal_error();
    }
    ESP_LOGI(TAG_FOTA, "esp_ota_begin succeeded");

    bool resp_body_start = false, flag = true;
    /*deal with all receive packet*/
    while (flag) {
        memset(packet, 0, BUFFSIZE);
        memset(writeDataBuff, 0, BUFFSIZE);
        int buff_len = recv(socketId, packet, BUFFSIZE, 0);
        if (buff_len < 0) { /*receive error*/
            ESP_LOGE(TAG_FOTA, "Error: receive data error! errno=%d", errno);
            fota_task_fatal_error();
        } else if (buff_len > 0 && !resp_body_start) { /*deal with response header*/
            memcpy(writeDataBuff, packet, buff_len);
            resp_body_start = fota_read_past_http_header(packet, buff_len, update_handle);
        } else if (buff_len > 0 && resp_body_start) { /*deal with response body*/
            memcpy(writeDataBuff, packet, buff_len);
            err = esp_ota_write( update_handle, (const void *)writeDataBuff, buff_len);
            if (err != ESP_OK) {
                ESP_LOGE(TAG_FOTA, "Error: esp_ota_write failed! err=0x%x", err);
                fota_task_fatal_error();
            }
            binaryFileLength += buff_len;
            ESP_LOGI(TAG_FOTA, "Have written image length %d", binaryFileLength);
        } else if (buff_len == 0) {  /*packet over*/
            flag = false;
            ESP_LOGI(TAG_FOTA, "Connection closed, all packets received");
            close(socketId);
        } else {
            ESP_LOGE(TAG_FOTA, "Unexpected recv result");
        }
    }

    ESP_LOGI(TAG_FOTA, "Total Write binary data length : %d", binaryFileLength);

    if (esp_ota_end(update_handle) != ESP_OK) {
        ESP_LOGE(TAG_FOTA, "esp_ota_end failed!");
        fota_task_fatal_error();
    }
    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_FOTA, "esp_ota_set_boot_partition failed! err=0x%x", err);
        fota_task_fatal_error();
    }
    ESP_LOGI(TAG_FOTA, "Prepare to restart system!");
    esp_restart();
    return ;
}



void __attribute__((noreturn)) fota_task_fatal_error()
{
   ESP_LOGE(TAG_FOTA, "Exiting task due to fatal error...");
   close(socketId);
   (void)vTaskDelete(NULL);

   while (1) {
       ;
   }
}

int fota_read_until(char *buffer, char delim, int len) {
//  /*TODO: delim check,buffer check,further: do an buffer length limited*/
    int i = 0;
    while (buffer[i] != delim && i < len) {
        ++i;
    }
    return i + 1;
}

bool fota_read_past_http_header(char text[], int total_len, esp_ota_handle_t update_handle) {
   /* i means current position */
   int i = 0, i_read_len = 0;
   while (text[i] != 0 && i < total_len) {
       i_read_len = fota_read_until(&text[i], '\n', total_len);
       // if we resolve \r\n line,we think packet header is finished
       if (i_read_len == 2) {
           int i_write_len = total_len - (i + 2);
           memset(writeDataBuff, 0, BUFFSIZE);
           /*copy first http packet body to write buffer*/
           memcpy(writeDataBuff, &(text[i + 2]), i_write_len);

           esp_err_t err = esp_ota_write( update_handle, (const void *)writeDataBuff, i_write_len);
           if (err != ESP_OK) {
               ESP_LOGE(TAG_FOTA, "Error: esp_ota_write failed! err=0x%x", err);
               return false;
           } else {
               ESP_LOGI(TAG_FOTA, "esp_ota_write header OK");
               binaryFileLength += i_write_len;
           }
           return true;
       }
       i += i_read_len;
   }
   return false;
}

bool fota_connect_to_http_server()
{
   // TODO: Use fota_update()
   ESP_LOGI(TAG_FOTA, "Server IP: %s Server Port:%s", "192.168.1.10", "8080");
   sprintf(http_request, "GET %s HTTP/1.1\r\nHost: %s:%s \r\n\r\n", "BLE_Tracker.bin", "192.168.1.10", "8080");

   int  http_connect_flag = -1;
   struct sockaddr_in sock_info;

   socketId = socket(AF_INET, SOCK_STREAM, 0);
   if (socketId == -1) {
       ESP_LOGE(TAG_FOTA, "Create socket failed!");
       return false;
   }

   // set connect info
   memset(&sock_info, 0, sizeof(struct sockaddr_in));
   sock_info.sin_family = AF_INET;
   // TODO Use start_update parameters
   sock_info.sin_addr.s_addr = inet_addr("192.168.1.10");
   sock_info.sin_port = htons(atoi("8080"));

   // connect to http server
   http_connect_flag = connect(socketId, (struct sockaddr *)&sock_info, sizeof(sock_info));
   if (http_connect_flag == -1) {
       ESP_LOGE(TAG_FOTA, "Connect to server failed! errno=%d", errno);
       close(socketId);
       return false;
   } else {
       ESP_LOGI(TAG_FOTA, "Connected to server");
       return true;
   }
   return false;
}