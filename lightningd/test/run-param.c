#include "config.h"
#include "../json.c"
#include "../json_escaped.c"
#include "../json_stream.c"
#include "../param.c"
#include <ccan/array_size/array_size.h>
#include <ccan/err/err.h>
#include <common/json.h>
#include <lightningd/jsonrpc.h>
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>

char *fail_msg = NULL;
bool failed = false;

static bool check_fail(void) {
	if (!failed)
		return false;
	failed = false;
	return true;
}

struct command *cmd;

void command_fail(struct command *cmd, int code, const char *fmt, ...)
{
	failed = true;
	va_list ap;
	va_start(ap, fmt);
	fail_msg = tal_vfmt(cmd, fmt, ap);
	va_end(ap);
}

/* AUTOGENERATED MOCKS START */
/* Generated stub for feerate_from_style */
u32 feerate_from_style(u32 feerate UNNEEDED, enum feerate_style style UNNEEDED)
{ fprintf(stderr, "feerate_from_style called!\n"); abort(); }
/* Generated stub for feerate_name */
const char *feerate_name(enum feerate feerate UNNEEDED)
{ fprintf(stderr, "feerate_name called!\n"); abort(); }
/* Generated stub for fmt_wireaddr_without_port */
char *fmt_wireaddr_without_port(const tal_t *ctx UNNEEDED, const struct wireaddr *a UNNEEDED)
{ fprintf(stderr, "fmt_wireaddr_without_port called!\n"); abort(); }
/* Generated stub for json_feerate_estimate */
bool json_feerate_estimate(struct command *cmd UNNEEDED,
			   u32 **feerate_per_kw UNNEEDED, enum feerate feerate UNNEEDED)
{ fprintf(stderr, "json_feerate_estimate called!\n"); abort(); }
/* AUTOGENERATED MOCKS END */

struct json {
	jsmntok_t *toks;
	char *buffer;
};

static void convert_quotes(char *first)
{
	while (*first != '\0') {
		if (*first == '\'')
			*first = '"';
		first++;
	}
}

static struct json *json_parse(const tal_t * ctx, const char *str)
{
	struct json *j = tal(ctx, struct json);
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
		assert(0);
	}
	return j;
}

static void zero_params(void)
{
	struct json *j = json_parse(cmd, "{}");
	assert(param(cmd, j->buffer, j->toks, NULL));

	j = json_parse(cmd, "[]");
	assert(param(cmd, j->buffer, j->toks, NULL));
}

struct sanity {
	char *str;
	bool failed;
	int ival;
	double dval;
	char *fail_str;
};

struct sanity buffers[] = {
	// pass
	{"['42', '3.15']", false, 42, 3.15, NULL},
	{"{ 'u64' : '42', 'double' : '3.15' }", false, 42, 3.15, NULL},

	// fail
	{"{'u64':'42', 'double':'3.15', 'extra':'stuff'}", true, 0, 0,
	 "unknown parameter"},
	{"['42', '3.15', 'stuff']", true, 0, 0, "too many"},
	{"['42', '3.15', 'null']", true, 0, 0, "too many"},

	// not enough
	{"{'u64':'42'}", true, 0, 0, "missing required"},
	{"['42']", true, 0, 0, "missing required"},

	// fail wrong type
	{"{'u64':'hello', 'double':'3.15'}", true, 0, 0, "be an unsigned 64"},
	{"['3.15', '3.15', 'stuff']", true, 0, 0, "integer"},
};

static void stest(const struct json *j, struct sanity *b)
{
	u64 *ival;
	double *dval;
	if (!param(cmd, j->buffer, j->toks,
		   p_req("u64", json_tok_u64, &ival),
		   p_req("double", json_tok_double, &dval), NULL)) {
		assert(check_fail());
		assert(b->failed == true);
		if (!strstr(fail_msg, b->fail_str)) {
			printf("%s != %s\n", fail_msg, b->fail_str);
			assert(false);
		}
	} else {
		assert(!check_fail());
		assert(b->failed == false);
		assert(*ival == 42);
		assert(*dval > 3.1499 && b->dval < 3.1501);
	}
}

