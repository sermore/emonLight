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
 * File:   buzzer.c
 * Author: Sergio
 *
 * Created on March 15, 2016, 21:15 PM
 */

#include "buzzer.h"
#include "main.h"
#include <wiringPi.h>

#define CLOCKID CLOCK_REALTIME

#define SIG_UP SIGRTMIN
#define SIG_DOWN (SIGRTMIN+1)

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

void buzzer_init() {
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

void buzzer_at_exit() {
    if (cfg.buzzer_pin != -1) {
        digitalWrite(cfg.buzzer_pin, 0);
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

void buzzer_control(double power, double elapsedkWh, struct timespec tnow) {
    if (cfg.buzzer_pin == -1)
        return;
    // data for hard and soft power limits
    int hs_limit = power >= cfg.power_hard_threshold;
    struct buzzer_config *bc = &buzzer_config[hs_limit];
    struct buzzer_power_queue *q = &pqueue[hs_limit];
    double time = time_to_double(tnow);
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

void buzzer_test() {
    int i;
    L(LOG_DEBUG, "test buzzer pin %d\n", cfg.buzzer_pin);
    for (i = 0; i < 2; i++) {
        digitalWrite(cfg.buzzer_pin, 1);
        usleep(140e3);
        digitalWrite(cfg.buzzer_pin, 0);
        usleep(2e5);
    }
    digitalWrite(cfg.buzzer_pin, 0);
}
