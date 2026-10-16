/* Validate CESG502.sys-equivalent USB setup without accessing hardware. */
#define libusb_control_transfer mock_control_transfer
#include "../src/obex.c"
#undef libusb_control_transfer
#include <assert.h>

static int mock_status, calls;

int LIBUSB_CALL mock_control_transfer(libusb_device_handle *handle,
	uint8_t request_type, uint8_t request, uint16_t value, uint16_t index,
	unsigned char *data, uint16_t length, unsigned int timeout)
{
	(void)handle;
	assert(request_type == 0x41); /* OUT | vendor | interface, not device. */
	assert(request == 1 && value == 0 && index == 0);
	assert(data == NULL && length == 0 && timeout == 1245);
	++calls;
	return mock_status;
}

int main(void)
{
	obex_t ctx;
	memset(&ctx, 0, sizeof(ctx));
	assert(obex_casio_init == 0); /* Existing modes do not send it by default. */
	mock_status = 0;
	assert(obex_casio_driver_init(&ctx) == 0);
	mock_status = LIBUSB_ERROR_PIPE;
	assert(obex_casio_driver_init(&ctx) == LIBUSB_ERROR_PIPE);
	mock_status = LIBUSB_ERROR_TIMEOUT;
	assert(obex_casio_driver_init(&ctx) == LIBUSB_ERROR_TIMEOUT);
	assert(calls == 3);
	puts("Casio driver initialization setup and error tests passed (no hardware accessed)");
	return 0;
}
