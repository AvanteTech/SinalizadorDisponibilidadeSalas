#include "gatt_svc.h"
#include "common.h"

extern char *message;
extern char status;

static const ble_uuid16_t lcd_svc_uuid = BLE_UUID16_INIT(0x180);
static const ble_uuid16_t write_svc_uuid = BLE_UUID16_INIT(0xDEAD);

#define MESSAGE_TIMEOUT (1000 / portTICK_PERIOD_MS)

static TickType_t last_rx_time = 0;

int device_write(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    char *rxValue = (char *)ctxt->om->om_data;
    rxValue[20] = '\0';
    ESP_LOGI("Debug", "Mensagem: %s\n", rxValue);
    TickType_t current_time = xTaskGetTickCount();

    if ((current_time - last_rx_time) > MESSAGE_TIMEOUT)
    {
        if (message != NULL)
        {
            free(message);
            message = NULL;
        }
    }

    last_rx_time = current_time;

    if (strcmp(rxValue, "O") == 0 || strcmp(rxValue, "D") == 0)
    {
        status = rxValue[0];
    }
    else if (strcmp(rxValue, "C") == 0)
    {
        if (message != NULL)
        {
            free(message);
            message = NULL;
        }
    }
    else
    {
        if (message == NULL)
        {
            message = malloc(strlen(rxValue) + 1);
            
            if (message)
            {
                strcpy(message, rxValue);
            }
        }
        else
        {
            char *new_message = realloc(message, strlen(message) + strlen(rxValue) + 1);
            if (new_message)
            {
                message = new_message;
                strcat(message, rxValue);
            }
        }
        char *newline_pos = strchr(message, '\n');
        if (newline_pos)
        {
            *newline_pos = '\0'; // Remover qualquer nova linha
        }
    }
    return 0;
}

static const struct ble_gatt_svc_def gatt_svcs[] = {
    {.type = BLE_GATT_SVC_TYPE_PRIMARY,
     .uuid = &lcd_svc_uuid.u,
     .characteristics = (struct ble_gatt_chr_def[]){
         {.uuid = &write_svc_uuid.u,
          .flags = BLE_GATT_CHR_F_WRITE,
          .access_cb = device_write},
         {0}}},
    {0}};

void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg)
{
    char buf[BLE_UUID_STR_LEN];

    switch (ctxt->op)
    {

    case BLE_GATT_REGISTER_OP_SVC:
        ESP_LOGD(TAG, "registered service %s with handle=%d",
                 ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                 ctxt->svc.handle);
        break;

    case BLE_GATT_REGISTER_OP_CHR:
        ESP_LOGD(TAG,
                 "registering characteristic %s with "
                 "def_handle=%d val_handle=%d",
                 ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                 ctxt->chr.def_handle, ctxt->chr.val_handle);
        break;

    case BLE_GATT_REGISTER_OP_DSC:
        ESP_LOGD(TAG, "registering descriptor %s with handle=%d",
                 ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                 ctxt->dsc.handle);
        break;

    default:
        assert(0);
        break;
    }
}

void gatt_svr_subscribe_cb(struct ble_gap_event *event)
{
    if (event->subscribe.conn_handle != BLE_HS_CONN_HANDLE_NONE)
    {
        ESP_LOGI(TAG, "subscribe event; conn_handle=%d attr_handle=%d",
                 event->subscribe.conn_handle, event->subscribe.attr_handle);
    }
    else
    {
        ESP_LOGI(TAG, "subscribe by nimble stack; attr_handle=%d",
                 event->subscribe.attr_handle);
    }
}

int gatt_svc_init(void)
{
    int rc;

    ble_svc_gatt_init();

    rc = ble_gatts_count_cfg(gatt_svcs);
    if (rc != 0)
    {
        return rc;
    }

    rc = ble_gatts_add_svcs(gatt_svcs);
    if (rc != 0)
    {
        return rc;
    }

    return 0;
}
