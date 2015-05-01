/*
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

#include <sys/queue.h>

#define DT_MIN 1.0e-1
#define DT_MAX 3600

#define QUEUE_NAME  "/emonLight_queue"

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
#define EMOCMS_URL "http://emoncms.org"
/* http://emoncms.org/input/bulk.json?data=[[-10,16,1137],[-8,17,1437,3164],[-6,19,1412,3077]]&time=1429335402 */
#define EMOCMS_PATH "/input/bulk.json"

struct send_entry {
    TAILQ_ENTRY(send_entry) entries;
    struct timespec tlast;
    struct timespec trec;
    double dt;
    double power;
    double elapsedkWh;
    long pulseCount;
};

TAILQ_HEAD(send_queue, send_entry);

struct cfg_t {
    const char *config;
    const char *emocms_url;
    short daemonize;
    short receiver;
    short sender;
    short unlink_queue;
    int queue_size;
    short verbose;
    short ppkwh;
    short pin;
    char* api_key;
    int node_id;
    char *data_log;
    short data_log_defaults;
    FILE *datalog_file;
    const char *pid_path;
    char *data_store;
    const char *homedir;
    struct timespec tstart;
    struct timespec tlast;
    struct timespec tnow;
    long pulseCount;
};

extern struct cfg_t cfg;

#ifdef	__cplusplus
}
#endif

#endif	/* MAIN_H */

