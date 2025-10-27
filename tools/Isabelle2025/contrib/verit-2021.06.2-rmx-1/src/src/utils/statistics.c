/*
  --------------------------------------------------------------
  Generic module for handling options
  --------------------------------------------------------------
*/

#include "veriT-config.h"
#include <sys/time.h>
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#include "utils/general.h"
#include "utils/options.h"
#include "utils/stack.h"
#include "utils/statistics.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char* option_monitor_filename;
FILE* monitor_file;

/*
  --------------------------------------------------------------
  timeval help functions
  --------------------------------------------------------------
*/

static void
timeval_init(struct timeval* x)
{
	x->tv_sec = 0;
	x->tv_usec = 0;
}

static void
timeval_diff(struct timeval* x, struct timeval* y)
{
	x->tv_sec -= y->tv_sec;
	x->tv_usec -= y->tv_usec;

	if (x->tv_usec < 0) {
		long int nsec = x->tv_usec / 1000000;
		x->tv_usec -= 1000000 * nsec;
		x->tv_sec += nsec;
	}
}

static void
timeval_add(struct timeval* x, struct timeval* y)
{
	x->tv_sec += y->tv_sec;
	x->tv_usec += y->tv_usec;

	if (x->tv_usec > 1000000) {
		long int nsec = x->tv_usec / 1000000;
		x->tv_usec -= 1000000 * nsec;
		x->tv_sec += nsec;
	}
}

static void
timeval_current_all(struct timeval* x)
{
#ifdef HAVE_SYS_RESOURCE_H
	struct rusage usage;
	getrusage(RUSAGE_CHILDREN, &usage);
	timeval_add(x, &usage.ru_utime);
	timeval_add(x, &usage.ru_stime);
	getrusage(RUSAGE_SELF, &usage);
	timeval_add(x, &usage.ru_utime);
	timeval_add(x, &usage.ru_stime);
#endif
	/* Does nothing on systems without getrusage */
}

/*
  --------------------------------------------------------------
  time_adder
  --------------------------------------------------------------
*/

struct TStime_adder
{
	int who;
	struct timeval last_time;
	struct timeval total_time;
};
typedef struct TStime_adder* Ttime_adder;

static Ttime_adder
time_adder_new(int who)
{
	Ttime_adder result;
	MY_MALLOC(result, sizeof(struct TStime_adder));
	timeval_init(&result->total_time);
	result->who = who;
	return result;
}

static void
time_adder_free(Ttime_adder* Ptime_adder)
{
	free(*Ptime_adder);
	*Ptime_adder = NULL;
}

static void
time_adder_start(Ttime_adder time_adder)
{
#ifdef HAVE_SYS_RESOURCE_H
	struct rusage usage;
	if (!time_adder) my_error("time_adder_start: NULL pointer\n");
	timeval_init(&time_adder->last_time);
	if (time_adder->who & STATS_TIMER_CHILDREN) {
		getrusage(RUSAGE_CHILDREN, &usage);
		timeval_add(&time_adder->last_time, &usage.ru_utime);
		timeval_add(&time_adder->last_time, &usage.ru_stime);
	}
	if (time_adder->who & STATS_TIMER_SELF) {
		getrusage(RUSAGE_SELF, &usage);
		timeval_add(&time_adder->last_time, &usage.ru_utime);
		timeval_add(&time_adder->last_time, &usage.ru_stime);
	}
#endif
}

static void
time_adder_stop(Ttime_adder time_adder)
{
#ifdef HAVE_SYS_RESOURCE_H
	struct rusage usage;
	struct timeval actual_time;
	if (!time_adder) my_error("time_adder_stop: NULL pointer\n");
	timeval_init(&actual_time);
	if (time_adder->who & STATS_TIMER_CHILDREN) {
		getrusage(RUSAGE_CHILDREN, &usage);
		timeval_add(&actual_time, &usage.ru_utime);
		timeval_add(&actual_time, &usage.ru_stime);
	}
	if (time_adder->who & STATS_TIMER_SELF) {
		getrusage(RUSAGE_SELF, &usage);
		timeval_add(&actual_time, &usage.ru_utime);
		timeval_add(&actual_time, &usage.ru_stime);
	}
	timeval_diff(&actual_time, &time_adder->last_time);
	timeval_add(&time_adder->total_time, &actual_time);
#endif
}

