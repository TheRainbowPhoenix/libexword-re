/* Exercise transport tracing without opening or writing to any USB device. */
#define libusb_bulk_transfer mock_bulk_transfer
#include "../src/obex.c"
#undef libusb_bulk_transfer
#include <assert.h>

static int mock_result, mock_length, mock_calls;
static const unsigned char *mock_data;

int LIBUSB_CALL mock_bulk_transfer(libusb_device_handle *handle,
	unsigned char endpoint, unsigned char *data, int length,
	int *actual, unsigned int timeout)
{
	(void)handle;
	assert(timeout == 1245);
	assert(mock_length <= length);
	++mock_calls;
	*actual = mock_length;
	if (endpoint & LIBUSB_ENDPOINT_IN)
		memcpy(data, mock_data, mock_length);
	return mock_result;
}

int main(int argc, char **argv)
{
	obex_t ctx;
	unsigned char buffer[64];
	const unsigned char response[] = {7, 0xa0, 0, 3};
	const unsigned char partial[] = {0xde, 0xad};
	const unsigned char request[] = {0, 0x81, 0, 3};
	int actual;
	assert(argc == 2);
	assert(freopen(argv[1], "w", stderr));
	memset(&ctx, 0, sizeof(ctx));
	ctx.read_endpoint_address = 0x82;
	ctx.write_endpoint_address = 0x01;
	ctx.mtu_rx = sizeof(buffer);
	ctx.rx_msg = buf_new(sizeof(buffer));
	assert(ctx.rx_msg);
	obex_verbose = 1;

	/* Sequence ACK coalesced with a complete OBEX response. */
	mock_data = response;
	mock_length = sizeof(response);
	assert(obex_verify_seq(&ctx, 7) == 1);
	assert(ctx.rx_msg->data_size == 3);
	assert(memcmp(ctx.rx_msg->data, response + 1, 3) == 0);
	assert(obex_bulk_read(&ctx, ctx.rx_msg) == 3);
	assert(mock_calls == 1); /* No second USB read for a buffered reply. */

	/* A failing read may still transfer bytes: these must be captured. */
	mock_data = partial;
	mock_length = sizeof(partial);
	mock_result = LIBUSB_ERROR_TIMEOUT;
	assert(usb_bulk(&ctx, 0x82, buffer, sizeof(buffer), &actual, 1245) == LIBUSB_ERROR_TIMEOUT);
	assert(actual == 2 && !memcmp(buffer, partial, 2));

	/* Attempted OUT bytes are available even if the write is rejected. */
	mock_length = 0;
	mock_result = LIBUSB_ERROR_PIPE;
	assert(usb_bulk(&ctx, 1, (unsigned char *)request, sizeof(request), &actual, 1245) == LIBUSB_ERROR_PIPE);
	assert(actual == 0);

	/* Verbose off must suppress tracing without suppressing the transfer. */
	obex_verbose = 0;
	mock_result = 0;
	mock_length = sizeof(request);
	assert(usb_bulk(&ctx, 1, (unsigned char *)request, sizeof(request), &actual, 1245) == 0);
	assert(actual == sizeof(request) && mock_calls == 4);
	buf_free(ctx.rx_msg);
	fclose(stderr);
	puts("USB trace mock checks passed");
	return 0;
}
