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

FILE *data_log_file = NULL;
struct pulse_queue receive_queue;
sig_atomic_t receive_queue_sem = 0;
volatile long rawCount = 0;
struct timespec tlast = {0L, 0L};
struct timespec tnow = {0L, 0L};
volatile long pulseCount = 0;
volatile short buzzer_enabled = 0;
volatile int buzzer_pulses = 0, buzzer_cnt = 10;
timer_t buzzer_timerup, buzzer_timerdown;
struct buzzer_power_queue pqueue[] = {
    { 0, 0 }, { 0, 0 }
};

struct buzzer_config buzzer_config[] = {
    { 0, 0, 0},
    { 0, 0, 0}
};


static void data_log_open() {
    if (cfg.data_log != NULL) {
        data_log_file = open_file(cfg.data_log, "a+");
        L(LOG_DEBUG, "log data to %s", cfg.data_log);
    }
}

static void data_log_write(struct pulse_entry *p) {
    if (data_log_file != NULL) {
        fprintf(data_log_file, "%ld,%ld,%f,%f,%ld,%ld\n", p->trec.tv_sec, p->trec.tv_nsec, p->power, p->elapsedkWh, p->pulseCount, p->rawCount);
    }
}

static void data_log_close() {
    if (data_log_file != NULL) {
        fclose(data_log_file);
        L(LOG_DEBUG, "close log file %s", cfg.data_log);
    }
}

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

static void buzzer_pqueue_init() {
    TAILQ_INIT(&pqueue[0]);
    TAILQ_INIT(&pqueue[1]);
}

void buzzer_setup() {
    if (cfg.buzzer_pin != -1) {
        buzzer_pulses = 0;
        buzzer_pqueue_init();
        buzzer_config[0].power_threshold_kwh = cfg.power_soft_threshold / 3.6e6;
        buzzer_config[0].time_threshold_sec = cfg.power_soft_threshold_time;
        buzzer_config[0].pulses_init = 1;
        buzzer_config[1].power_threshold_kwh = cfg.power_hard_threshold / 3.6e6;
        buzzer_config[1].time_threshold_sec = cfg.power_hard_threshold_time;
        buzzer_config[1].pulses_init = 4;
        buzzer_timer_setup(&buzzer_timerup, SIG_UP, buzzer_timer_handler);
        buzzer_timer_setup(&buzzer_timerdown, SIG_DOWN, buzzer_timer_handler);
    }
}

void buzzer_pqueue_push(struct buzzer_power_queue *q, double elapsedkWh, double time) {
    struct buzzer_power_entry *n = malloc(sizeof(struct buzzer_power_entry));
    n->power_acc_kwh = elapsedkWh;
    n->time_sec = time;
    TAILQ_INSERT_TAIL(q, n, entries);
}

void buzzer_pqueue_pop(struct buzzer_power_queue *q, double time_interval, double tnow) {
    while (q->tqh_first != NULL && q->tqh_first->time_sec + time_interval < tnow) {
        struct buzzer_power_entry *n = q->tqh_first;
        TAILQ_REMOVE(q, n, entries);
        free(n);
    }
}

double buzzer_pqueue_delta(struct buzzer_power_queue *q, double power_threshold_kwh, double time_threshold_sec) {
    struct buzzer_power_entry *h = TAILQ_FIRST(q);
    struct buzzer_power_entry *t = TAILQ_LAST(q, buzzer_power_queue);
    if (h != NULL) {
        double de = t->power_acc_kwh - h->power_acc_kwh;
//        double dt = t->time_sec - h->time_sec;
//        L(LOG_DEBUG, "de=%f, dt=%f, me=%f", de, dt, power_threshold_kwh * time_threshold_sec);        
        return de / (power_threshold_kwh * time_threshold_sec);
    }
    return 0;
}

int buzzer_calc_pulses(struct buzzer_power_queue *q, struct buzzer_config *bc) {
    double p = buzzer_pqueue_delta(q, bc->power_threshold_kwh, bc->time_threshold_sec);
    int buzzer_pulses = bc->pulses_init + (3.0 * p);
    L(LOG_DEBUG, "power usage warning %d, pulses=%d, p=%f", buzzer_pulses, bc->pulses_init, p);
    return buzzer_pulses;
}

static void buzzer_control(double power, double elapsedkWh) {
    if (cfg.buzzer_pin == -1)
        return;
    // data for hard and soft power limits
    int hs_limit = power >= cfg.power_hard_threshold;
    struct buzzer_config *bc = &buzzer_config[hs_limit];
    struct buzzer_power_queue *q = &pqueue[hs_limit];
    double time = time_to_double(&tnow);
    buzzer_pqueue_pop(&pqueue[0], buzzer_config[0].time_threshold_sec, time);
    buzzer_pqueue_pop(&pqueue[1], buzzer_config[1].time_threshold_sec, time);
    buzzer_pqueue_push(&pqueue[0], elapsedkWh, time);
    buzzer_pqueue_push(&pqueue[1], elapsedkWh, time);
    if (power >= cfg.power_soft_threshold || power >= cfg.power_hard_threshold) {
        if (!buzzer_enabled) {
            // buzzer config
            buzzer_enabled = 1;
            buzzer_cnt = 0;
            buzzer_pulses = buzzer_calc_pulses(q, bc);
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
            receive_queue_sem = 1;
            power = calc_power(dt);
            // calculate kwh elapsed
            elapsedkWh = 1.0 * pulseCount / cfg.ppkwh; //multiply by 1000 to pulses per wh to kwh convert wh to kwh
            struct pulse_entry *entry = malloc(sizeof(struct pulse_entry));
            populate_entry(entry, tlast, tnow, dt, power, elapsedkWh, pulseCount, rawCount);
            TAILQ_INSERT_TAIL(&receive_queue, entry, entries);
            receive_queue_sem = 0;
            buzzer_control(power, elapsedkWh);            
            data_log_write(entry);
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
    data_log_open();
    TAILQ_INIT(&receive_queue);

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
    data_log_close();
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