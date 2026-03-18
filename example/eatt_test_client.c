/*
 * EATT Test Client for BTstack examples
 */

#define BTSTACK_FILE__ "eatt_test_client.c"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "btstack.h"

typedef enum {
    APP_OFF,
    APP_W4_SCAN_RESULT,
    APP_W4_CONNECT,
    APP_W4_EATT_CONNECTED,
    APP_W4_SERVICE,
    APP_W4_CHARACTERISTIC,
    APP_W4_ENABLE_NOTIFICATIONS,
    APP_STREAMING
} app_state_t;

static const char * const server_name = "EATT Server";

static bd_addr_t      server_addr;
static bd_addr_type_t server_addr_type;
static hci_con_handle_t connection_handle = HCI_CON_HANDLE_INVALID;

static app_state_t app_state = APP_OFF;

static gatt_client_service_t eatt_test_service;
static gatt_client_characteristic_t eatt_test_characteristic;
static gatt_client_notification_t notification_listener;
static btstack_context_callback_registration_t write_without_response_request;

#define EATT_NUM_CHANNELS 2
#define EATT_CLIENT_STORAGE_SIZE 2048
static uint8_t eatt_client_storage[EATT_CLIENT_STORAGE_SIZE];

static uint32_t write_counter;
static uint32_t notification_counter;

static btstack_packet_callback_registration_t hci_event_callback_registration;

static bool adv_report_contains_name(const char * name, const uint8_t * adv_data, uint8_t adv_len){
    ad_context_t context;
    const uint16_t name_len = (uint16_t) strlen(name);

    for (ad_iterator_init(&context, adv_len, adv_data); ad_iterator_has_more(&context); ad_iterator_next(&context)){
        const uint8_t data_type = ad_iterator_get_data_type(&context);
        const uint8_t data_size = ad_iterator_get_data_len(&context);
        const uint8_t * data    = ad_iterator_get_data(&context);

        switch (data_type){
            case BLUETOOTH_DATA_TYPE_SHORTENED_LOCAL_NAME:
            case BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME:
                if (data_size < name_len) break;
                if (memcmp(data, name, name_len) == 0) return true;
                break;
            default:
                break;
        }
    }
    return false;
}

static void request_can_write_without_response(void);

static void handle_can_write_without_response(void * context){
    UNUSED(context);

    char payload[32];
    uint16_t payload_len;
    uint8_t status;

    if (app_state != APP_STREAMING) return;

    write_counter++;
    payload_len = (uint16_t) btstack_snprintf_best_effort(payload, sizeof(payload), "EATT write #%" PRIu32, write_counter);
    if (payload_len > sizeof(payload)){
        payload_len = sizeof(payload);
    }

    status = gatt_client_write_value_of_characteristic_without_response(connection_handle,
                                                                        eatt_test_characteristic.value_handle,
                                                                        payload_len,
                                                                        (uint8_t *) payload);
    if (status == ERROR_CODE_SUCCESS){
        printf("EATT Client: write w/o response #%" PRIu32 " (%u bytes)\n", write_counter, payload_len);
    } else {
        printf("EATT Client: write failed, status 0x%02x\n", status);
    }

    request_can_write_without_response();
}

static void request_can_write_without_response(void){
    write_without_response_request.callback = &handle_can_write_without_response;
    write_without_response_request.context  = NULL;
    gatt_client_request_to_write_without_response(&write_without_response_request, connection_handle);
}

