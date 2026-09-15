#include "usb_midi_host.h"
#include "usb/usb_host.h"
#include "usb/usb_helpers.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>

static const char *TAG = "usb_midi_host";

#define USB_HOST_TASK_PRIORITY     5
#define USB_CLIENT_TASK_PRIORITY   5
#define USB_TASK_STACK_SIZE        4096

static QueueHandle_t s_midi_queue = NULL;
static usb_midi_connection_cb_t s_conn_cb = NULL;
static void *s_conn_cb_ctx = NULL;

static usb_host_client_handle_t s_client_hdl = NULL;
static usb_device_handle_t s_dev_hdl = NULL;
static usb_transfer_t *s_in_xfer = NULL;
static uint8_t s_claimed_interface = 0;
static bool s_interface_claimed = false;
static uint8_t s_in_ep_addr = 0;
static uint16_t s_in_mps = 64;
static volatile bool s_connected = false;
static volatile bool s_transfer_in_flight = false;

static void in_transfer_cb(usb_transfer_t *transfer);

bool usb_midi_host_is_connected(void)
{
    return s_connected;
}

static void parse_usb_midi_packet(const uint8_t *packet)
{
    uint8_t cin = packet[0] & 0x0F;
    uint8_t status = packet[1];
    uint8_t data1 = packet[2];
    uint8_t data2 = packet[3];
    uint8_t channel = status & 0x0F;

    midi_message_t msg = {0};
    bool valid = false;

    switch (cin) {
    case 0x8: /* Note Off */
        msg.type = MIDI_EVENT_NOTE_OFF;
        msg.channel = channel;
        msg.data1 = data1; /* note */
        msg.data2 = data2; /* velocity */
        valid = true;
        break;

    case 0x9: /* Note On */
        msg.channel = channel;
        msg.data1 = data1; /* note */
        msg.data2 = data2; /* velocity */
        if (data2 == 0) {
            /* Velocity 0 is Note Off according to MIDI spec */
            msg.type = MIDI_EVENT_NOTE_OFF;
        } else {
            msg.type = MIDI_EVENT_NOTE_ON;
        }
        valid = true;
        break;

    case 0xB: /* Control Change */
        msg.type = MIDI_EVENT_CC;
        msg.channel = channel;
        msg.data1 = data1; /* controller */
        msg.data2 = data2; /* value */
        valid = true;
        break;

    case 0xC: /* Program Change */
        msg.type = MIDI_EVENT_PROGRAM;
        msg.channel = channel;
        msg.data1 = data1; /* program */
        msg.data2 = 0;
        valid = true;
        break;

    case 0xE: /* Pitch Bend */
        msg.type = MIDI_EVENT_PITCH_BEND;
        msg.channel = channel;
        msg.data1 = data1; /* LSB */
        msg.data2 = data2; /* MSB */
        /* Combine 7-bit LSB and 7-bit MSB into 14-bit unsigned, then center at 0 */
        msg.pitch_bend = (int16_t)(((uint16_t)data2 << 7) | (uint16_t)data1) - 8192;
        valid = true;
        break;

    default:
        /* Other CIN types (SysEx, etc.) ignored for basic keyboard play */
        break;
    }

    if (valid && s_midi_queue) {
        if (xQueueSend(s_midi_queue, &msg, 0) != pdTRUE) {
            ESP_LOGW(TAG, "MIDI queue full, dropped message type=0x%02X", msg.type);
        }
    }
}

static void in_transfer_cb(usb_transfer_t *transfer)
{
    s_transfer_in_flight = false;
    if (!transfer) return;

    if (transfer->status == USB_TRANSFER_STATUS_COMPLETED) {
        size_t bytes = transfer->actual_num_bytes;
        uint8_t *data = transfer->data_buffer;
        for (size_t i = 0; i + 4 <= bytes; i += 4) {
            parse_usb_midi_packet(&data[i]);
        }
    } else if (transfer->status == USB_TRANSFER_STATUS_CANCELED ||
               transfer->status == USB_TRANSFER_STATUS_NO_DEVICE) {
        ESP_LOGI(TAG, "IN transfer cancelled or device disconnected (status=%d)", transfer->status);
        return;
    } else {
        ESP_LOGW(TAG, "IN transfer status: %d", transfer->status);
    }

    /* Resubmit transfer while device is connected */
    if (s_connected && s_dev_hdl && transfer) {
        esp_err_t ret = usb_host_transfer_submit(transfer);
        if (ret == ESP_OK) {
            s_transfer_in_flight = true;
        } else {
            ESP_LOGW(TAG, "Failed to resubmit IN transfer: %s", esp_err_to_name(ret));
        }
    }
}