static void sanity(void)
{
	for (int i = 0; i < ARRAY_SIZE(buffers); ++i) {
		struct json *j = json_parse(cmd, buffers[i].str);
		assert(j->toks->type == JSMN_OBJECT
		       || j->toks->type == JSMN_ARRAY);
		stest(j, &buffers[i]);
	}
}

/*
 * Make sure toks are passed through correctly, and also make sure
 * optional missing toks are set to NULL.
 */
static void tok_tok(void)
{
	{
		unsigned int n;
		const jsmntok_t *tok = NULL;
		struct json *j = json_parse(cmd, "{ 'satoshi', '546' }");

		assert(param(cmd, j->buffer, j->toks,
			     p_req("satoshi", json_tok_tok, &tok), NULL));
		assert(tok);
		assert(json_to_number(j->buffer, tok, &n));
		assert(n == 546);
	}
	// again with missing optional parameter
	{
		/* make sure it is *not* NULL */
		const jsmntok_t *tok = (const jsmntok_t *) 65535;

		struct json *j = json_parse(cmd, "{}");
		assert(param(cmd, j->buffer, j->toks,
			     p_opt("satoshi", json_tok_tok, &tok), NULL));

		/* make sure it *is* NULL */
		assert(tok == NULL);
	}
}

/* check for valid but duplicate json name-value pairs */
static void dup_names(void)
{
	struct json *j =
		json_parse(cmd,
			   "{ 'u64' : '42', 'u64' : '43', 'double' : '3.15' }");

	u64 *i;
	double *d;
	assert(!param(cmd, j->buffer, j->toks,
		      p_req("u64", json_tok_u64, &i),
		      p_req("double", json_tok_double, &d), NULL));
}

static void null_params(void)
{
	uint64_t **intptrs = tal_arr(cmd, uint64_t *, 7);
	/* no null params */
	struct json *j =
	    json_parse(cmd, "[ '10', '11', '12', '13', '14', '15', '16']");

	assert(param(cmd, j->buffer, j->toks,
		     p_req("0", json_tok_u64, &intptrs[0]),
		     p_req("1", json_tok_u64, &intptrs[1]),
		     p_req("2", json_tok_u64, &intptrs[2]),
		     p_req("3", json_tok_u64, &intptrs[3]),
		     p_opt_def("4", json_tok_u64, &intptrs[4], 999),
		     p_opt("5", json_tok_u64, &intptrs[5]),
		     p_opt("6", json_tok_u64, &intptrs[6]),
		     NULL));
	for (int i = 0; i < tal_count(intptrs); ++i) {
		assert(intptrs[i]);
		assert(*intptrs[i] == i + 10);
	}

	/* missing at end */
	j = json_parse(cmd, "[ '10', '11', '12', '13', '14']");
	assert(param(cmd, j->buffer, j->toks,
		     p_req("0", json_tok_u64, &intptrs[0]),
		     p_req("1", json_tok_u64, &intptrs[1]),
		     p_req("2", json_tok_u64, &intptrs[2]),
		     p_req("3", json_tok_u64, &intptrs[3]),
		     p_opt("4", json_tok_u64, &intptrs[4]),
		     p_opt("5", json_tok_u64, &intptrs[5]),
		     p_opt_def("6", json_tok_u64, &intptrs[6], 888),
		     NULL));
	assert(*intptrs[0] == 10);
	assert(*intptrs[1] == 11);
	assert(*intptrs[2] == 12);
	assert(*intptrs[3] == 13);
	assert(*intptrs[4] == 14);
	assert(!intptrs[5]);
	assert(*intptrs[6] == 888);
}

