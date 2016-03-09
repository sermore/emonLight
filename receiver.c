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
 * File:   receiver.c
 * Author: Sergio
 *
 * Created on May 03, 2015, 10:21 AM
 */

#include "main.h"
#include <wiringPi.h>

#define CLOCKID CLOCK_REALTIME
#define SIG_UP SIGRTMIN
#define SIG_DOWN (SIGRTMIN+1)

volatile long rawCount = 0;
struct timespec tlast = {0L, 0L};
struct timespec tnow = {0L, 0L};
volatile long pulseCount = 0;
volatile short buzzer_enabled = 0;
volatile int buzzer_pulses = 0, buzzer_cnt = 10;
timer_t buzzer_timerup, buzzer_timerdown;
struct buzzer_alarm_tracker tracker[] = { { 0, 0L, 0, 0L }, { 0, 0L, 0, 0L } };

double calc_power(double dt) {
    return (3600000.0 / dt) / cfg.ppkwh;
}

static void buzzer_timer_handler(int sig, siginfo_t *si, void *uc) {
//    printf("Caught signal %d BC=%d, BP=%d\n", sig, buzzer_cnt, buzzer_pulses);
    if (sig == SIG_UP && buzzer_cnt < buzzer_pulses) {
//        printf("B1 %d\n", cfg.buzzer_pin);
        digitalWrite(cfg.buzzer_pin, 1);
    } else if (sig == SIG_DOWN) {
//        printf("B0 %d\n", cfg.buzzer_pin);
        digitalWrite(cfg.buzzer_pin, 0);
        ++buzzer_cnt;
        if (buzzer_cnt >= 10) {
            struct itimerspec its;
            its.it_value.tv_sec = 0L;
            its.it_value.tv_nsec = 0L;
            its.it_interval.tv_sec = 0L;
            its.it_interval.tv_nsec = 0L;
            CHECK(timer_settime(buzzer_timerup, 0, &its, NULL) != -1);
            CHECK(timer_settime(buzzer_timerdown, 0, &its, NULL) != -1);
            buzzer_cnt = 0;
            buzzer_enabled = 0;
        }
    }
}

void buzzer_timer_setup(timer_t *buzzer_timer, int sig, void *handler) {
    struct sigaction sa;
    sigset_t mask;
    struct sigevent sev;
    //L(LOG_DEBUG, "Establishing handler for signal %d\n", sig);
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = handler;
    sigemptyset(&sa.sa_mask);
    CHECK(sigaction(sig, &sa, NULL) != -1);

    /* Block timer signal temporarily */
    //L(LOG_DEBUG, "Blocking signal %d\n", sig);
    sigemptyset(&mask);
    sigaddset(&mask, sig);
    CHECK(sigprocmask(SIG_SETMASK, &mask, NULL) != -1);

    /* Create the timer */

    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = sig;
    sev.sigev_value.sival_ptr = buzzer_timer;
    CHECK(timer_create(CLOCKID, &sev, buzzer_timer) != -1);
    //printf("timer ID is 0x%lx\n", (long) buzzer_timer);
    CHECK(sigprocmask(SIG_UNBLOCK, &mask, NULL) != -1);
}

static void buzzer_setup() {
    if (cfg.buzzer_pin != -1) {
        buzzer_pulses = 0;
        tracker[0].power_threshold_kwh = cfg.power_soft_threshold / (1000.0 * 3600.0);
        tracker[0].time_threshold_sec = cfg.power_soft_threshold_time;
        tracker[1].power_threshold_kwh = cfg.power_hard_threshold / (1000.0 * 3600.0);
        tracker[1].time_threshold_sec = cfg.power_hard_threshold_time;
        buzzer_timer_setup(&buzzer_timerup, SIG_UP, buzzer_timer_handler);
        buzzer_timer_setup(&buzzer_timerdown, SIG_DOWN, buzzer_timer_handler);
    }
}

void buzzer_alarm_tracker_update(struct buzzer_alarm_tracker *t, double elapsedkWh, struct timespec *time) {
    double tnow = 0.0 + time->tv_sec + time->tv_nsec / 1.0e9;
    if (t->power_acc_kwh == 0 || t->time_sec == 0) {
        t->power_acc_kwh = elapsedkWh;
        t->time_sec = tnow;
    } else {
        double de = 0.0 + t->power_threshold_kwh * (tnow - t->time_sec) + t->power_acc_kwh - elapsedkWh;
        if (de > 0) {
            if (t->power_acc_kwh + de < elapsedkWh) {
                // saved power state is still relevant for threshold calculation
                t->time_sec = tnow - (elapsedkWh - t->power_acc_kwh) / t->power_threshold_kwh;
                t->power_acc_kwh = t->power_acc_kwh + de;
            } else {
                // saved power state is irrelevant for threshold calculation
                t->time_sec = tnow;
                t->power_acc_kwh = elapsedkWh;
            }
        }
//        L(LOG_DEBUG, "thr=%f, t->p=%f, p=%f, t=%f, t->t=%f, de=%f", t->power_threshold_kwh, elapsedkWh, t->power_acc_kwh, tnow, t->time_sec, de);
    }
}

int calc_buzzer_pulses(double power, double elapsedkWh) {
    int hs_limit = power >= cfg.power_hard_threshold;
    struct buzzer_alarm_tracker *t = &tracker[hs_limit];
    double dt_sec = 0.0 + tnow.tv_sec + tnow.tv_nsec / 1.0e9 - t->time_sec;
    float m = (0.0 + dt_sec) / t->time_threshold_sec;
    int buzzer_pulses = (hs_limit ? 4 : 1) + (3.0 * m);
    L(LOG_DEBUG, "power usage warning %d, hard=%d, dt=%f, dt_max=%ld", buzzer_pulses, hs_limit, dt_sec, t->time_threshold_sec);
    return buzzer_pulses;
}

