/* Regression tests based on XD-D6600 capture and Casio obex2_in.dll. No USB I/O. */
#define libusb_bulk_transfer mock_bulk_transfer
#include "../src/obex.c"
#undef libusb_bulk_transfer
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>

static const unsigned char *reply;
static int reply_size, read_step, reply_offset, fragments, short_write;
static int text2, writes;
static unsigned char profile_version = 0x20, profile_locale = 0x20;

int LIBUSB_CALL mock_bulk_transfer(libusb_device_handle *handle,
	unsigned char endpoint, unsigned char *data, int length,
	int *actual, unsigned int timeout)
{
	unsigned char modern[] = {0, 0x80, 0, 10, 0x20, 0, 0x10, 0, 0x40, 0, 0x20};
	unsigned char legacy[] = {0, 0x80, 0, 10, 0x20, 0x40, 0x10, 0, 0x40, 0, 0x20};
	int n;
	modern[4] = legacy[4] = profile_version;
	modern[10] = legacy[10] = profile_locale;
	(void)handle;
	assert(timeout == 1245);
	if (!(endpoint & LIBUSB_ENDPOINT_IN)) {
		assert(length == 11);
		assert(!memcmp(data, text2 ? modern : legacy, 11));
		*actual = length - short_write;
		++writes;
		return 0;
	}
	assert(!short_write); /* A short OUT must fail before any read. */
	if (read_step++ == 0) {
		data[0] = 0; /* Sequence ACK. */
		*actual = 1;
		return 0;
	}
	n = reply_size - reply_offset;
	assert(n > 0 && n <= length);
	if (fragments && reply_offset < 3) n = 1;
	memcpy(data, reply + reply_offset, n);
	reply_offset += n;
	*actual = n;
	return 0;
}

static void check(const unsigned char *data, int size, int expected,
		  int modern, int fragmented, int short_out)
{
	obex_t ctx;
	obex_object_t *obj;
	memset(&ctx, 0, sizeof(ctx));
	ctx.mtu_rx = ctx.mtu_tx = OBEX_DEFAULT_MTU;
	ctx.mtu_tx_max = OBEX_MAXIMUM_MTU;
	ctx.read_endpoint_address = 0x82;
	ctx.write_endpoint_address = 1;
	ctx.version = profile_version;
	ctx.locale = profile_locale;
	ctx.textloader2 = modern;
	ctx.rx_msg = buf_new(ctx.mtu_rx);
	ctx.tx_msg = buf_new(ctx.mtu_tx_max);
	assert(ctx.rx_msg && ctx.tx_msg);
	text2 = modern;
	reply = data;
	reply_size = size;
	reply_offset = read_step = writes = 0;
	fragments = fragmented;
	short_write = short_out;
	obj = obex_object_new(&ctx, OBEX_CMD_CONNECT);
	assert(obj);
	assert(obex_request(&ctx, obj) == expected);
	assert(writes == 1);
	if (short_out) assert(read_step == 0);
	if (expected == OBEX_RSP_SUCCESS) {
		assert(ctx.mtu_tx == ((data[5] << 8) | data[6]));
		assert(obj->rx_nonhdr_data->data_size == (size_t)size - 3);
	}
	if (expected == OBEX_RSP_INTERNAL_SERVER_ERROR)
		assert(obj->rx_nonhdr_data == NULL); /* 3-byte failure has no parameters. */
	obex_object_delete(&ctx, obj);
	buf_free(ctx.rx_msg);
	buf_free(ctx.tx_msg);
}

int main(void)
{
	const unsigned char rejected[] = {0xd0, 0, 3};
	const unsigned char success7[] = {0xa0, 0, 7, 0x68, 0, 0x10, 0};
	const unsigned char success9[] = {0xa0, 0, 9, 0x68, 0, 0x10, 0, 0x40, 0};
	const unsigned char success10[] = {0xa0, 0, 10, 0x68, 0, 0x10, 0, 0x40, 0, 0};
	const unsigned char success11[] = {0xa0, 0, 11, 0x68, 0, 0x10, 0, 0x40, 0, 0, 5};
	const unsigned char invalid_flags[] = {0xa0, 0, 7, 0x68, 0x40, 0x10, 0};
	const unsigned char invalid_mtu[] = {0xa0, 0, 7, 0x68, 0, 0, 1};
	const unsigned char invalid_reserved[] = {0xa0, 0, 10, 0x68, 0, 0x10, 0, 0x40, 0, 1};
	check(rejected, sizeof(rejected), 0x50, 0, 0, 0);
	check(rejected, sizeof(rejected), 0x50, 1, 0, 0);
	check(success7, sizeof(success7), 0x20, 1, 0, 0);
	check(success9, sizeof(success9), 0x20, 1, 0, 0);
	check(success10, sizeof(success10), 0x20, 1, 0, 0);
	check(success11, sizeof(success11), 0x20, 1, 0, 0);
	check(success11, sizeof(success11), 0x20, 1, 1, 0);
	check(invalid_flags, sizeof(invalid_flags), -1, 1, 0, 0);
	check(invalid_mtu, sizeof(invalid_mtu), -1, 1, 0, 0);
	check(invalid_reserved, sizeof(invalid_reserved), -1, 1, 0, 0);
	check(success7, sizeof(success7), LIBUSB_ERROR_IO, 1, 0, 1);
	{
		const unsigned char cy436[] = {0xa0, 0, 9, 0x51, 0, 8, 0, 0x40, 0};
		profile_version = 0x51;
		profile_locale = 0x60;
		check(cy436, sizeof(cy436), 0x20, 0, 1, 0);
		check(rejected, sizeof(rejected), 0x50, 0, 0, 0);
	}
	puts("13 CONNECT regression cases passed (no hardware accessed)");
	return 0;
}
