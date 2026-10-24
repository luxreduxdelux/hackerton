#include "network.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "esp_idf_version.h"
#include <string.h>

static const char *TAG = "ESP_NOW_COMM";

// dir broadcasta para enviar a la otra placa
static const uint8_t s_broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static comm_rx_callback_t s_rx_callback = NULL;

// callback interno invocado del driver de ESP-NOW al recibir datos
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
static void on_data_recv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    const uint8_t *src_mac = recv_info ? recv_info->src_addr : NULL;
#else
static void on_data_recv(const uint8_t *mac_addr, const uint8_t *data, int len) {
    const uint8_t *src_mac = mac_addr;
#endif
    if (data && len > 0) {
        //ESP_LOGI(TAG, "Mensaje recibido: %d bytes", len);
        if (s_rx_callback) {
            s_rx_callback(data, (size_t)len, src_mac);
        }
    }
}

// callback del status de envío (para depurar)
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
static void on_data_sent(const wifi_tx_info_t *tx_info,
                         esp_now_send_status_t status) {
#else
static void on_data_sent(const uint8_t *mac_addr,
                         esp_now_send_status_t status) {
#endif
    if (status == ESP_NOW_SEND_SUCCESS) {
        ESP_LOGD(TAG, "Paquete enviado exitosamente");
    } else {
        ESP_LOGW(TAG, "Fallo al enviar paquete");
    }
}

bool comm_init(comm_rx_callback_t rx_cb) {
    s_rx_callback = rx_cb;

    // 1 inicializo el almacenamiento NVS Flash
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error inicializando NVS Flash: %s", esp_err_to_name(ret));
        return false;
    }

    // 2 - inicializo la pila de red y el loop de eventos
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 3 - Wi-Fi en modo station (STA)
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // fijo el canal wifi 1 para sincronizar las placas
    ESP_ERROR_CHECK(esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE));

    ESP_LOGI(TAG, "Wi-Fi activado en modo STA (Canal 1)");

    // 4 - inicializo el protocolo ESP-NOW
    ret = esp_now_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al inicializar ESP-NOW: %s", esp_err_to_name(ret));
        return false;
    }

    // registro de callbacks
    esp_now_init();
    esp_now_register_recv_cb(on_data_recv);
    esp_now_register_send_cb(on_data_sent);

    // 5 - agrego la dirección broadcast como peer
    esp_now_peer_info_t peer_info = {0};
    memcpy(peer_info.peer_addr, s_broadcast_mac, 6);
    peer_info.channel = 1;
    peer_info.encrypt = false;

    ret = esp_now_add_peer(&peer_info);
    if (ret != ESP_OK && ret != ESP_ERR_ESPNOW_EXIST) {
        ESP_LOGE(TAG, "Error agregando peer broadcast: %s", esp_err_to_name(ret));
        return false;
    }

    ESP_LOGI(TAG, "ESP-NOW inicializado :D");
    return true;
}

bool comm_send(const uint8_t *data, size_t len) {
    if (!data || len == 0 || len > 250) {
        ESP_LOGE(TAG, "Datos nulos o tamaño inválido (max 250B)");
        return false;
    }

    esp_err_t result = esp_now_send(s_broadcast_mac, data, len);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Error en esp_now_send: %s", esp_err_to_name(result));
        return false;
    }

    return true;
}

void comm_get_my_mac(uint8_t mac_out[6]) {
    if (mac_out) {
        esp_read_mac(mac_out, ESP_MAC_WIFI_STA);
    }
}