static void cleanup_device(void)
{
    s_connected = false;

    if (s_conn_cb) {
        s_conn_cb(false, s_conn_cb_ctx);
    }

    if (s_in_xfer) {
        usb_host_transfer_free(s_in_xfer);
        s_in_xfer = NULL;
    }
    s_transfer_in_flight = false;

    if (s_dev_hdl) {
        if (s_interface_claimed) {
            esp_err_t err = usb_host_interface_release(s_client_hdl, s_dev_hdl, s_claimed_interface);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "Interface release error: %s", esp_err_to_name(err));
            }
            s_interface_claimed = false;
        }
        esp_err_t err = usb_host_device_close(s_client_hdl, s_dev_hdl);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Device close error: %s", esp_err_to_name(err));
        }
        s_dev_hdl = NULL;
    }
    ESP_LOGI(TAG, "USB MIDI device cleaned up");
}

static void handle_device_connected(uint8_t dev_addr)
{
    ESP_LOGI(TAG, "New USB device detected at address %d", dev_addr);

    if (s_dev_hdl) {
        ESP_LOGW(TAG, "Device already open, ignoring address %d", dev_addr);
        return;
    }

    esp_err_t ret = usb_host_device_open(s_client_hdl, dev_addr, &s_dev_hdl);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open device at addr %d: %s", dev_addr, esp_err_to_name(ret));
        return;
    }

    const usb_device_desc_t *dev_desc = NULL;
    ret = usb_host_get_device_descriptor(s_dev_hdl, &dev_desc);
    if (ret == ESP_OK && dev_desc) {
        ESP_LOGI(TAG, "Connected USB Device: VID=0x%04X, PID=0x%04X, Class=0x%02X",
                 dev_desc->idVendor, dev_desc->idProduct, dev_desc->bDeviceClass);
    }

    const usb_config_desc_t *config_desc = NULL;
    ret = usb_host_get_active_config_descriptor(s_dev_hdl, &config_desc);
    if (ret != ESP_OK || !config_desc) {
        ESP_LOGE(TAG, "Failed to get config descriptor: %s", esp_err_to_name(ret));
        cleanup_device();
        return;
    }

    ESP_LOGI(TAG, "Config descriptor: total_len=%d, interfaces=%d",
             config_desc->wTotalLength, config_desc->bNumInterfaces);

    /* Search for Audio / MIDIStreaming interface: Class 0x01, SubClass 0x03 */
    bool midi_found = false;
    for (int i = 0; i < config_desc->bNumInterfaces; i++) {
        for (int alt = 0; alt < 4; alt++) {
            int offset = 0;
            const usb_intf_desc_t *intf = usb_parse_interface_descriptor(config_desc, i, alt, &offset);
            if (!intf) break;

            ESP_LOGI(TAG, "Intf %d (alt %d): class=0x%02X subclass=0x%02X eps=%d",
                     intf->bInterfaceNumber, intf->bAlternateSetting,
                     intf->bInterfaceClass, intf->bInterfaceSubClass, intf->bNumEndpoints);

            /* Check for Standard USB MIDIStreaming class (0x01/0x03) OR first interface with Bulk/Interrupt IN */
            bool is_midi_class = (intf->bInterfaceClass == 0x01 && intf->bInterfaceSubClass == 0x03);

            if (is_midi_class || (!midi_found && intf->bNumEndpoints > 0)) {
                ret = usb_host_interface_claim(s_client_hdl, s_dev_hdl, intf->bInterfaceNumber, intf->bAlternateSetting);
                if (ret != ESP_OK) {
                    ESP_LOGW(TAG, "Failed to claim interface %d: %s", intf->bInterfaceNumber, esp_err_to_name(ret));
                    continue;
                }

                s_claimed_interface = intf->bInterfaceNumber;
                s_interface_claimed = true;

                /* Parse endpoints for IN transfer */
                for (int ep_idx = 0; ep_idx < intf->bNumEndpoints; ep_idx++) {
                    int ep_offset = offset;
                    const usb_ep_desc_t *ep = usb_parse_endpoint_descriptor_by_index(
                        intf, ep_idx, config_desc->wTotalLength, &ep_offset);
                    if (!ep) continue;

                    ESP_LOGI(TAG, "  EP[%d]: addr=0x%02X attr=0x%02X mps=%d",
                             ep_idx, ep->bEndpointAddress, ep->bmAttributes, ep->wMaxPacketSize);

                    if (USB_EP_DESC_GET_EP_DIR(ep)) {
                        s_in_ep_addr = ep->bEndpointAddress;
                        s_in_mps = ep->wMaxPacketSize > 0 ? ep->wMaxPacketSize : 64;
                        midi_found = true;
                        ESP_LOGI(TAG, "Found MIDI IN endpoint: 0x%02X, MPS=%d", s_in_ep_addr, s_in_mps);
                        break;
                    }
                }

                if (midi_found) break;
            }
        }
        if (midi_found) break;
    }

    if (!midi_found) {
        ESP_LOGE(TAG, "No suitable MIDIStreaming interface/endpoint found");
        cleanup_device();
        return;
    }

    /* Allocate IN transfer */
    ret = usb_host_transfer_alloc(s_in_mps, 0, &s_in_xfer);
    if (ret != ESP_OK || !s_in_xfer) {
        ESP_LOGE(TAG, "Failed to allocate IN transfer buffer: %s", esp_err_to_name(ret));
        cleanup_device();
        return;
    }

    s_in_xfer->device_handle = s_dev_hdl;
    s_in_xfer->bEndpointAddress = s_in_ep_addr;
    s_in_xfer->callback = in_transfer_cb;
    s_in_xfer->context = NULL;
    s_in_xfer->num_bytes = s_in_mps;

    s_connected = true;

    ret = usb_host_transfer_submit(s_in_xfer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to submit initial IN transfer: %s", esp_err_to_name(ret));
        cleanup_device();
        return;
    }
    s_transfer_in_flight = true;

    ESP_LOGI(TAG, ">>> Nektar / USB MIDI device READY on EP 0x%02X <<<", s_in_ep_addr);

    if (s_conn_cb) {
        s_conn_cb(true, s_conn_cb_ctx);
    }
}

