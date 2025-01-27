#ifndef GAP_SVC_H
#define GAP_SVC_H

#include "host/ble_gap.h"
#include "services/gap/ble_svc_gap.h"

#define BLE_GAP_APPEARANCE_GENERIC_TAG 0x0200
#define BLE_GAP_URI_PREFIX_HTTPS 0x17
#define BLE_GAP_LE_ROLE_PERIPHERAL 0x00

void adv_init(void);
int gap_init(void);

#endif 