static double
time_adder_get(Ttime_adder time_adder)
{
	double result;
	if (!time_adder) my_error("time_adder_get: NULL pointer\n");
	result = (double)time_adder->total_time.tv_sec;
	result += (double)time_adder->total_time.tv_usec / 1000000.0;
	return result;
}

static double
time_adder_intermediate_get(Ttime_adder time_adder)
{
#ifdef HAVE_SYS_RESOURCE_H
	double result;
	struct rusage usage;
	struct timeval actual_time;
	if (!time_adder) my_error("time_adder_intermediate_get: NULL pointer\n");
	result = (double)time_adder->total_time.tv_sec;
	result += (double)time_adder->total_time.tv_usec / 1000000.0;
	timeval_init(&actual_time);
	if (time_adder->who & STATS_TIMER_CHILDREN) {
		getrusage(RUSAGE_CHILDREN, &usage);
		timeval_add(&actual_time, &usage.ru_utime);
		timeval_add(&actual_time, &usage.ru_stime);
	}
	if (time_adder->who & STATS_TIMER_SELF) {
		getrusage(RUSAGE_SELF, &usage);
		timeval_add(&actual_time, &usage.ru_utime);
		timeval_add(&actual_time, &usage.ru_stime);
	}
	timeval_diff(&actual_time, &time_adder->last_time);
	result += (double)actual_time.tv_sec;
	result += (double)actual_time.tv_usec / 1000000.0;
	return result;
#else
	return 1.0; /* fake value for systems without getrusage */
#endif
}

/*
  --------------------------------------------------------------
  State tracker
  --------------------------------------------------------------
*/

Tstack_Pchar states;

unsigned
state_new(char* name)
{
	stack_push(states, name);
	return stack_size(states) - 1;
}

struct Tstate_entry
{
	unsigned state_id;
	struct timeval time;
};
typedef struct Tstate_entry Tstate_entry;

TSstack(_state_entry, Tstate_entry);

/*
  --------------------------------------------------------------
  Statistics
  --------------------------------------------------------------
*/

#define STAT_NONE 0
#define STAT_INT 1
#define STAT_UNSIGNED 1
#define STAT_TIMER 2
#define STAT_FLOAT 3
#define STAT_STATE 4

struct Tstat
{
	char *name, *desc, *form;
	unsigned type;
	union
	{
		Ttime_adder time_adder;
		Tstack_state_entry a_state_list;
		int a_int;
		unsigned a_unsigned;
		float a_float;
	} value;
};
typedef struct Tstat Tstat;

TSstack(_stat, Tstat);
Tstack_stat stats;

static void
fprint_state_list(FILE* file, Tstack_state_entry entries)
{
	unsigned i;
	Tstate_entry entry;
	for (i = 0; i < stack_size(entries); i++) {
		entry = stack_get(entries, i);
		fprintf(
			file, "\t%ld.%06ld %s\n", (long int)entry.time.tv_sec,
			(long int)entry.time.tv_usec, stack_get(states, entry.state_id));
	}
}

static void
fprint_stat(FILE* file, unsigned stat_id, char* prefix)
{
	Tstat* Pstat = &stack_get(stats, stat_id);
	unsigned state_id;
	if (Pstat->type == STAT_INT) {
		if (Pstat->name)
			fprintf(file, "%s%s=%d\n", prefix, Pstat->name, Pstat->value.a_int);
		else
			fprintf(file, "%sSC%.2u=%d\n", prefix, stat_id + 1, Pstat->value.a_int);
	} else if (Pstat->type == STAT_UNSIGNED) {
		if (Pstat->name)
			fprintf(file, "%s%s=%d\n", prefix, Pstat->name, Pstat->value.a_unsigned);
		else
			fprintf(
				file, "%sSC%.2u=%d\n", prefix, stat_id + 1, Pstat->value.a_unsigned);
	} else if (Pstat->type == STAT_FLOAT) {
		if (Pstat->name)
			fprintf(file, "%s%s=%.3f\n", prefix, Pstat->name, Pstat->value.a_float);
		else
			fprintf(
				file, "%sSC%.2u=%.3f\n", prefix, stat_id + 1, Pstat->value.a_float);
	} else if (Pstat->type == STAT_STATE) {
		state_id = stack_top(Pstat->value.a_state_list).state_id;
		if (Pstat->name)
			fprintf(
				file, "%s%s=%s\n", prefix, Pstat->name, stack_get(states, state_id));
		else
			fprintf(
				file, "%sSS%.2u=%s\n", prefix, stat_id + 1,
				stack_get(states, state_id));
		fprint_state_list(file, Pstat->value.a_state_list);
	} else {
		if (Pstat->name)
			fprintf(
				file, "%s%s=%.3f\n", prefix, Pstat->name,
				time_adder_get(Pstat->value.time_adder));
		else
			fprintf(
				file, "%sST%.2u=%.3f\n", prefix, stat_id + 1,
				time_adder_get(Pstat->value.time_adder));
	}
}

