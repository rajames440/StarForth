#include "picotest.h"

#include "utils/options.h"
#include "utils/statistics.h"
#include "symbolic/DAG.h"
#include "symbolic/veriT-DAG.h"

#include <stdio.h>
#include <stdlib.h>

PicoTestFailureLoggerProc logFailure;
#undef PICOTEST_FAILURE_LOGGER
#define PICOTEST_FAILURE_LOGGER logFailure

/* Test failure logger function. */
void
logFailure(
	const char* file, int line, const char* type, const char* test,
	const char* msg, va_list args)
{
	/* Error type. */
	printf("# [%s] ", type);

	/* Location in source code. */
	printf("%s(%d) : ", file, line);

	/* Failed expression. */
	printf("%s", test);

	/* Optional message. */
	if (msg) {
		printf(" | ");
		vprintf(msg, args);
	}

	printf("\n");
}

/* Hooks */
PicoTestCaseEnterProc logEnter;
PicoTestCaseLeaveProc logLeave;
#undef PICOTEST_CASE_ENTER
#undef PICOTEST_CASE_LEAVE
#define PICOTEST_CASE_ENTER logEnter
#define PICOTEST_CASE_LEAVE logLeave

unsigned test_count = 0;

void
logEnter(const char* name)
{
	test_count++;
}

void
logLeave(const char* name, int fail)
{
	if (!fail)
		printf("ok %u - %s\n", test_count, name);
	else
		printf("not ok %u - %s\n", test_count, name);
}

static inline void standardEnv_init(void)
{
	options_init();
	stats_init();
	DAG_init();
	DAG_smtlib_logic_init();
	DAG_smtlib_logic_set("UF");
}

static inline void standardEnv_done(void)
{
	DAG_smtlib_logic_done();
	DAG_done();
	stats_done();
	options_done();
}

PICOTEST_FIXTURE_SETUP(standardEnv) {
	standardEnv_init();
}

PICOTEST_FIXTURE_TEARDOWN(standardEnv) {
	standardEnv_done();
}

PICOTEST_CASE(createOr, standardEnv) {
	TDAG t = DAG_dup(DAG_or2(DAG_TRUE, DAG_FALSE));
	PICOTEST_ASSERT(t != DAG_NULL);
	DAG_free(t);
}

PICOTEST_CASE(createAnd, standardEnv) {
	TDAG t = DAG_dup(DAG_and2(DAG_TRUE, DAG_FALSE));
	PICOTEST_ASSERT(t != DAG_NULL);
}

#include "suits/discrimination-tree.h"
#include "suits/syntactic-unify.h"

PICOTEST_SUITE(mainSuite,
	createOr, createAnd, discriminationTreeSuite, syntacticUnifySuite)

unsigned count_tests(const PicoTestMetadata *metadata) {
	unsigned count = 0;
	if (metadata->nbSubtests == 0)
		return 1;
	for (unsigned i = 0; i < metadata->nbSubtests; ++i) {
		if (metadata->subtests[i] == NULL)
			count++;
		else {
			count = count + count_tests(metadata->subtests[i]);
		}
	}
	return count;
}

int
main(void)
{
	PicoTestMetadata *metadata;
	metadata = PICOTEST_METADATA(mainSuite);

	/* TAP header */
	printf("1..%u\n", count_tests(metadata));

	return mainSuite(NULL);
}

