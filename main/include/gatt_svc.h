#ifndef GATT_SVR_H
#define GATT_SVR_H

#include "host/ble_gatt.h"
#include "services/gatt/ble_svc_gatt.h"

#include "host/ble_gap.h"

int device_write(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg);
void gatt_svr_subscribe_cb(struct ble_gap_event *event);
int gatt_svc_init(void);

#endif