static void
write_monitor(unsigned stat_id)
{
	Tstat* Pstat = &stack_get(stats, stat_id);
	unsigned state_id;
	if (monitor_file) {
		if (Pstat->type != STAT_STATE)
			fprint_stat(monitor_file, stat_id, "");
		else {
			state_id = stack_top(Pstat->value.a_state_list).state_id;
			fprintf(
				monitor_file, "%s=%s\n", Pstat->name, stack_get(states, state_id));
		}
	}
}

unsigned
stats_counter_new(char* name, char* desc, char* form)
{
	stack_inc(stats);
	stack_top(stats).name = name;
	stack_top(stats).desc = desc;
	stack_top(stats).form = form;
	stack_top(stats).type = STAT_INT;
	stack_top(stats).value.a_int = 0;
	return stack_size(stats) - 1;
}

void
stats_easy_inc(char* name, char* desc, char* form)
{
	unsigned i;
	/* IMPROVE: use hash table to associate stat_id to name */
	for (i = 0; i < stack_size(stats); i++)
		if (!strcmp(name, stack_get(stats, i).name)) {
			assert(stack_get(stats, i).type == STAT_INT);
			stack_get(stats, i).value.a_int++;
			write_monitor(i);
			return;
		}
	stack_inc(stats);
	stack_top(stats).name = name;
	stack_top(stats).desc = desc;
	stack_top(stats).form = form;
	stack_top(stats).type = STAT_INT;
	stack_top(stats).value.a_int = 1;
	write_monitor(stack_size(stats) - 1);
}

void
stats_easy_set(char* name, char* desc, char* form, int v)
{
	unsigned i;
	/* IMPROVE: use hash table to associate stat_id to name */
	for (i = 0; i < stack_size(stats); i++)
		if (!strcmp(name, stack_get(stats, i).name)) {
			assert(stack_get(stats, i).type == STAT_INT);
			stack_get(stats, i).value.a_int = v;
			write_monitor(i);
			return;
		}
	stack_inc(stats);
	stack_top(stats).name = name;
	stack_top(stats).desc = desc;
	stack_top(stats).form = form;
	stack_top(stats).type = STAT_INT;
	stack_top(stats).value.a_int = v;
	write_monitor(stack_size(stats) - 1);
}

void
stats_easy_max(char* name, char* desc, char* form, int v)
{
	unsigned i;
	/* IMPROVE: use hash table to associate stat_id to name */
	for (i = 0; i < stack_size(stats); i++)
		if (!strcmp(name, stack_get(stats, i).name)) {
			assert(stack_get(stats, i).type == STAT_INT);
			if (stack_get(stats, i).value.a_int < v) {
				write_monitor(i);
				stack_get(stats, i).value.a_int = v;
			}
			return;
		}
	stack_inc(stats);
	stack_top(stats).name = name;
	stack_top(stats).desc = desc;
	stack_top(stats).form = form;
	stack_top(stats).type = STAT_INT;
	stack_top(stats).value.a_int = v;
	write_monitor(stack_size(stats) - 1);
}

void
stats_counter_set(unsigned stat_id, int value)
{
	assert(stat_id < stack_size(stats));
	assert(stack_get(stats, stat_id).type == STAT_INT);
	stack_get(stats, stat_id).value.a_int = value;
	write_monitor(stat_id);
}

