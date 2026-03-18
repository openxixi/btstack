/*
 * EATT Test Server for BTstack examples
 */

#define BTSTACK_FILE__ "eatt_test_server.c"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "btstack.h"
#include "gatt_streamer_server.h"

// Reuse gatt_streamer_server.gatt service:
// - Service UUID:        0xFF10
// - Characteristic UUID: 0xFF11 (WRITE_WITHOUT_RESPONSE | NOTIFY)

#define APP_AD_FLAGS 0x06

#define EATT_NUM_BEARERS 3
#define EATT_STORAGE_SIZE 1536

static uint8_t eatt_storage_buffer[EATT_STORAGE_SIZE];

static const uint8_t adv_data[] = {
    // Flags general discoverable
    0x02, BLUETOOTH_DATA_TYPE_FLAGS, APP_AD_FLAGS,
    // Name: "EATT Server"
    0x0c, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, 'E', 'A', 'T', 'T', ' ', 'S', 'e', 'r', 'v', 'e', 'r',
    // Incomplete List of 16-bit Service Class UUIDs
    0x03, BLUETOOTH_DATA_TYPE_INCOMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS, 0x10, 0xff,
};
static const uint8_t adv_data_len = sizeof(adv_data);

static hci_con_handle_t connection_handle = HCI_CON_HANDLE_INVALID;
static bool notifications_enabled;
static uint32_t tx_counter;
static uint32_t rx_bytes;

static btstack_packet_callback_registration_t hci_event_callback_registration;

static void att_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static int att_write_callback(hci_con_handle_t con_handle, uint16_t att_handle, uint16_t transaction_mode,
                              uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
    UNUSED(offset);
    UNUSED(buffer);

    if (transaction_mode != ATT_TRANSACTION_MODE_NONE) return 0;

    switch (att_handle){
        case ATT_CHARACTERISTIC_0000FF11_0000_1000_8000_00805F9B34FB_01_CLIENT_CONFIGURATION_HANDLE:
            notifications_enabled = little_endian_read_16(buffer, 0) == GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION;
            printf("EATT Server: notifications %s\n", notifications_enabled ? "enabled" : "disabled");
            if (notifications_enabled){
                att_server_request_can_send_now_event(con_handle);
            }
            break;

        case ATT_CHARACTERISTIC_0000FF11_0000_1000_8000_00805F9B34FB_01_VALUE_HANDLE:
            rx_bytes += buffer_size;
            printf("EATT Server: RX %u bytes (total %" PRIu32 ")\n", buffer_size, rx_bytes);
            if (notifications_enabled){
                att_server_request_can_send_now_event(con_handle);
            }
            break;

        default:
            printf("EATT Server: write to handle 0x%04x, len %u\n", att_handle, buffer_size);
            break;
    }
    return 0;
}

static void att_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;

    switch (hci_event_packet_get_type(packet)){
        case ATT_EVENT_CONNECTED:
            connection_handle = att_event_connected_get_handle(packet);
            printf("EATT Server: ATT connected, handle 0x%04x\n", connection_handle);
            break;

        case ATT_EVENT_MTU_EXCHANGE_COMPLETE:
            printf("EATT Server: MTU exchange complete, mtu %u\n", att_event_mtu_exchange_complete_get_MTU(packet));
            break;

        case ATT_EVENT_CAN_SEND_NOW:
            if (connection_handle == HCI_CON_HANDLE_INVALID) break;
            if (!notifications_enabled) break;

            tx_counter++;
            {
                char payload[40];
                uint16_t payload_len = (uint16_t) btstack_snprintf_best_effort(payload, sizeof(payload), "EATT notify #%" PRIu32, tx_counter);
                if (payload_len > sizeof(payload)){
                    payload_len = sizeof(payload);
                }
                att_server_notify(connection_handle,
                                  ATT_CHARACTERISTIC_0000FF11_0000_1000_8000_00805F9B34FB_01_VALUE_HANDLE,
                                  (uint8_t *) payload,
                                  payload_len);
            }
            break;

        case ATT_EVENT_DISCONNECTED:
            printf("EATT Server: ATT disconnected, handle 0x%04x\n", att_event_disconnected_get_handle(packet));
            connection_handle = HCI_CON_HANDLE_INVALID;
            notifications_enabled = false;
            break;

        default:
            break;
    }
}

static void hci_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;

    if (hci_event_packet_get_type(packet) != BTSTACK_EVENT_STATE) return;

    if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING){
        printf("EATT Server: ready, advertising started\n");
    }
}

int btstack_main(void);
int btstack_main(void){
    uint8_t status;

    l2cap_init();
    sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);

    att_server_init(profile_data, NULL, att_write_callback);
    att_server_register_packet_handler(att_packet_handler);

#ifdef ENABLE_GATT_OVER_EATT
    status = att_server_eatt_init(EATT_NUM_BEARERS, eatt_storage_buffer, sizeof(eatt_storage_buffer));
    printf("EATT Server: att_server_eatt_init -> 0x%02x, bearers %u\n", status, EATT_NUM_BEARERS);
#else
    printf("EATT Server: built without ENABLE_GATT_OVER_EATT\n");
#endif

    hci_event_callback_registration.callback = &hci_event_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // setup advertisements
    gap_advertisements_set_params(0x0030, 0x0030, 0, 0, (bd_addr_t){0}, 0x07, 0x00);
    gap_advertisements_set_data(adv_data_len, (uint8_t*) adv_data);
    gap_advertisements_enable(1);

    hci_power_control(HCI_POWER_ON);
    return 0;
}
