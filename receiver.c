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

#include "receiver.h"
#include "main.h"
#include "buzzer.h"
#include <wiringPi.h>

struct receive_queue rec_queue;
sig_atomic_t receive_queue_sem = 0;


static void pulse_interrupt(void) {
    struct receive_entry *e = malloc(sizeof(struct receive_entry));
    if (clock_gettime(CLOCK_REALTIME, &e->time)) {
        L(LOG_WARNING, "%s: %s", "Unable to call clock_gettime", strerror(errno));
        return;
    }
    TAILQ_INSERT_TAIL(&rec_queue, e, entries);
//    char timestr[31];
//    time_str(timestr, 31, &e->time);
//    L(LOG_DEBUG, "receive T=%s", timestr);
}

void receiver_init() {
    TAILQ_INIT(&rec_queue);

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

    // set Pin 17/0 generate an interrupt on high-to-low transitions
    // and attach myInterrupt() to the interrupt
    if (wiringPiISR(cfg.pulse_pin, INT_EDGE_FALLING, &pulse_interrupt) < 0) {
        L(LOG_WARNING, "Unable to setup ISR: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

void receiver_at_exit() {
    if (cfg.buzzer_pin != -1) {
        digitalWrite(cfg.buzzer_pin, 0);
    }
}
