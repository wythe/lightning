#include <signal.h>
#include <setjmp.h>
#include <lightningd/jsonrpc.h>


#include <lightningd/params.c>
#include <common/json.c>
#include <common/json_escaped.c>
#include <ccan/array_size/array_size.h>

bool failed;
char * fail_msg;

struct command * ctx;

void command_fail(struct command *cmd, int code, const char *fmt, ...)
{
	failed = true;
	va_list ap;
	va_start(ap, fmt);
	fail_msg = tal_vfmt(cmd, fmt, ap);
	//printf("fail msg: %s\n", fail_msg);
	va_end(ap);
}

void command_fail_detailed(struct command *cmd, int code, const struct json_result *data, const char *fmt, ...)
{
	failed = true;
	va_list ap;
	va_start(ap, fmt);
	fail_msg = tal_vfmt(cmd, fmt, ap);
	fail_msg = tal_fmt(cmd, "%s data: %s", fail_msg, json_result_string(data));
	//printf("detailed fail msg: %s\n", fail_msg);
	va_end(ap);
}

/* AUTOGENERATED MOCKS START */
/* Generated stub for json_tok_newaddr */
bool json_tok_newaddr(const char *buffer UNNEEDED,
		      const jsmntok_t * tok UNNEEDED, bool * is_p2wpkh UNNEEDED)
{ fprintf(stderr, "json_tok_newaddr called!\n"); abort(); }
/* Generated stub for json_tok_wtx */
bool json_tok_wtx(const char * buffer UNNEEDED,
		  const jsmntok_t *sattok UNNEEDED,
		  struct wallet_tx *wtx UNNEEDED)
{ fprintf(stderr, "json_tok_wtx called!\n"); abort(); }
/* AUTOGENERATED MOCKS END */

struct json {
	jsmntok_t *toks;
	char *buffer;
};

static void convert_quotes(char * first)
{
	while (*first != '\0') {
		if (*first == '\'')
			*first = '"';
		first++;
	}
}

static struct json *json_parse(const tal_t *ctx, const char * str)
{
	struct json * j = tal(ctx, struct json);
	j->buffer = tal_strdup(j, str);
	convert_quotes(j->buffer);

	j->toks = tal_arr(j, jsmntok_t, 50);
	assert(j->toks);
	jsmn_parser parser;

again:
	jsmn_init(&parser);
	int ret = jsmn_parse(&parser, j->buffer, strlen(j->buffer), j->toks,
			     tal_count(j->toks));
	if (ret == JSMN_ERROR_NOMEM) {
		tal_resize(&j->toks, tal_count(j->toks) * 2);
		goto again;
	}

	if (ret <= 0) {
		printf("%s\nret = %d\n", j->buffer, ret);
		assert(0);
	}
	failed = false;
	return j;
}

static void zero_params(void)
{
	struct param_table *pt = new_param_table(ctx);
	struct json * j = json_parse(pt, "{}");
	assert(param_parse(pt, j->buffer, j->toks));

	pt = new_param_table(ctx);
	j = json_parse(pt, "[]");
	assert(param_parse(pt, j->buffer, j->toks));
}

struct sanity_t {
	char * str;
	bool failed;
	int ival;
	double dval;
	char * fail_str;
};

struct sanity_t buffers[] = {
	// pass
	{"{ 'u64' : '42', 'double' : '3.15' }", false, 42, 3.15, NULL },
	{"['42', '3.15']", false, 42, 3.15, NULL },

	// fail
	{"{'u64':'42', 'double':'3.15', 'extra':'stuff'}", true, 0, 0, "unknown parameter" },
	{"['42', '3.15', 'stuff']", true, 0, 0, "too many" },
	{"['42', '3.15', 'null']", true, 0, 0, "too many" },

	// not enough
	{"{'u64':'42'}", true, 0, 0, "missing required" },
	{"['42']", true, 0, 0, "missing required" },

	// fail wrong type
	{"{'u64':'hello', 'double':'3.15'}", true, 0, 0, "\"u64\": \"hello\"" },
	{"['3.15', '3.15', 'stuff']", true, 0, 0, "integer" },
};

static void stest(struct param_table * pt, const struct json * j, struct sanity_t *b)
{
	u64 ival;
	double dval;
	failed = false;
	param_add(pt, "u64", json_tok_u64, &ival);
	param_add(pt, "double", json_tok_double, &dval);
	if (!param_parse(pt, j->buffer, j->toks)) {
		//printf("%s\n%s\n", fail_msg, j->buffer);
		assert(failed == true);
		assert(b->failed == true);
		assert(strstr(fail_msg, b->fail_str));
	} else {
		assert(failed == false);
		assert(b->failed == false);
		// printf("%d\n", b->ival);
		assert(ival == 42);
		assert(dval > 3.1499 && b->dval < 3.1501);
	}
}