static void buzzer_control(double power, double elapsedkWh) {
    if (cfg.buzzer_pin == -1)
        return;
    // data for hard and soft power limits
    int hs_limit = power >= cfg.power_hard_threshold;
    struct buzzer_alarm_tracker *t = &tracker[hs_limit];
    buzzer_alarm_tracker_update(t, elapsedkWh, &tnow);
    if (power >= cfg.power_soft_threshold || power >= cfg.power_hard_threshold) {
        if (!buzzer_enabled) {
            // buzzer config
            buzzer_enabled = 1;
            buzzer_cnt = 0;
            buzzer_pulses = calc_buzzer_pulses(power, elapsedkWh);
            long q = 70;
            struct itimerspec its_up;
            its_up.it_value.tv_sec = 0L;
            // set to q * 1 millisecond
            its_up.it_value.tv_nsec = 1000000L;
            its_up.it_interval.tv_sec = 0L;
            // set to q * 3 milliseconds
            its_up.it_interval.tv_nsec = q * 3000000L;
            CHECK(timer_settime(buzzer_timerup, 0, &its_up, NULL) != -1);
            struct itimerspec its_down;
            its_down.it_value.tv_sec = 0L;
            its_down.it_value.tv_nsec = q * 2000000L;
            its_down.it_interval.tv_sec = 0L;
            its_down.it_interval.tv_nsec = q * 3000000L;
            CHECK(timer_settime(buzzer_timerdown, 0, &its_down, NULL) != -1);
        }
    }
}

static void buzzer_test() {
    buzzer_control(cfg.power_soft_threshold, 0.7 * cfg.power_soft_limit / 1000);
    int i;
    for (i = 0; i < 10; i++) {
        sleep(1);
    }
}

static void pulse_interrupt(void) {
    double power = 0, elapsedkWh = 0;
    struct send_entry entry;
    char timestr[31];

    if (clock_gettime(CLOCK_REALTIME, &tnow)) {
        L(LOG_WARNING, "%s: %s", "Unable to call clock_gettime", strerror(errno));
        return;
    }

    ++rawCount;
    double dt = time_diff(tnow, tlast);
    if (rawCount == 1 || dt > DT_MIN) {
        ++pulseCount;
        //Calculate power
        if (pulseCount > 1 && dt < DT_MAX) {
            power = calc_power(dt);
            // calculate kwh elapsed
            elapsedkWh = 1.0 * pulseCount / cfg.ppkwh; //multiply by 1000 to pulses per wh to kwh convert wh to kwh
            populate_entry(&entry, tlast, tnow, dt, power, elapsedkWh, pulseCount, rawCount);
            buzzer_control(power, elapsedkWh);
            CHECK(0 <= mq_send(mq, (char*) &entry, sizeof (struct send_entry), 0));
        }
        time_str(timestr, 31, &tnow);
        L(LOG_DEBUG, "receive %s: DT=%f, P=%ld(%ld), Power=%f, Elapsed kWh=%f", timestr, dt, pulseCount, rawCount, power, elapsedkWh);
    }
    time_copy(&tlast, tnow);
}

static void data_store_load() {
    if (cfg.data_store != NULL) {
        FILE *file = open_file(cfg.data_store, "r");
        fscanf(file, "%ld\n%ld\n%ld\n", &pulseCount, &tlast.tv_sec, &tlast.tv_nsec);
        fclose(file);
        struct tm t;
        char strd[20];
        strftime(strd, sizeof (strd), "%F %T", localtime_r(&tlast.tv_sec, &t));
        L(LOG_DEBUG, "read last_time=%s, pulse count=%ld from %s", strd, pulseCount, cfg.data_store);
    }
}

static void data_store_save() {
    if (cfg.data_store != NULL && rawCount > 0) {
        FILE *file = open_file(cfg.data_store, "w");
        fprintf(file, "%ld\n%ld\n%ld\n", pulseCount, tnow.tv_sec, tnow.tv_nsec);
        fclose(file);
        struct tm t;
        char strd[20];
        strftime(strd, sizeof (strd), "%F %T", localtime_r(&tnow.tv_sec, &t));
        L(LOG_DEBUG, "write last_time=%s, pulse count=%ld to %s", strd, pulseCount, cfg.data_store);
    }
}

void receiver_init() {
    L(LOG_DEBUG, "running as receiver");

    // sets up the wiringPi library
    //    if (wiringPiSetupGpio() < 0) {
    //        fprintf(stderr, "Unable to setup wiringPi: %s\n", strerror(errno));
    //        return 1;
    //    }
    //    pinMode(17, INPUT);
    //    pullUpDnControl(17, PUD_UP);

    if (wiringPiSetupSys() < 0) {
        L(LOG_WARNING, "Unable to setup wiringPi: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    buzzer_setup();
    if (cfg.buzzer_test) {
        buzzer_test();
        exit(EXIT_FAILURE);
    }

    // set Pin 17/0 generate an interrupt on high-to-low transitions
    // and attach myInterrupt() to the interrupt
    if (wiringPiISR(cfg.pulse_pin, INT_EDGE_FALLING, &pulse_interrupt) < 0) {
        L(LOG_WARNING, "Unable to setup ISR: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    data_store_load();
}

void receiver_at_exit() {
    data_store_save();
    if (cfg.buzzer_pin != -1) {
        digitalWrite(cfg.buzzer_pin, 0);
    }
}

void receiver_loop() {
    if (!cfg.sender) {
        sleep(TIMEOUT);
    }
}