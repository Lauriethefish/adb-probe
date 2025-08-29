
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <syslog.h>

// Descriptor types as defined in the USB specification
#define DEVICE_DESCRIPTOR_TYPE 1
#define CFG_DESCRIPTOR_TYPE 2
#define INTERFACE_DESCRIPTOR_TYPE 4
#define ENDPOINT_DESCRIPTOR_TYPE 5

// USB descriptor structures in the USB specification

// This is the generic header which all descriptors have
typedef struct _usb_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
} usb_descriptor;

typedef struct _usb_device_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint16_t bcdUSB;
	uint8_t bDeviceClass;
	uint8_t bDeviceSubClass;
	uint8_t bDeviceProtocol;
	uint8_t bMaxPacketSize0;
	uint16_t idVendor;
	uint16_t idProduct;
	uint16_t bcdDevice;
	uint8_t iManufacturer;
	uint8_t iProduct;
	uint8_t iSerialNumber;
	uint8_t bNumConfigurations;
} usb_device_descriptor;

typedef struct _usb_configuration_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint16_t wTotalLength;
	uint8_t bNumInterfaces;
	uint8_t bConfigurationValue;
	uint8_t iConfiguration;
	uint8_t bmAttributes;
	uint8_t bMaxPower;
} usb_configuration_descriptor;

typedef struct _usb_interface_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bInterfaceNumber;
	uint8_t bAlternativeSetting;
	uint8_t bNumEndpoints;
	uint8_t bInterfaceClass;
	uint8_t bInterfaceSubClass;
	uint8_t bInterfaceProtocol;
	uint8_t iInterface;
} usb_interface_descriptor;

typedef struct _usb_endpoint_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bEndpointAddress;
	uint8_t bmAttributes;
	uint16_t wMaxPacketSize;
	uint8_t bInterval;
} usb_endpoint_descriptor;

// Interface details for the ADB interface
#define ADB_INTERFACE_CLASS 0xFF
#define ADB_INTERFACE_SUBCLASS 0x42
#define ADB_INTERFACE_PROTOCOL 0x1

// Reads a descriptor from the given usbDevice in /dev/bus/usb into the buffer
// The buffer must be 256 bytes in length
// Returns `true` on success
bool read_descriptor(FILE* usbDevice, usb_descriptor* buffer) {
	if(fread(buffer, sizeof(usb_descriptor), 1, usbDevice) != 1) {
		// Normally in this case, we just have EOF
		return false;
	}

	int descriptorLength = buffer->bLength;
	if(descriptorLength < 2)	{
		syslog(LOG_ERR, "Descriptor length too short");
		return false;
	}

	// Definitely no buffer overrun since bLength is only 1 byte.
	int remainingBytes = descriptorLength - sizeof(usb_descriptor);

	// Read the rest of the descriptor, skipping over the header in the output buffer
	if(fread(((char*) buffer) + sizeof(usb_descriptor), 1, remainingBytes, usbDevice) != remainingBytes) {
		syslog(LOG_ERR, "Failed to read descriptor body");
		return false;
	}  else  {
		return true;
	}
}

bool read_descriptors(int busno, int devno) {
	char filenameBuf[64];

	snprintf(filenameBuf, sizeof(filenameBuf), "/dev/bus/usb/%03d/%03d", busno, devno);
	// Definitely enough space in the buffer for two integers

	FILE* usbDevice = fopen(filenameBuf, "r");
	if(usbDevice == NULL)	{
		syslog(LOG_ERR, "Failed to open usb device");
		return false;
	}

	syslog(LOG_INFO, "Checking %s for a valid ADB interface", filenameBuf);
	
	// A USB descriptor has a `bLength` field that is only one byte, thus a descriptor is at most 256 bytes.
	char descriptorBuffer[256];
	memset(descriptorBuffer, 0, sizeof(descriptorBuffer));
	

	// Keep track of whether we're currently in an interface matching the parameters for ADB
	// Also track whether we have seen the required bulk in/bulk out pipes
	// Once we see the bulk in/out pipes within a valid ADB interface, we know we have a valid ADB device
	bool withinAdbInterface = false;
	bool gotBulkIn = false;
	bool gotBulkOut = false;
	while(read_descriptor(usbDevice, (usb_descriptor*) &descriptorBuffer)) {
		usb_descriptor* descriptor = (usb_descriptor*) &descriptorBuffer;
		if(descriptor->bDescriptorType == CFG_DESCRIPTOR_TYPE) {
			withinAdbInterface = false;
		}  else if(descriptor->bDescriptorType == INTERFACE_DESCRIPTOR_TYPE) {
			usb_interface_descriptor* if_descriptor = (usb_interface_descriptor*) descriptor;
			
			if(if_descriptor->bInterfaceClass == ADB_INTERFACE_CLASS
			&& if_descriptor->bInterfaceSubClass == ADB_INTERFACE_SUBCLASS
			&& if_descriptor->bInterfaceProtocol == ADB_INTERFACE_PROTOCOL) {
				withinAdbInterface = true;
			}	else	{
				withinAdbInterface = false;
			}
		} else if((descriptor->bDescriptorType == ENDPOINT_DESCRIPTOR_TYPE) && withinAdbInterface) {
			usb_endpoint_descriptor* ep_descriptor = (usb_endpoint_descriptor*) descriptor;
			
			// Check is a bulk endpoint
			if((ep_descriptor->bmAttributes & 0b11) == 0b10) {
				// The MSB being a `1` indicates that the endpoint direction is `IN`
				if((ep_descriptor->bEndpointAddress >> 7)) {
					gotBulkIn = true;
				}	else	{
					gotBulkOut = true;
				}
			}

			// Stop reading descriptors if both endpoints have been found.
			if(gotBulkIn && gotBulkOut) {
				break;
			}
			
		}
	}

	fclose(usbDevice);

	return gotBulkIn && gotBulkOut && withinAdbInterface;	
}

int main(int argc, char** argv)
{
	if(argc < 3)
	{
		printf("0");
		return 0;
	}

	int busno = atoi(argv[1]);
	int devno = atoi(argv[2]);
	
	bool hasAdbInterface = read_descriptors(busno, devno);
	if(hasAdbInterface) {
		syslog(LOG_INFO, "Device had an ADB interface");
		printf("1");
	}	else	{
		syslog(LOG_INFO, "Device had no ADB interface");
		printf("0");
	}

	return 0;
}
