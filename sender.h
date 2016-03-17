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
 * File:   sender.h
 * Author: sergio
 *
 * Created on March 16, 2016, 8:41 PM
 */

#ifndef SENDER_H
#define SENDER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <time.h>
#include <sys/queue.h>

struct send_entry {
    TAILQ_ENTRY(send_entry) entries;
    struct timespec tlast;
    struct timespec trec;
    double dt;
    double power;
    double elapsedkWh;
    long pulseCount;
    long rawCount;
};

TAILQ_HEAD(send_queue, send_entry);

extern struct send_entry* populate_entry(struct send_entry *entry, struct timespec tlast, struct timespec trec, double dt, double power, double elapsedkWh, long pulseCount, long rawCount);

extern void sender_at_exit();
extern void sender_init();
extern void sender_loop();

#ifdef __cplusplus
}
#endif

#endif /* SENDER_H */

