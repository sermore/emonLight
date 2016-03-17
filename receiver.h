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
 * File:   receiver.h
 * Author: sergio
 *
 * Created on March 16, 2016, 8:08 PM
 */

#ifndef RECEIVER_H
#define RECEIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <time.h>
#include <signal.h>
#include <sys/queue.h>

struct receive_entry {
    TAILQ_ENTRY(receive_entry) entries;
    struct timespec time;
};

TAILQ_HEAD(receive_queue, receive_entry);

extern struct receive_queue rec_queue;
extern sig_atomic_t receive_queue_sem;

extern void receiver_at_exit();
extern void receiver_init();

#ifdef __cplusplus
}
#endif

#endif /* RECEIVER_H */

