ESP32 BLE beacon tracker
========================


Work in progress to implement an BLE sniffer sending packets through MQTT

Installation
------------

* Install ESP32 toolchain as described [here](https://esp-idf.readthedocs.io/en/latest/get-started/linux-setup.html). I currently use [1.22.0-75](http://dl.espressif.com/dl/xtensa-esp32-elf-linux64-1.22.0-75-gbaf03c2-5.2.0.tar.gz)
* Clone esp-idf and set its `IDF_PATH` environment variable. I use this specific [commit](https://github.com/espressif/esp-idf/tree/02304ad83e0a5f4815789d581446fa3afdd017b9), close to v2.1
* Run `make menuconfig`
  * `Network configuration` to configure WiFi and MQTT
  * `Component config`
    * `Bluetooth`->`Bluedroid Bluetooth stack enabled` to activate `GATT client module(GATTC)`
    * `Partition Table` -> Select `Custom partition CSV file`


Configuration
-------------
While WiFi and MQTT have been configured in previous step, some configuration remains -for now- in code:
* MQTT
 * Security (TLS): Edit espmqtt library `#define CONFIG_MQTT_SECURITY_ON`, in file [mqtt_config.h](https://github.com/tuanpmt/espmqtt/blob/2967332b95454d4b53068a0d5484ae60e312eb12/include/mqtt_config.h#L7)
 * Publication topic, retain & QOS: Edit them in `mqtt_publish()` [here](https://github.com/Oliv4945/ESP-Beacon-Tracker/blob/master/main/gattc_demo.c#L541)
* Scan parameters
  * Frequency: `#define SCAN_FREQUENCY_MS` [here](https://github.com/Oliv4945/ESP-Beacon-Tracker/blob/master/main/gattc_demo.c#L60) in milliseconds
  * Duration: `#define SCAN_DURATION_S` [next line](https://github.com/Oliv4945/ESP-Beacon-Tracker/blob/master/main/gattc_demo.c#L62)
