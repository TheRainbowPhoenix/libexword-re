/* Registration/authentication failure handling, with a mocked device only. */
#define exword_setpath mock_setpath
#define exword_authinfo mock_authinfo
#define exword_authchallenge mock_challenge
#define exword_userid mock_userid
#define exword_list mock_list
#define exword_free_list mock_free_list
#define get_data_dir mock_data_dir
#include "../src/dict.c"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>

static const char *data_dir;
static int path_rsp, reset_rsp, user_rsp, list_rsp, sd, sd_rsp, reset_calls, user_calls, challenge_calls;
static unsigned char new_key = 0x55;
const char *mock_data_dir(void) { return data_dir; }
int mock_setpath(exword_t *device, uint8_t *path, uint8_t create)
{ (void)device; (void)path; assert(!create); return path_rsp; }
int mock_authinfo(exword_t *device, exword_authinfo_t *info)
{ (void)device; ++reset_calls; memset(info->challenge, new_key, 20); return reset_rsp; }
int mock_challenge(exword_t *device, exword_authchallenge_t c)
{ (void)device; (void)c; return ++challenge_calls > 1 ? sd_rsp : 0x20; }
int mock_userid(exword_t *device, exword_userid_t u)
{ (void)device; assert(!strcmp(u.name, "testuser")); ++user_calls; return user_rsp; }
int mock_list(exword_t *device, exword_dirent_t **entries, uint16_t *count)
{
	static exword_dirent_t e;
	(void)device; e.name = (uint8_t *)"_SD_00"; *entries = &e; *count = sd;
	return list_rsp;
}
void mock_free_list(exword_dirent_t *entries) { (void)entries; }
static void defaults(void)
{
	path_rsp = reset_rsp = user_rsp = list_rsp = sd_rsp = 0x20;
	sd = reset_calls = user_calls = challenge_calls = 0;
}
int main(int argc, char **argv)
{
	char key[20], loaded[20];
	assert(argc == 2); data_dir = argv[1]; memset(key, 0x55, sizeof(key));
	defaults(); path_rsp = 0x43;
	assert(!dict_reset(NULL, "testuser") && !reset_calls && !user_calls);
	defaults(); reset_rsp = 0x43;
	assert(!dict_reset(NULL, "testuser") && reset_calls == 1 && !user_calls);
	defaults(); user_rsp = 0x43;
	assert(!dict_reset(NULL, "testuser") && reset_calls == 1 && user_calls == 1);
	defaults(); sd = 1; sd_rsp = 0x43;
	assert(!dict_auth(NULL, "testuser", key) && !reset_calls && !user_calls);
	defaults(); user_rsp = 0x43;
	assert(!dict_auth(NULL, "testuser", key) && !reset_calls);
	defaults(); list_rsp = 0x43;
	assert(!dict_auth(NULL, "testuser", key));
	defaults(); assert(!dict_reset(NULL, "username-is-too-long") && !reset_calls);
	assert(dict_reset(NULL, "testuser") && reset_calls == 1);
	assert(_load_user_key("testuser", loaded) && !memcmp(loaded, key, 20));
	new_key = 0x66; defaults(); assert(dict_reset(NULL, "testuser"));
	memset(key, new_key, sizeof(key));
	assert(_load_user_key("testuser", loaded) && !memcmp(loaded, key, 20));
	puts("Auth/reset mock cases passed: failures, no implicit SD reset, username bounds and cached-key refresh");
	return 0;
}
