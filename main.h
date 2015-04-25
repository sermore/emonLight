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
    

#define PPWH 1
#define DT_MIN 1.0e-1

#define QUEUE_NAME  "/emonLight_queue"

#define CHECK(x) \
    do { \
        if (!(x)) { \
            fprintf(stderr, "%s:%d: ", __func__, __LINE__); \
            perror(#x); \
            exit(-1); \
        } \
    } while (0) \

#define TIMEOUT 5
#define EMOCMS_URL "http://emoncms.org"
#define EMOCMS_PATH "/input/bulk.json"
// http://emoncms.org/input/bulk.json?data=[[-10,16,1137],[-8,17,1437,3164],[-6,19,1412,3077]]&time=1429335402

struct send_entry {
    TAILQ_ENTRY(send_entry) entries;
    struct timespec trec;
    double dt;
    double power;
    double elapsedkWh;
    long pulseCount;
};

TAILQ_HEAD(send_queue, send_entry);

struct cfg_t {
    char *config;
    char *emocms_url;
    int daemonize;
    int receiver;
    int sender;
    int unlink_queue;
    int queue_size;
    int verbose;
    int pin;
    char* api_key;
    int node_id;
};

extern struct cfg_t cfg;

#ifdef	__cplusplus
}
#endif

#endif	/* MAIN_H */

