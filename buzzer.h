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
 * File:   buzzer.h
 * Author: sergio
 *
 * Created on March 15, 2016, 9:19 PM
 */

#ifndef BUZZER_H
#define BUZZER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <time.h>
#include <sys/queue.h>
    
struct buzzer_config {
    double power_threshold_kwh; // power threshold 
    long time_threshold_sec; // interval length for which the power excess can be maintained
    int pulses_init; // initial number of pulses to apply
};

struct buzzer_power_entry {
    TAILQ_ENTRY(buzzer_power_entry) entries;
    double power_acc_kwh;
    double time_sec;
};

TAILQ_HEAD(buzzer_power_queue, buzzer_power_entry);

extern struct cfg_t cfgxx;

extern void buzzer_init();
extern void buzzer_at_exit();
extern void buzzer_control(double power, double elapsedkWh, struct timespec tnow);
extern void buzzer_test();

#ifdef __cplusplus
}
#endif

#endif /* BUZZER_H */