int
stats_counter_get(unsigned stat_id)
{
	assert(stat_id < stack_size(stats));
	assert(stack_get(stats, stat_id).type == STAT_INT);
	return stack_get(stats, stat_id).value.a_int;
}

void
stats_counter_add(unsigned stat_id, int value)
{
	assert(stat_id < stack_size(stats));
	assert(stack_get(stats, stat_id).type == STAT_INT);
	stack_get(stats, stat_id).value.a_int += value;
	write_monitor(stat_id);
}

void
stats_counter_inc(unsigned stat_id)
{
	assert(stat_id < stack_size(stats));
	assert(stack_get(stats, stat_id).type == STAT_INT);
	stack_get(stats, stat_id).value.a_int++;
	write_monitor(stat_id);
}

void
stats_counter_dec(unsigned stat_id)
{
	assert(stat_id < stack_size(stats));
	assert(stack_get(stats, stat_id).type == STAT_INT);
	stack_get(stats, stat_id).value.a_int--;
	write_monitor(stat_id);
}

unsigned
stats_timer_new(char* name, char* desc, char* form, int which)
{
	if ((which & STATS_TIMER_SELF) == 0 && (which & STATS_TIMER_CHILDREN) == 0)
		my_error("stats_timer_new: no process specified");
	stack_inc(stats);
	stack_top(stats).name = name;
	stack_top(stats).desc = desc;
	stack_top(stats).form = form;
	stack_top(stats).type = STAT_TIMER;
	stack_top(stats).value.time_adder = time_adder_new(which);
	return stack_size(stats) - 1;
}

void
stats_timer_start(unsigned stat_timer_id)
{
	assert(stat_timer_id < stack_size(stats));
	assert(stack_get(stats, stat_timer_id).type == STAT_TIMER);
	time_adder_start(stack_get(stats, stat_timer_id).value.time_adder);
}

void
stats_timer_stop(unsigned stat_timer_id)
{
	assert(stat_timer_id < stack_size(stats));
	assert(stack_get(stats, stat_timer_id).type == STAT_TIMER);
	time_adder_stop(stack_get(stats, stat_timer_id).value.time_adder);
	write_monitor(stat_timer_id);
}

double
stats_timer_get(unsigned stat_timer_id)
{
	assert(stat_timer_id < stack_size(stats));
	assert(stack_get(stats, stat_timer_id).type == STAT_TIMER);
	return time_adder_intermediate_get(
		stack_get(stats, stat_timer_id).value.time_adder);
}

unsigned
stats_state_new(char* name, char* desc)
{
	Tstack_state_entry states;
	stack_INIT(states);
	stack_inc(stats);
	stack_top(stats).name = name;
	stack_top(stats).desc = desc;
	stack_top(stats).form = "state list";
	stack_top(stats).type = STAT_STATE;
	stack_top(stats).value.a_state_list = states;
	return stack_size(stats) - 1;
}

void
stats_state_switch(unsigned stat_id, unsigned state_id)
{
	Tstate_entry entry;
	Tstack_state_entry entries;
	struct timeval time;
	assert(stat_id < stack_size(stats));
	assert(state_id < stack_size(states));
	assert(stack_get(stats, stat_id).type == STAT_STATE);
	entries = stack_get(stats, stat_id).value.a_state_list;
	timeval_init(&time);
	timeval_current_all(&time);
	entry.state_id = state_id;
	entry.time = time;
	stack_push(entries, entry);
	stack_get(stats, stat_id).value.a_state_list = entries;
	write_monitor(stat_id);
}

void
stats_int(char* name, char* desc, char* form, int value)
{
	stack_inc(stats);
	stack_top(stats).name = name;
	stack_top(stats).desc = desc;
	stack_top(stats).form = form;
	stack_top(stats).type = STAT_INT;
	stack_top(stats).value.a_int = value;
	write_monitor(stack_size(stats) - 1);
}

void
stats_unsigned(char* name, char* desc, char* form, unsigned value)
{
	stack_inc(stats);
	stack_top(stats).name = name;
	stack_top(stats).desc = desc;
	stack_top(stats).form = form;
	stack_top(stats).type = STAT_UNSIGNED;
	stack_top(stats).value.a_unsigned = value;
	write_monitor(stack_size(stats) - 1);
}