static bool sanity(void)
{
	for (int i=0; i<ARRAY_SIZE(buffers); ++i) {
		struct param_table *pt = new_param_table(ctx);
		struct json *j = json_parse(pt, buffers[i].str);
		//printf("type = %d\n", j->toks->type);
		assert(j->toks->type == JSMN_OBJECT || j->toks->type == JSMN_ARRAY);
		stest(pt, j, &buffers[i]);
	}
	return true;
}

/*
 * Make sure toks are passed through correctly, and also make sure
 * optional missing toks are set to NULL.
 */
static void tok_tok(void)
{
	{
		unsigned int n;
		const jsmntok_t * tok;
		tok = NULL;
		struct param_table *pt = new_param_table(ctx);
		struct json * j = json_parse(pt, "{ 'satoshi', '546' }");
		param_add(pt, "?satoshi", json_tok_tok, &tok);
		assert(param_parse(pt, j->buffer, j->toks));
		assert(tok);
		assert(json_tok_number(j->buffer, tok, &n));
		assert(n == 546);
		tal_free(pt);
	}
	// again with missing optional parameter
	{
		const jsmntok_t * tok;
		/* make sure it is *not* NULL */
		tok = (jsmntok_t *) 65535;
		struct param_table *pt = new_param_table(ctx);
		struct json * j = json_parse(pt, "{}");
		param_add(pt, "?satoshi", json_tok_tok, &tok);
		assert(param_parse(pt, j->buffer, j->toks));
		/* make sure it is NULL */
		assert(tok == NULL);
	}
}

/*
 * Make sure arrays with optional paramaters set to null
 * are processed correctly.
 */
static void null_params(void)
{
	struct param_table *pt = new_param_table(ctx);
	uint64_t * ints = tal_arr(pt, uint64_t, 7);
	for (int i=0; i<tal_count(ints); ++i) {
		ints[i] = i;
	}

	param_add(pt, "0", json_tok_u64, &ints[0]);
	param_add(pt, "1", json_tok_u64, &ints[1]);
	param_add(pt, "2", json_tok_u64, &ints[2]);
	param_add(pt, "3", json_tok_u64, &ints[3]);
	param_add(pt, "?4", json_tok_u64, &ints[4]);
	param_add(pt, "?5", json_tok_u64, &ints[5]);
	param_add(pt, "?6", json_tok_u64, &ints[6]);

	/* no null params */
	struct json * j = json_parse(pt, "[ '10', '11', '12', '13', '14', '15', '16']");
	assert(param_parse(pt, j->buffer, j->toks));
	for (int i=0; i<tal_count(ints); ++i) {
		// printf("%d == %ld\n", i, ints[i]);
		assert(ints[i] == i + 10);
	}

	/* missing at end */
	for (int i=0; i<tal_count(ints); ++i) {
		ints[i] = 42;
	}
	j = json_parse(pt, "[ '10', '11', '12', '13', '14']");
	assert(param_parse(pt, j->buffer, j->toks));
	assert(ints[4] == 14);
	assert(&ints[4] == param_is_set(pt, &ints[4]));
	assert(ints[5] == 42);
	assert(param_is_set(pt, &ints[5]) == false);
	assert(ints[6] == 42);

	/* too many nulls at end */
	j = json_parse(pt, "[ '10', '11', '12', '13', '14', null, null, null]");
	assert(!param_parse(pt, j->buffer, j->toks));

	/* null at end */
	for (int i=0; i<tal_count(ints); ++i) {
		ints[i] = 111;
	}
	j = json_parse(pt, "[ '10', '11', '12', '13', '14', null]");
	assert(param_parse(pt, j->buffer, j->toks));
	assert(ints[4] == 14);
	assert(ints[5] == 111);
	assert(param_is_set(pt, &ints[5]) == NULL);
	assert(ints[6] == 111);

	/* null in middle */
	for (int i=0; i<tal_count(ints); ++i) {
		ints[i] = 1 << 24;
	}

	j = json_parse(pt, "[ '10', '11', '12', '13', null, null, '16']");
	assert(param_parse(pt, j->buffer, j->toks));
	assert(ints[3] == 13);
	assert(ints[4] == 1 << 24);
	assert(ints[5] == 1 << 24);
	assert(ints[6] == 16);

	/* set required parameter to null */
	j = json_parse(pt, "[ '10', '11', '12', null, null, '15', '16']");
	assert(!param_parse(pt, j->buffer, j->toks));
}

