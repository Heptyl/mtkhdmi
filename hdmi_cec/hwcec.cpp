#include <cutils/properties.h>

//#include <cutils/xlog.h>

#include "src/hwcec.h"

// ---------------------------------------------------------------------------

static int hwcec_device_open(
    const struct hw_module_t* module,
    const char* name,
    struct hw_device_t** device);

static struct hw_module_methods_t hwcec_module_methods = {
    .open = hwcec_device_open
};

hdmi_module_t HAL_MODULE_INFO_SYM = {
    .common = {
        .tag =                HARDWARE_MODULE_TAG,
        .module_api_version = HDMI_CEC_MODULE_API_VERSION_1_0,
        .hal_api_version =    HARDWARE_HAL_API_VERSION,
        .id =                 HDMI_CEC_HARDWARE_MODULE_ID,
        .name =               "MediaTek Hardware CEC HAL",
        .author =             "MediaTek Inc.",
        .methods =            &hwcec_module_methods,
        .dso                = NULL,
        .reserved           = {0},
    }
};

// ---------------------------------------------------------------------------

static int hwcec_add_logical_address(
    const struct hdmi_cec_device* dev, cec_logical_address_t addr)
{
    return HWCECMediator::getInstance().add_logical_address(addr);
}

static void hwcec_clear_logical_address(
    const struct hdmi_cec_device* dev)
{
     HWCECMediator::getInstance().clear_logical_address();
}

static int hwcec_get_physical_address(
    const struct hdmi_cec_device* dev, uint16_t* addr)
{
    return HWCECMediator::getInstance().get_physical_address_lock(addr);
}

static int hwcec_send_message(
    const struct hdmi_cec_device* dev, const cec_message_t* msg )
{
    return HWCECMediator::getInstance().send_message(msg);
}

static void hwcec_register_event_callback(
    const struct hdmi_cec_device* dev,
            event_callback_t callback, void* arg)
{

    HWCECMediator::getInstance().register_event_callback(callback, arg);
}

static void hwcec_get_version(
    const struct hdmi_cec_device* dev, int* version)
{
    HWCECMediator::getInstance().get_version(version);
}

static void hwcec_get_vendor_id(
    const struct hdmi_cec_device* dev, uint32_t* vendor_id)
{
     HWCECMediator::getInstance().get_vendor_id(vendor_id);
}

static void hwcec_get_port_info(
    const struct hdmi_cec_device* dev,
            struct hdmi_port_info* list[], int* total)
{
     HWCECMediator::getInstance().get_port_info(list, total);
}

static void hwcec_set_option(
    const struct hdmi_cec_device* dev, int flag, int value)
{
     HWCECMediator::getInstance().set_option(flag, value);
}

static void hwcec_set_audio_return_channel(
    const struct hdmi_cec_device* dev, int port_id, int flag)
{
     HWCECMediator::getInstance().set_audio_return_channel(port_id,flag);
}

static int hwcec_is_connected(
    const struct hdmi_cec_device* dev, int port_id)
{
    return HWCECMediator::getInstance().is_connected(port_id);
}

static int hwcec_device_close(struct hw_device_t* device)
{
    hwcec_private_device_t* dev = (hwcec_private_device_t*) device;
    if (dev)
    {
        HWCECMediator::getInstance().device_close(dev);

        free(dev);
    }
    return 0;
}

// ---------------------------------------------------------------------------

static int hwcec_device_open(
    const struct hw_module_t* module,
    const char* name,
    struct hw_device_t** device)
{
    hwcec_private_device_t* dev;

    if (strcmp(name, HDMI_CEC_HARDWARE_INTERFACE))
        return -EINVAL;

    dev = (hwcec_private_device_t*) malloc(sizeof(*dev));
    if (dev == NULL)
        return -ENOMEM;

    // initialize our state here
    memset(dev, 0, sizeof(*dev));

    // initialize the procs
    dev->base.common.tag            = HARDWARE_DEVICE_TAG;
    dev->base.common.version        = HDMI_CEC_DEVICE_API_VERSION_1_0;
    dev->base.common.module         = const_cast<hw_module_t*>(module);
    dev->base.common.close          = hwcec_device_close;

    dev->base.add_logical_address               = hwcec_add_logical_address;
    dev->base.clear_logical_address                   = hwcec_clear_logical_address;
    dev->base.get_physical_address          = hwcec_get_physical_address;
    dev->base.send_message                 = hwcec_send_message;
    dev->base.register_event_callback                 = hwcec_register_event_callback;
    dev->base.get_version         = hwcec_get_version;
    dev->base.get_vendor_id                  = hwcec_get_vendor_id;
    dev->base.get_port_info     = hwcec_get_port_info;
    dev->base.set_option  = hwcec_set_option;
    dev->base.set_audio_return_channel         = hwcec_set_audio_return_channel;
    dev->base.is_connected         = hwcec_is_connected;

    *device = &dev->base.common;

    HWCECMediator::getInstance().device_open(dev);

    return 0;
}