static void no_params(void)
{
	struct json *j = json_parse(cmd, "[]");
	assert(param(cmd, j->buffer, j->toks, NULL));

	j = json_parse(cmd, "[ 'unexpected' ]");
	assert(!param(cmd, j->buffer, j->toks, NULL));
}


#if DEVELOPER
/*
 * Check to make sure there are no programming mistakes.
 */
static void bad_programmer(void)
{
	u64 *ival;
	u64 *ival2;
	double *dval;
	struct json *j = json_parse(cmd, "[ '25', '546', '26' ]");

	/* check for repeated names */
	assert(!param(cmd, j->buffer, j->toks,
		      p_req("repeat", json_tok_u64, &ival),
		      p_req("double", json_tok_double, &dval),
		      p_req("repeat", json_tok_u64, &ival2), NULL));
	assert(check_fail());
	assert(strstr(fail_msg, "developer error"));

	assert(!param(cmd, j->buffer, j->toks,
		      p_req("repeat", json_tok_u64, &ival),
		      p_req("double", json_tok_double, &dval),
		      p_req("repeat", json_tok_u64, &ival), NULL));
	assert(check_fail());
	assert(strstr(fail_msg, "developer error"));

	assert(!param(cmd, j->buffer, j->toks,
		      p_req("u64", json_tok_u64, &ival),
		      p_req("repeat", json_tok_double, &dval),
		      p_req("repeat", json_tok_double, &dval), NULL));
	assert(check_fail());
	assert(strstr(fail_msg, "developer error"));

	/* check for repeated arguments */
	assert(!param(cmd, j->buffer, j->toks,
		      p_req("u64", json_tok_u64, &ival),
		      p_req("repeated-arg", json_tok_u64, &ival), NULL));
	assert(check_fail());
	assert(strstr(fail_msg, "developer error"));

	assert(!param(cmd, j->buffer, j->toks,
		      p_req("u64", (param_cbx) NULL, NULL), NULL));
	assert(check_fail());
	assert(strstr(fail_msg, "developer error"));

	/* Add required param after optional */
	j = json_parse(cmd, "[ '25', '546', '26', '1.1' ]");
	unsigned int *msatoshi;
	double *riskfactor;
	assert(!param(cmd, j->buffer, j->toks,
		      p_req("u64", json_tok_u64, &ival),
		      p_req("double", json_tok_double, &dval),
		      p_opt_def("msatoshi", json_tok_number, &msatoshi, 100),
		      p_req("riskfactor", json_tok_double, &riskfactor), NULL));
	assert(*msatoshi);
	assert(*msatoshi == 100);
	assert(check_fail());
	assert(strstr(fail_msg, "developer error"));
}
#endif

static void add_members(struct param **params,
			char **obj,
			char **arr, unsigned int **ints)
{
	for (int i = 0; i < tal_count(ints); ++i) {
		const char *name = tal_fmt(*params, "%i", i);
		if (i != 0) {
			tal_append_fmt(obj, ", ");
			tal_append_fmt(arr, ", ");
		}
		tal_append_fmt(obj, "\"%i\" : %i", i, i);
		tal_append_fmt(arr, "%i", i);
		param_add(params, name, true,
			  typesafe_cb_preargs(bool, void **,
					      json_tok_number,
					      &ints[i],
					      struct command *,
					      const char *,
					      const char *,
					      const jsmntok_t *),
			  &ints[i]);
	}
}

/*
 * A roundabout way of initializing an array of ints to:
 * ints[0] = 0, ints[1] = 1, ... ints[499] = 499
 */