static void add_members(struct param_table *pt, struct json_result *obj,
			struct json_result *arr,
			unsigned int *ints)
{
	char name[256];
	for (int i=0; i<tal_count(ints); ++i) {
		sprintf(name, "%d", i);
		//printf("adding: %s, %d\n", name, i);
		json_add_num(obj, name, i);
		json_add_num(arr, NULL, i);
		param_add(pt, name, json_tok_number, &ints[i]);
	}
}

/*
 * A roundabout way of initializing an array of ints to:
 * ints[0] = 0, ints[1] = 1, ... ints[499] = 499
 */
static void five_hundred_params(void)
{
	struct param_table *pt = new_param_table(ctx);
	unsigned int * ints = tal_arr(pt, unsigned int, 500);
	struct json_result *obj = new_json_result(pt);
	struct json_result *arr = new_json_result(pt);
	json_object_start(obj, NULL);
	json_array_start(arr, NULL);
	add_members(pt, obj, arr, ints);
	json_object_end(obj);
	json_array_end(arr);

	//printf("%s\n", json_result_string(obj));

	/* first test object version */
	struct json *j = json_parse(pt, obj->s);
	assert(param_parse(pt, j->buffer, j->toks));
	for (int i=0; i<tal_count(ints); ++i) {
		assert(ints[i] == i);
		ints[i] = 65535;
	}

	/* now test array */
	j = json_parse(pt, arr->s);
	assert(param_parse(pt, j->buffer, j->toks));
	for (int i=0; i<tal_count(ints); ++i) {
		assert(ints[i] == i);
	}
}

#if DEVELOPER
jmp_buf jump;
static void handle_abort(int signal_number)
{
	longjmp(jump, 1);
}

/*
 * Check to make sure there are no programming mistakes.
 */
static void bad_programmer(void)
{
	u64 ival;
	double dval;
	signal(SIGABRT, &handle_abort);

	/* check for repeated names */
	if (setjmp(jump) == 0) {
		struct param_table *pt = new_param_table(ctx);
		param_add(pt, "repeat", json_tok_u64, &ival);
		param_add(pt, "double", json_tok_double, &dval);
		param_add(pt, "repeat", json_tok_u64, &ival);
		/* shouldn't get here */
		assert(false);
	}

	if (setjmp(jump) == 0) {
		struct param_table *pt = new_param_table(ctx);
		param_add(pt, "u64", json_tok_u64, &ival);
		param_add(pt, "repeat", json_tok_double, &dval);
		param_add(pt, "repeat", json_tok_double, &dval);
		assert(false);
	}

	/* check for repeated arguments */
	if (setjmp(jump) == 0) {
		struct param_table *pt = new_param_table(ctx);
		param_add(pt, "u64", json_tok_u64, &ival);
		param_add(pt, "repeated-arg", json_tok_u64, &ival);
		assert(false);
	}

	/* check for NULL input */
	if (setjmp(jump) == 0) {
		struct param_table *pt = new_param_table(ctx);
		param_add(pt, NULL, json_tok_u64, &ival);
		assert(false);
	}

	if (setjmp(jump) == 0) {
		struct param_table *pt = new_param_table(ctx);
		param_add(pt, "u64", NULL, &ival);
		assert(false);
	}
}

/* Add required param after optional */
static void bad_optional(void)
{
	u64 ival;
	double dval;
	unsigned int msatoshi;
	double riskfactor;
	if (setjmp(jump) == 0) {
		struct param_table *pt = new_param_table(ctx);
		param_add(pt, "u64", json_tok_u64, &ival);
		param_add(pt, "double", json_tok_double, &dval);
		param_add(pt, "?msatoshi", json_tok_number, &msatoshi);
		param_add(pt, "riskfactor", json_tok_double, &riskfactor);
		assert(false);
	}
}
#endif

int main(void)
{
	setup_locale();
	ctx = tal(NULL, struct command);
	fail_msg = tal_arr(ctx, char, 10000);

	zero_params();
	sanity();
	tok_tok();
	null_params();
	five_hundred_params();
#if DEVELOPER
	bad_programmer();
	bad_optional();
#endif
	tal_free(ctx);
	printf("ok\n");
}
