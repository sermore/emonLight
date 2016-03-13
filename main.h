/*
 * emonLight: home energy monitor for Raspberry Pi.
 *  Copyright (C) 2015  Sergio Moretti
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 * File:   main.h
 * Author: Sergio
 *
 * Created on April 21, 2015, 8:47 PM
 */

#ifndef MAIN_H
#define	MAIN_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <sys/queue.h>
#include <syslog.h>
#include <libconfig.h>


#define DT_MIN 1.0e-1
#define DT_MAX 3600

#define CHECK(x) \
    do { \
        if (!(x)) { \
            fprintf(stderr, "%s:%d: ", __func__, __LINE__); \
            perror(#x); \
            exit(-1); \
        } \
    } while (0) \

#define L(log_level, fmt, ...) \
    if (cfg.verbose) { \
        fprintf(log_level > LOG_INFO ? stderr : stdout, fmt, ##__VA_ARGS__); \
        fprintf(log_level > LOG_INFO ? stderr : stdout, "\n"); \
        syslog(log_level, fmt, ##__VA_ARGS__); \
    } \

#define TIMEOUT 5
#define EMONCMS_URL "http://emoncms.org"
/* http://emoncms.org/input/bulk.json?data=[[-10,16,1137],[-8,17,1437,3164],[-6,19,1412,3077]]&time=1429335402 */
#define EMONCMS_PATH "/input/bulk.json"
    
#define EMONCMS_REMOTE "emoncms"
#define EMONLIGHT_REMOTE "emonlight"
#define EMONCMS_REMOTE_ID 1
#define EMONLIGHT_REMOTE_ID 2
    
struct pulse_entry {
    TAILQ_ENTRY(pulse_entry) entries;
    struct timespec tlast;
    struct timespec trec;
    double dt;
    double power;
    double elapsedkWh;
    long pulseCount;
    long rawCount;
};

TAILQ_HEAD(pulse_queue, pulse_entry);

struct cfg_t {
    const char *config;
    int remote;
    const char *url;
    char* api_key;
    int node_id;
    short daemonize;
    short receiver;
    short sender;
    short unlink_queue;
    int queue_size;
    short verbose;
    int ppkwh;
    int pulse_pin;
    int buzzer_pin;
    int power_soft_threshold;
    int power_soft_threshold_time;
    int power_soft_limit;
    int power_hard_threshold;
    int power_hard_threshold_time;
    int power_hard_limit;
    short buzzer_test;
    char *data_log;
    short data_log_defaults;
    const char *pid_path;
    char *data_store;
    const char *homedir;
    short help;
};

struct buzzer_config {
    double power_threshold_kwh; // power threshold 
    long time_threshold_sec; // interval length for which the power excess can be maintained
    int pulses_init; // initial number of pulses to apply
};

struct buzzer_power_entry {
    TAILQ_ENTRY(buzzer_power_entry) entries;
    double power_acc_kwh;
    double time_sec;
};

TAILQ_HEAD(buzzer_power_queue, buzzer_power_entry);

extern struct cfg_t cfg;
extern volatile int stop;
extern struct pulse_queue receive_queue;
extern sig_atomic_t receive_queue_sem;

extern FILE* open_file(const char *filepath, const char *mode);
extern struct pulse_entry* populate_entry(struct pulse_entry *entry, struct timespec tlast, struct timespec trec, double dt, double power, double elapsedkWh, long pulseCount, long rawCount);
extern void time_copy(struct timespec *tdest, struct timespec tsource);
extern double time_diff(struct timespec tend, struct timespec tstart);
extern int time_str(char *buf, uint len, struct timespec * ts);
extern double time_to_double(struct timespec *t);

extern void receiver_at_exit();
extern void receiver_init();
extern void receiver_loop();

extern void sender_at_exit();
extern void sender_init();
extern void sender_loop();

#ifdef	__cplusplus
}
#endif

#endif	/* MAIN_H */