static void five_hundred_params(void)
{
	struct param *params = tal_arr(NULL, struct param, 0);

	unsigned int **ints = tal_arr(params, unsigned int*, 500);
	char *obj = tal_fmt(params, "{ ");
	char *arr = tal_fmt(params, "[ ");
	add_members(&params, &obj, &arr, ints);
	tal_append_fmt(&obj, "}");
	tal_append_fmt(&arr, "]");

	/* first test object version */
	struct json *j = json_parse(params, obj);
	assert(param_arr(cmd, j->buffer, j->toks, params));
	for (int i = 0; i < tal_count(ints); ++i) {
		assert(ints[i]);
		assert(*ints[i] == i);
		*ints[i] = 65535;
	}

	/* now test array */
	j = json_parse(params, arr);
	assert(param_arr(cmd, j->buffer, j->toks, params));
	for (int i = 0; i < tal_count(ints); ++i) {
		assert(*ints[i] == i);
	}

	tal_free(params);
}

static void sendpay(void)
{
	struct json *j = json_parse(cmd, "[ 'A', '123', 'hello there' '547']");

	const jsmntok_t *routetok, *note;
	u64 *msatoshi;
	unsigned *cltv;

	if (!param(cmd, j->buffer, j->toks,
		   p_req("route", json_tok_tok, &routetok),
		   p_req("cltv", json_tok_number, &cltv),
		   p_opt("note", json_tok_tok, &note),
		   p_opt("msatoshi", json_tok_u64, &msatoshi),
		   NULL))
		assert(false);

	assert(note);
	assert(!strncmp("hello there", j->buffer + note->start,
			note->end - note->start));
	assert(msatoshi);
	assert(*msatoshi == 547);
}

static void sendpay_nulltok(void)
{
	struct json *j = json_parse(cmd, "[ 'A', '123']");

	const jsmntok_t *routetok, *note = (void *) 65535;
	u64 *msatoshi;
	unsigned *cltv;

	if (!param(cmd, j->buffer, j->toks,
		   p_req("route", json_tok_tok, &routetok),
		   p_req("cltv", json_tok_number, &cltv),
		   p_opt("note", json_tok_tok, &note),
		   p_opt("msatoshi", json_tok_u64, &msatoshi),
		   NULL))
		assert(false);

	assert(note == NULL);
	assert(msatoshi == NULL);
}

static void advanced(void)
{
	{
		struct json *j = json_parse(cmd, "[ 'lightning', 24, 'tok', 543 ]");

		struct json_escaped *label;
		u64 *msat;
		u64 *msat_opt1, *msat_opt2;
		const jsmntok_t *tok;

		assert(param(cmd, j->buffer, j->toks,
			     p_req("description", json_tok_label, &label),
			     p_req("msat", json_tok_msat, &msat),
			     p_req("tok", json_tok_tok, &tok),
			     p_opt("msat_opt1", json_tok_msat, &msat_opt1),
			     p_opt("msat_opt2", json_tok_msat, &msat_opt2),
			     NULL));
		assert(label != NULL);
		assert(streq(label->s, "lightning"));
		assert(*msat == 24);
		assert(tok);
		assert(msat_opt1);
		assert(*msat_opt1 == 543);
		assert(msat_opt2 == NULL);
	}
	{
		struct json *j = json_parse(cmd, "[ 3 'foo' ]");
		struct json_escaped *label, *foo;
		assert(param(cmd, j->buffer, j->toks,
			      p_req("label", json_tok_label, &label),
			      p_opt("foo", json_tok_label, &foo),
			      NULL));
		assert(streq(label->s, "3"));
		assert(streq(foo->s, "foo"));
	}
	{
		u64 *msat;
		u64 *msat2;
		struct json *j = json_parse(cmd, "[ 3 ]");
		assert(param(cmd, j->buffer, j->toks,
			      p_opt_def("msat", json_tok_msat, &msat, 23),
			      p_opt_def("msat2", json_tok_msat, &msat2, 53),
			      NULL));
		assert(*msat == 3);
		assert(msat2);
		assert(*msat2 == 53);
	}
}

