#include <iostream>
#include <libusb-1.0/libusb.h>
#include <stdexcept>
#include <string>

#define BUFFER_SIZE  1024  // Buffer size for reading and writing data

extern "C" {
    class USBException : public std::runtime_error {
    public:
        explicit USBException(const std::string &msg) : std::runtime_error(msg) {}
    };

    uint8_t BULK_OUT_ENDPOINT = 0;
    uint8_t BULK_IN_ENDPOINT = 0;

    int connectFX3(libusb_context **ctx, libusb_device_handle **handle, uint16_t vendor_id, uint16_t product_id) {
        if (libusb_init(ctx) < 0) {
            throw USBException("Error initializing libusb: " + std::string(libusb_error_name(libusb_init(ctx))));
        }

        *handle = libusb_open_device_with_vid_pid(*ctx, vendor_id, product_id);
        if (!*handle) {
            libusb_exit(*ctx);
            throw USBException("Error opening device: Could not find device with VID:PID " +
                                std::to_string(vendor_id) + ":" + std::to_string(product_id));
        }

        if (libusb_claim_interface(*handle, 0) < 0) {
            libusb_close(*handle);
            libusb_exit(*ctx);
            throw USBException("Error claiming interface: " + std::string(libusb_error_name(libusb_claim_interface(*handle, 0))));
        }

        std::cout << "Connected to device with VID:PID " << vendor_id << ":" << product_id << std::endl;

        libusb_config_descriptor *config_desc;
        libusb_device *device = libusb_get_device(*handle);
        
        if (libusb_get_active_config_descriptor(device, &config_desc) < 0) {
            libusb_close(*handle);
            libusb_exit(*ctx);
            throw USBException("Error retrieving configuration descriptor.");
        }

        for (int i = 0; i < config_desc->bNumInterfaces; i++) {
            const struct libusb_interface *interface = &config_desc->interface[i];
            for (int j = 0; j < interface->num_altsetting; j++) {
                const struct libusb_interface_descriptor *altsetting = &interface->altsetting[j];
                for (int k = 0; k < altsetting->bNumEndpoints; k++) {
                    const struct libusb_endpoint_descriptor *endpoint = &altsetting->endpoint[k];
                    uint8_t endpoint_address = endpoint->bEndpointAddress;
                    uint8_t endpoint_type = endpoint->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK;
                    
                    // Check for bulk endpoints
                    if (endpoint_type == LIBUSB_TRANSFER_TYPE_BULK) {
                        if (endpoint_address & LIBUSB_ENDPOINT_IN) {
                            BULK_IN_ENDPOINT = endpoint_address;
                        } else {
                            BULK_OUT_ENDPOINT = endpoint_address;
                        }
                    }
                }
            }
        }

        libusb_free_config_descriptor(config_desc);
        return 0;
    }

    int writeData(libusb_device_handle *handle, const unsigned char *data, int length) {
        unsigned char buffer[BUFFER_SIZE];

        if (length > BUFFER_SIZE) {
            throw USBException("Error: Data length exceeds buffer size");
        }

        std::copy(data, data + length, buffer);

        int bytes_sent = libusb_bulk_transfer(handle, BULK_OUT_ENDPOINT, buffer, length, nullptr, 0);
        if (bytes_sent < 0) {
            throw USBException("Error sending data to FX3: " + std::string(libusb_error_name(bytes_sent)));
        }

        return length;
    }

    int readData(libusb_device_handle *handle, unsigned char *buffer, int length) {
        if (length > BUFFER_SIZE) {
            throw USBException("Error: Requested data size exceeds buffer size!");
        }

        int bytes_received_local = 0;
        int bytes_sent = libusb_bulk_transfer(handle, BULK_IN_ENDPOINT, buffer, length, &bytes_received_local, 150);
        if (bytes_sent < 0) {
            throw USBException("Error receiving data from FX3: " + std::string(libusb_error_name(bytes_sent)));
        }

        return bytes_received_local;
    }

    void disconnectFX3(libusb_context *ctx, libusb_device_handle *handle) {
        if (handle) {
            // Clear endpoints
            if (libusb_clear_halt(handle, BULK_OUT_ENDPOINT) < 0) {
                std::cerr << "Warning: Failed to clear halt on BULK_OUT_ENDPOINT" << std::endl;
            }
            if (libusb_clear_halt(handle, BULK_IN_ENDPOINT) < 0) {
                std::cerr << "Warning: Failed to clear halt on BULK_IN_ENDPOINT" << std::endl;
            }
            libusb_release_interface(handle, 0);  // Release the claimed interface
            libusb_close(handle);  // Close the device handle
        }
        if (ctx) {
            libusb_exit(ctx);  // Exit libusb context
        }
        std::cout << "Disconnected from FX3 device" << std::endl;
    }
}
