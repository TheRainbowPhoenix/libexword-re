/* File body validation with mocked protocol replies, no USB. */
#define obex_request mock_request
#define obex_object_getnextheader mock_header
#include "../src/exword.c"
#undef obex_request
#undef obex_object_getnextheader
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>

static unsigned declared_size, actual_size, step;
static int length_present, body_present, body_first, response;
int mock_request(obex_t *ctx, obex_object_t *obj) { (void)ctx; (void)obj; return response; }
int mock_header(obex_t *ctx, obex_object_t *obj, uint8_t *hi, obex_headerdata_t *hv, uint32_t *size)
{
	unsigned which = step++;
	(void)ctx; (void)obj;
	if (which > 1) return 0;
	if ((which == 0) == !body_first) {
		if (!length_present) return mock_header(ctx, obj, hi, hv, size);
		*hi = OBEX_HDR_LENGTH; hv->bq4 = declared_size; *size = 4;
	} else {
		if (!body_present) return mock_header(ctx, obj, hi, hv, size);
		*hi = OBEX_HDR_BODY; hv->bs = "abc"; *size = actual_size;
	}
	return 1;
}
static void check(exword_t *device, unsigned declared, unsigned actual, int has_length,
		int has_body, int reversed, int rsp, int expected)
{
	char *data = NULL; int length = -1;
	declared_size = declared; actual_size = actual; step = 0;
	length_present = has_length; body_present = has_body; body_first = reversed; response = rsp;
	assert(exword_get_file(device, "test.bin", &data, &length) == expected);
	if (expected == 0x20) {
		assert(length == actual); assert(!actual || !memcmp(data, "abc", actual));
	} else { assert(!data && length == 0); }
	free(data);
}
int main(void)
{
	exword_t device; obex_t ctx;
	memset(&device, 0, sizeof(device)); memset(&ctx, 0, sizeof(ctx)); device.obex_ctx = &ctx;
	check(&device, 3, 3, 1, 1, 0, 0x20, 0x20);
	check(&device, 3, 3, 1, 1, 1, 0x20, 0x20);
	check(&device, 0, 3, 0, 1, 0, 0x20, 0x20);
	check(&device, 4, 3, 1, 1, 0, 0x20, -1);
	check(&device, 2, 3, 1, 1, 0, 0x20, -1);
	check(&device, 0, 0, 1, 1, 0, 0x20, 0x20);
	check(&device, 0, 0, 1, 0, 0, 0x20, 0x20);
	check(&device, 3, 0, 1, 0, 0, 0x20, -1);
	check(&device, 0, 0, 0, 0, 0, 0x20, -1);
	check(&device, 0, 0, 0, 0, 0, 0x43, 0x43);
	puts("10 download validation cases passed (no hardware accessed)");
	return 0;
}
