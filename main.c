/* 
 * File:   main.c
 * Author: Sergio
 *
 * Created on April 12, 2015, 10:21 AM
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <mqueue.h>
#include<signal.h>
#include <syslog.h>
#include <wiringPi.h>

// Use GPIO Pin 17, which is Pin 0 for wiringPi library

#define GPIO_PIN 17

#define PPWH 1
#define DT_MIN 1.0e-3

#define QUEUE_NAME  "/test_queue"
#define MAX_SIZE    1024
#define MSG_STOP    "exit"

#define CHECK(x) \
    do { \
        if (!(x)) { \
            fprintf(stderr, "%s:%d: ", __func__, __LINE__); \
            perror(#x); \
            exit(-1); \
        } \
    } while (0) \


// the event counter 
volatile int stop = 0;
mqd_t mq;

double time_diff(volatile struct timespec tend, volatile struct timespec tstart) {
    double dt = 0.0 + (tend.tv_sec - tstart.tv_sec) + (tend.tv_nsec - tstart.tv_nsec) / 1.0e9;
    //if (dt < 0)
    //printf("dt = %lld, te.s=%lld,te.n=%lld,ts.s=%lld,ts.n=%lld\n", dt, tend.tv_sec, tend.tv_nsec, tstart.tv_sec, tstart.tv_nsec);
    return dt;
}

void copy_time(volatile struct timespec *tdest, volatile struct timespec tsource) {
    tdest->tv_sec = tsource.tv_sec;
    tdest->tv_nsec = tsource.tv_nsec;
}

// -------------------------------------------------------------------------
// myInterrupt:  called every time an event occurs

void myInterrupt(void) {
    struct timespec tnow;
    if (clock_gettime(CLOCK_MONOTONIC, &tnow)) {
        perror("Unable to call clock_gettime");
        return;
    }
    CHECK(0 <= mq_send(mq, (char*) &tnow, sizeof (struct timespec), 0));
    syslog(LOG_INFO, "interrupt received");
}

void sig_handler(int signo) {
    if (signo == SIGINT || signo == SIGKILL) {
        printf("received SIGINT\n");
        stop = 1;
    }
}

// -------------------------------------------------------------------------
// main

int main(void) {

    long pulseCount = 0, rawCount = 0, lastPulseCount = 0;
    double power = 0, elapsedkWh = 0, dt = -1;
    struct timespec tstart = {0, 0}, tnow = {0, 0}, tlast = {0, 0};
    struct mq_attr attr;
    CHECK(signal(SIGINT, sig_handler) != SIG_ERR);
    openlog("rp-energy-meter", LOG_PID | LOG_CONS, LOG_USER);


    /* initialize the queue attributes */
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof (struct timespec);
    attr.mq_curmsgs = 0;

    /* create the message queue */
    mq = mq_open(QUEUE_NAME, O_RDWR | O_CREAT, 0644, &attr);
    CHECK((mqd_t) - 1 != mq);
    syslog(LOG_INFO, "messages in queue at startup %d\n", attr.mq_curmsgs);

    // sets up the wiringPi library
    //    if (wiringPiSetupGpio() < 0) {
    //        fprintf(stderr, "Unable to setup wiringPi: %s\n", strerror(errno));
    //        return 1;
    //    }
    //    pinMode(17, INPUT);
    //    pullUpDnControl(17, PUD_UP);

    if (wiringPiSetupSys() < 0) {
        fprintf(stderr, "Unable to setup wiringPi: %s\n", strerror(errno));
        return 1;
    }

    // set Pin 17/0 generate an interrupt on high-to-low transitions
    // and attach myInterrupt() to the interrupt
    if (wiringPiISR(GPIO_PIN, INT_EDGE_FALLING, &myInterrupt) < 0) {
        fprintf(stderr, "Unable to setup ISR: %s\n", strerror(errno));
        return 1;
    }

    do {
        struct timespec tnow;
        ssize_t bytes_read;
        bytes_read = mq_receive(mq, (char*) &tnow, sizeof (struct timespec), NULL);
        CHECK(bytes_read >= 0);

        if (pulseCount == 0) {
            copy_time(&tstart, tnow);
        }
        ++rawCount;
        dt = time_diff(tnow, tlast);
        if (rawCount == 1 || dt > DT_MIN) {
            ++pulseCount;
            //Calculate power
            if (pulseCount > 1) {
                power = (3600.0 / dt) / PPWH;
                //Find kwh elapsed
                elapsedkWh = (1.0 * pulseCount / (PPWH * 1000)); //multiply by 1000 to pulses per wh to kwh convert wh to kwh
                //        } else {
                //            printf("YYYY R=%ld, PC=%ld, dt=%f\n", rawCount, pulseCount, dt);
            }
            //    } else {
            //        printf("XXXXX R=%ld, PC=%ld, dt=%f\n", rawCount, pulseCount, dt);

            time_t ltime; /* calendar time */
            ltime = time(NULL); /* get current cal time */
            printf("%s: R=%ld, Pulse Count=%ld, Power=%f, Elapsed kWh=%f, dt=%f\n", asctime(localtime(&ltime)), rawCount, pulseCount, power, elapsedkWh, dt);
            syslog(LOG_INFO, "%s: R=%ld, Pulse Count=%ld, Power=%f, Elapsed kWh=%f, dt=%f\n", asctime(localtime(&ltime)), rawCount, pulseCount, power, elapsedkWh, dt);
        }
        copy_time(&tlast, tnow);
        sleep(20);
    } while (!stop);

    /* cleanup */
    CHECK(-1 != mq_getattr(mq, &attr));
    CHECK((mqd_t) - 1 != mq_close(mq));
    if (attr.mq_curmsgs == 0) {
        CHECK((mqd_t) - 1 != mq_unlink(QUEUE_NAME));
        syslog(LOG_INFO, "queue unlinked");
    }
    syslog(LOG_INFO, "exit, messages in queue %d", attr.mq_curmsgs);
    closelog();

    return 0;
}
