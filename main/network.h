#pragma once

//================================================================

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

//================================================================

// callback para notificar cunado llega un paquete - data: puntero al buffer,
// len: cant de bytes recibidos, src_mac = MAC de la placa emisora
typedef void (*network_rx_callback_t)(const uint8_t *data, size_t len,
                                      const uint8_t *src_mac);
// si se inició bien => true - rx_cb = función que procesa los paquetes de
// entrada
bool network_initialize(network_rx_callback_t rx_cb);
// data = puntero al buffer de datos, len = tamaño en bytes - el paquete se
// encoló bien => true
bool network_send(const uint8_t *data, size_t len);
// para obtener el MAC de _esta_ placa
void network_get_my_mac(uint8_t mac_out[6]);