static void handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);

    uint8_t att_status;

    switch (hci_event_packet_get_type(packet)){
        case GATT_EVENT_CONNECTED:
            if (app_state != APP_W4_EATT_CONNECTED) break;
            att_status = gatt_event_connected_get_status(packet);
            if (att_status != ERROR_CODE_SUCCESS){
                printf("EATT Client: EATT setup failed, status 0x%02x\n", att_status);
                gap_disconnect(connection_handle);
                break;
            }
            printf("EATT Client: EATT connected, start service discovery\n");
            app_state = APP_W4_SERVICE;
            gatt_client_discover_primary_services_by_uuid16(handle_gatt_client_event, connection_handle, 0xFF10);
            break;

        case GATT_EVENT_SERVICE_QUERY_RESULT:
            if (app_state != APP_W4_SERVICE) break;
            gatt_event_service_query_result_get_service(packet, &eatt_test_service);
            printf("EATT Client: service found [%04x..%04x]\n",
                   eatt_test_service.start_group_handle,
                   eatt_test_service.end_group_handle);
            break;

        case GATT_EVENT_CHARACTERISTIC_QUERY_RESULT:
            if (app_state != APP_W4_CHARACTERISTIC) break;
            gatt_event_characteristic_query_result_get_characteristic(packet, &eatt_test_characteristic);
            printf("EATT Client: characteristic value_handle 0x%04x\n", eatt_test_characteristic.value_handle);
            break;

        case GATT_EVENT_NOTIFICATION:
            notification_counter++;
            printf("EATT Client: notification #%" PRIu32 ", len %u\n", notification_counter,
                   gatt_event_notification_get_value_length(packet));
            break;

        case GATT_EVENT_QUERY_COMPLETE:
            att_status = gatt_event_query_complete_get_att_status(packet);

            if (app_state == APP_W4_SERVICE){
                if ((att_status != ATT_ERROR_SUCCESS) || (eatt_test_service.start_group_handle == 0)){
                    printf("EATT Client: service query failed, ATT 0x%02x\n", att_status);
                    gap_disconnect(connection_handle);
                    break;
                }
                app_state = APP_W4_CHARACTERISTIC;
                gatt_client_discover_characteristics_for_service_by_uuid16(handle_gatt_client_event,
                                                                            connection_handle,
                                                                            &eatt_test_service,
                                                                            0xFF11);
                break;
            }

            if (app_state == APP_W4_CHARACTERISTIC){
                if ((att_status != ATT_ERROR_SUCCESS) || (eatt_test_characteristic.value_handle == 0)){
                    printf("EATT Client: characteristic query failed, ATT 0x%02x\n", att_status);
                    gap_disconnect(connection_handle);
                    break;
                }

                gatt_client_listen_for_characteristic_value_updates(&notification_listener,
                                                                    handle_gatt_client_event,
                                                                    connection_handle,
                                                                    &eatt_test_characteristic);
                app_state = APP_W4_ENABLE_NOTIFICATIONS;
                gatt_client_write_client_characteristic_configuration(handle_gatt_client_event,
                                                                      connection_handle,
                                                                      &eatt_test_characteristic,
                                                                      GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION);
                break;
            }

            if (app_state == APP_W4_ENABLE_NOTIFICATIONS){
                printf("EATT Client: notifications enabled, ATT 0x%02x\n", att_status);
                app_state = APP_STREAMING;
                request_can_write_without_response();
                break;
            }
            break;

        default:
            break;
    }
}

static void start_scanning(void){
    app_state = APP_W4_SCAN_RESULT;
    printf("EATT Client: scanning for '%s' ...\n", server_name);
    gap_set_scan_parameters(0, 0x0030, 0x0030);
    gap_start_scan();
}

static void hci_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;

    switch (hci_event_packet_get_type(packet)){
        case BTSTACK_EVENT_STATE:
            if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING){
                start_scanning();
            } else {
                app_state = APP_OFF;
            }
            break;

        case GAP_EVENT_ADVERTISING_REPORT:
            if (app_state != APP_W4_SCAN_RESULT) break;
            if (!adv_report_contains_name(server_name,
                                          gap_event_advertising_report_get_data(packet),
                                          gap_event_advertising_report_get_data_length(packet))) break;

            gap_event_advertising_report_get_address(packet, server_addr);
            server_addr_type = gap_event_advertising_report_get_address_type(packet);

            printf("EATT Client: found %s, addr %s\n", server_name, bd_addr_to_str(server_addr));
            gap_stop_scan();
            app_state = APP_W4_CONNECT;
            gap_connect(server_addr, server_addr_type);
            break;

        case HCI_EVENT_META_GAP:
            if (hci_event_gap_meta_get_subevent_code(packet) == GAP_SUBEVENT_LE_CONNECTION_COMPLETE){
                if (app_state != APP_W4_CONNECT) break;

                connection_handle = gap_subevent_le_connection_complete_get_connection_handle(packet);
                printf("EATT Client: LE connected, handle 0x%04x\n", connection_handle);

#ifdef ENABLE_GATT_OVER_EATT
                gatt_client_le_enhanced_enable(true);
                app_state = APP_W4_EATT_CONNECTED;
                {
                    uint8_t status = gatt_client_le_enhanced_connect(handle_gatt_client_event,
                                                                     connection_handle,
                                                                     EATT_NUM_CHANNELS,
                                                                     eatt_client_storage,
                                                                     sizeof(eatt_client_storage));
                    printf("EATT Client: gatt_client_le_enhanced_connect -> 0x%02x\n", status);
                    if (status != ERROR_CODE_SUCCESS){
                        gap_disconnect(connection_handle);
                    }
                }
#else
                printf("EATT Client: built without ENABLE_GATT_OVER_EATT\n");
                gap_disconnect(connection_handle);
#endif
            }
            break;

        case HCI_EVENT_DISCONNECTION_COMPLETE:
            printf("EATT Client: disconnected\n");
            connection_handle = HCI_CON_HANDLE_INVALID;
            memset(&eatt_test_service, 0, sizeof(eatt_test_service));
            memset(&eatt_test_characteristic, 0, sizeof(eatt_test_characteristic));
            write_counter = 0;
            notification_counter = 0;
            start_scanning();
            break;

        default:
            break;
    }
}

int btstack_main(void);
int btstack_main(void){
    l2cap_init();
    sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);

    gatt_client_init();

    hci_event_callback_registration.callback = &hci_event_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    hci_power_control(HCI_POWER_ON);
    return 0;
}
