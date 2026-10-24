#ifndef NETWORK_H
#define NETWORK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// callback para notificar cunado llega un paquete - data: puntero al buffer, len: cant de bytes recibidos, src_mac = MAC de la placa emisora
typedef void (*comm_rx_callback_t)(const uint8_t *data, size_t len, const uint8_t *src_mac);

// si se inició bien => true - rx_cb = función que procesa los paquetes de entrada
bool comm_init(comm_rx_callback_t rx_cb);

// data = puntero al buffer de datos, len = tamaño en bytes - el paquete se encoló bien => true
bool comm_send(const uint8_t *data, size_t len);

// para obtener el MAC de _esta_ placa
void comm_get_my_mac(uint8_t mac_out[6]);

#ifdef __cplusplus
}
#endif

#endif // ESP_NOW_COMM_H