void
stats_float(char* name, char* desc, char* form, float value)
{
	stack_inc(stats);
	stack_top(stats).name = name;
	stack_top(stats).desc = desc;
	stack_top(stats).form = form;
	stack_top(stats).type = STAT_FLOAT;
	stack_top(stats).value.a_float = value;
	write_monitor(stack_size(stats) - 1);
}

void
stats_fprint_formats(FILE* file)
{
	unsigned i;
	/* DD print the glossary */
	for (i = 0; i < stack_size(stats); i++) {
		Tstat* Pstat = &stack_get(stats, i);
		if (Pstat->name)
			fprintf(file, "STAT_FORM: %s: %s\n", Pstat->name, Pstat->form);
		else {
			switch (Pstat->type) {
				case STAT_TIMER:
					fprintf(file, "STAT_FORM: ST%.2u: %s\n", i + 1, Pstat->form);
					break;
				case STAT_STATE:
					fprintf(file, "STAT_FORM: SS%.2u: %s\n", i + 1, Pstat->form);
					break;
				default: fprintf(file, "STAT_FORM: SC%.2u: %s\n", i + 1, Pstat->form);
			}
		}
	}
}

void
stats_fprint_short(FILE* file)
{
	unsigned i;
	for (i = 0; i < stack_size(stats); ++i) fprint_stat(file, i, "");
}

void
stats_fprint_list(FILE* file, char** list)
{
	unsigned i, j;
	/* DD print value of each counter / timer */
	for (i = 0; i < stack_size(stats); i++) {
		Tstat* Pstat = &stack_get(stats, i);
		/* HB Avoid non-listed counters */
		if (
			(Pstat->type == STAT_INT || Pstat->type == STAT_UNSIGNED ||
			 Pstat->type == STAT_FLOAT) &&
			Pstat->name) {
			for (j = 0; *(list + j); ++j)
				if (!strcmp(*(list + j), Pstat->name)) break;
			if (!*(list + j)) continue;
		}
		fprint_stat(file, i, "STAT: ");
	}
	for (i = 0; *(list + i); ++i) free(*(list + i));
	free(list);
}

void
stats_fprint(FILE* file)
{
	unsigned i;
	/* DD print the glossary */
	for (i = 0; i < stack_size(stats); i++) {
		Tstat* Pstat = &stack_get(stats, i);
		if (Pstat->name)
			fprintf(file, "STAT_DESC: %s: %s\n", Pstat->name, Pstat->desc);
		else {
			switch (Pstat->type) {
				case STAT_TIMER:
					fprintf(file, "STAT_DESC: ST%.2u: %s\n", i + 1, Pstat->desc);
					break;
				case STAT_STATE:
					fprintf(file, "STAT_DESC: SS%.2u: %s\n", i + 1, Pstat->desc);
					break;
				default: fprintf(file, "STAT_DESC: SC%.2u: %s\n", i + 1, Pstat->desc);
			}
		}
	}
	/* DD print value of each counter / timer */
	for (i = 0; i < stack_size(stats); i++) fprint_stat(file, i, "STAT: ");
}

void
stats_init(void)
{
	stack_INIT(stats);
	stack_INIT(states);
	options_new_string(
		0, "monitor-stats", "Print statistic to the given file during runtime",
		"FILENAME", &option_monitor_filename);
}

void
stats_monitor_init(void)
{
	if (option_monitor_filename) {
		if (!(monitor_file = fopen(option_monitor_filename, "w"))) {
			my_warning(
				"Unable to open stats monitoring file %s\n", option_monitor_filename);
		} else
			setbuf(monitor_file, NULL);
	}
}

void
stats_done(void)
{
	unsigned i;
	for (i = 0; i < stack_size(stats); i++) {
		if (stack_get(stats, i).type == STAT_TIMER)
			time_adder_free(&(stack_get(stats, i).value.time_adder));
		if (stack_get(stats, i).type == STAT_STATE)
			stack_free(stack_get(stats, i).value.a_state_list);
	}
	if (monitor_file) fclose(monitor_file);
	/* TODO: free the monitor filename */
	stack_free(stats);
	stack_free(states);
}
