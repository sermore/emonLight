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
#include <wiringPi.h>

// Use GPIO Pin 17, which is Pin 0 for wiringPi library

#define GPIO_PIN 17

#define PPWH 1
#define DT_MIN 1.0e-4

// the event counter 
volatile long pulseCount = 0, rawCount = 0, lastPulseCount = 0;
volatile double power = 0, elapsedkWh = 0, dt = -1;
volatile struct timespec tstart, tnow, tlast = {0, 0};

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
    if (clock_gettime(CLOCK_MONOTONIC, &tnow)) {
        perror("Unable to call clock_gettime");
        return;
    }
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
        }
        //printf("YYYY R=%ld, PC=%ld, dt=%f\n", rawCount, pulseCount, dt);
    } else {
        printf("XXXXX R=%ld, PC=%ld, dt=%f\n", rawCount, pulseCount, dt);
    }
    copy_time(&tlast, tnow);
}

// -------------------------------------------------------------------------
// main

int main(void) {
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

    // display counter value every second.
    while (1) {
        if (pulseCount != lastPulseCount) {
            time_t ltime; /* calendar time */
            ltime = time(NULL); /* get current cal time */
            printf("%s: R=%ld, Pulse Count=%ld, Power=%f, Elapsed kWh=%f, dt=%f\n", asctime(localtime(&ltime)), rawCount, pulseCount, power, elapsedkWh, dt);
            lastPulseCount = pulseCount;
        }
        delay(1000); // wait 1 second
    }

    return 0;
}
