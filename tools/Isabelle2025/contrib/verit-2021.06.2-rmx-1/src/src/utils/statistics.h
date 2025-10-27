/*
  --------------------------------------------------------------
  Generic module for handling statistics
  --------------------------------------------------------------
*/

#ifndef __STATISTICS_H
#define __STATISTICS_H

#include <stdio.h>

#define STATS_TIMER_SELF 1
#define STATS_TIMER_CHILDREN 2
#define STATS_TIMER_ALL 3

void stats_init(void);
void stats_monitor_init(void);
void stats_done(void);

/**
   \brief creates a new counter (integer) stat, with name and description
   \param name the name of the statistic
   \param desc a description
   \param form a format string
   \return the id of the stat */
unsigned stats_counter_new(char* name, char* desc, char* form);

/**
   \brief sets the value of counter
   \param stat_id id of statistic
   \param value the value to set */
void stats_counter_set(unsigned stat_id, int value);

/**
   \brief gets the value of counter
   \param stat_id id of statistic
   \return the value of the counter */
int stats_counter_get(unsigned stat_id);

/**
   \brief adds a value to counter
   \param stat_id id of statistic
   \param value the value to add */
void stats_counter_add(unsigned stat_id, int value);

/**
   \brief decrement counter
   \param stat_id id of statistic */
void stats_counter_inc(unsigned stat_id);

/**
   \brief decrement counter
   \param stat_id id of statistic */
void stats_counter_dec(unsigned stat_id);

/**
   \brief increment counter associated to desc
   \param name the name of the statistic
   \param desc textual description.  Be careful of conflicts
   \param form a format string
   \remark automatically create counter at first call */
void stats_easy_inc(char* name, char* desc, char* form);

/**
   \brief set statistic associated to desc
   \param name the name of the statistic
   \param desc textual description.  Be careful of conflicts
   \param form a format string
   \param v the value
   \remark automatically create counter at first call */
void stats_easy_set(char* name, char* desc, char* form, int v);

/**
   \brief set statistic associated to desc to max
   \param name the name of the statistic
   \param desc textual description.  Be careful of conflicts
   \param form a format string
   \param v the value
   \remark automatically create counter at first call */
void stats_easy_max(char* name, char* desc, char* form, int v);

/**
   \brief creates a new timer stat, with name and description
   \param name the name of the statistic
   \param desc a description
   \param form a format string
   \param which flags to select process and children to take time into
   account
   \return the id of the timer */
unsigned stats_timer_new(char* name, char* desc, char* form, int which);

/**
   \brief starts timer
   \param stat_timer_id id of timer */
void stats_timer_start(unsigned stat_timer_id);

/**
   \brief stop timer
   \param stat_timer_id id of timer */
void stats_timer_stop(unsigned stat_timer_id);

/**
   \brief get timer (in seconds)
   \param stat_timer_id id of timer */
double stats_timer_get(unsigned stat_timer_id);

/**
   \brief creates a new solver state
   \param name the name of the state
   \return the id of the state */
unsigned state_new(char* name);

/**
   \brief creates a new state tracking stat, with name and description
   \param name the name of the statistic
   \param desc a description
   \param form a format string (not used)
   \return the id of the state tracker */
unsigned stats_state_new(char* name, char* desc);

/**
   \brief records a sate change together with the current cpu
          time used so far.
   \param stat_id the id of the state tracker
   \param state_id the id of the state to switch to */
void stats_state_switch(unsigned stat_id, unsigned state_id);

/**
   \brief sets an integer statistic, with name and description
   \param name the name of the statistic
   \param desc a description
   \param form a format string
   \param value the value to set
   \remark sort of contraction of stats_counter_new and stats_counter_set */
void stats_int(char* name, char* desc, char* form, int value);

/**
   \brief sets an unsigned integer statistic, with name and description
   \param name the name of the statistic
   \param desc a description
   \param form a format string
   \param value the value to set
   \remark like stats_int but for unsigned */
void stats_unsigned(char* name, char* desc, char* form, unsigned value);

/**
   \brief sets a float statistic, with name and description
   \param name the name of the statistic
   \param desc a description
   \param form a format string
   \param value the value to set
   \remark like stats_int but for float */
void stats_float(char* name, char* desc, char* form, float value);

/**
   \brief prints statistics to file
   \param file */
void stats_fprint(FILE* file);

/**
   \brief prints list of statistics to file
   \param file
   \param list */
void stats_fprint_list(FILE* file, char** list);

/**
   \brief prints formats of statistics to file
   \param file */
void stats_fprint_formats(FILE* file);

/**
   \brief prints short-form statistics to file
   \param file */
void stats_fprint_short(FILE* file);

#endif /* __STATISTICS */