static void advanced_fail(void)
{
	{
		struct json *j = json_parse(cmd, "[ 'anyx' ]");
		u64 *msat;
		assert(!param(cmd, j->buffer, j->toks,
			      p_req("msat", json_tok_msat, &msat),
			      NULL));
		assert(check_fail());
		assert(strstr(fail_msg, "'msat' should be a positive"
			      " number or 'any', not 'anyx'"));
	}
}

#define test_cb(cb, T, json_, value, pass) \
{ \
	struct json *j = json_parse(cmd, json_); \
	T *v; \
	bool ret = cb(cmd, "name", j->buffer, j->toks + 1, &v); \
	assert(ret == pass); \
	if (ret) { \
		assert(v); \
		assert(*v == value); \
	} \
}

static void json_tok_tests(void)
{
	test_cb(json_tok_bool, bool, "[ true ]", true, true);
	test_cb(json_tok_bool, bool, "[ false ]", false, true);
	test_cb(json_tok_bool, bool, "[ tru ]", false, false);
	test_cb(json_tok_bool, bool, "[ 1 ]", false, false);

	test_cb(json_tok_percent, double, "[ -0.01 ]", 0, false);
	test_cb(json_tok_percent, double, "[ 0.00 ]", 0, true);
	test_cb(json_tok_percent, double, "[ 1 ]", 1, true);
	test_cb(json_tok_percent, double, "[ 1.1 ]", 1.1, true);
	test_cb(json_tok_percent, double, "[ 1.01 ]", 1.01, true);
	test_cb(json_tok_percent, double, "[ 99.99 ]", 99.99, true);
	test_cb(json_tok_percent, double, "[ 100.0 ]", 100, true);
	test_cb(json_tok_percent, double, "[ 100.001 ]", 100.001, true);
	test_cb(json_tok_percent, double, "[ 1000 ]", 1000, true);
	test_cb(json_tok_percent, double, "[ 'wow' ]", 0, false);
}

static void test_invoice(struct command *cmd, const char *buffer,
			 const jsmntok_t *params)
{
	u64 *msatoshi_val;
	struct json_escaped *label_val;
	const char *desc_val;
	u64 *expiry;
	const jsmntok_t *fallbacks;
	const jsmntok_t *preimagetok;

	assert(cmd->mode == CMD_USAGE);
	if(!param(cmd, buffer, params,
		  p_req("msatoshi", json_tok_msat, &msatoshi_val),
		  p_req("label", json_tok_label, &label_val),
		  p_req("description", json_tok_escaped_string, &desc_val),
		  p_opt("expiry", json_tok_u64, &expiry),
		  p_opt("fallbacks", json_tok_array, &fallbacks),
		  p_opt("preimage", json_tok_tok, &preimagetok), NULL))
		return;

	/* should not be here since we are in the mode of CMD_USAGE
	 * and it always returns false. */
	abort();
}

static void usage(void)
{
	/* Do this to simulate a call to our pretend handler (test_invoice) */
	struct json_command invoice_command = {
		"invoice",
		test_invoice,
		"",
		false,
		""
	};

	cmd->mode = CMD_USAGE;
	cmd->json_cmd = &invoice_command;

	cmd->json_cmd->dispatch(cmd, NULL, NULL);

	assert(streq(cmd->usage,
		     "msatoshi label description "
		     "[expiry] [fallbacks] [preimage]"));

	cmd->mode = CMD_NORMAL;
}

int main(void)
{
	setup_locale();
	setup_tmpctx();
	cmd = tal(tmpctx, struct command);
	cmd->mode = CMD_NORMAL;
	cmd->allow_unused = false;
	fail_msg = tal_arr(cmd, char, 10000);

	zero_params();
	sanity();
	tok_tok();
	null_params();
	no_params();
#if DEVELOPER
	bad_programmer();
#endif
	dup_names();
	five_hundred_params();
	sendpay();
	sendpay_nulltok();
	advanced();
	advanced_fail();
	json_tok_tests();
	usage();

	tal_free(tmpctx);
	printf("run-params ok\n");
}