static void client_event_cb(const usb_host_client_event_msg_t *event_msg, void *arg)
{
    (void)arg;
    if (!event_msg) return;

    switch (event_msg->event) {
    case USB_HOST_CLIENT_EVENT_NEW_DEV:
        handle_device_connected(event_msg->new_dev.address);
        break;

    case USB_HOST_CLIENT_EVENT_DEV_GONE:
        ESP_LOGW(TAG, "USB device disconnected (dev_gone)");
        cleanup_device();
        break;

    default:
        ESP_LOGD(TAG, "USB client event: %d", event_msg->event);
        break;
    }
}

static void usb_lib_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "USB Host Library event task running");
    while (1) {
        uint32_t event_flags = 0;
        esp_err_t err = usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        if (err == ESP_OK) {
            if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
                ESP_LOGI(TAG, "USB Host: No clients registered");
                usb_host_device_free_all();
            }
        }
    }
}

static void usb_client_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "USB Host Client event task running");
    while (1) {
        usb_host_client_handle_events(s_client_hdl, portMAX_DELAY);
    }
}

esp_err_t usb_midi_host_init(QueueHandle_t midi_queue, usb_midi_connection_cb_t conn_cb, void *user_ctx)
{
    s_midi_queue = midi_queue;
    s_conn_cb = conn_cb;
    s_conn_cb_ctx = user_ctx;

    ESP_LOGI(TAG, "Installing USB Host Driver (Full-Speed OTG on GPIO 19/20)");

    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    esp_err_t ret = usb_host_install(&host_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install USB Host: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Start USB Host Lib task */
    BaseType_t task_ret = xTaskCreatePinnedToCore(
        usb_lib_task, "usb_lib", USB_TASK_STACK_SIZE, NULL, USB_HOST_TASK_PRIORITY, NULL, 0);
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create usb_lib_task");
        return ESP_FAIL;
    }

    /* Register USB client */
    const usb_host_client_config_t client_config = {
        .is_synchronous = false,
        .max_num_event_msg = 5,
        .async = {
            .client_event_callback = client_event_cb,
            .callback_arg = NULL,
        },
    };
    ret = usb_host_client_register(&client_config, &s_client_hdl);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register USB host client: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Start USB Client task */
    task_ret = xTaskCreatePinnedToCore(
        usb_client_task, "usb_client", USB_TASK_STACK_SIZE, NULL, USB_CLIENT_TASK_PRIORITY, NULL, 0);
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create usb_client_task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "USB Host MIDI driver initialized successfully");
    return ESP_OK;
}
