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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <signal.h>

#define DT_MIN 1.0e-1
#define DT_MAX 3600

#define CHECK(x) \
    do { \
        if (!(x)) { \
            fprintf(stderr, "%s:%d: ", __func__, __LINE__); \
            perror(#x); \
            exit(-1); \
        } \
    } while (0) \

#define TIMEOUT 5
#define EMONCMS_URL "http://emoncms.org"
/* http://emoncms.org/input/bulk.json?data=[[-10,16,1137],[-8,17,1437,3164],[-6,19,1412,3077]]&time=1429335402 */
#define EMONCMS_PATH "/input/bulk.json"

extern struct cfg_t cfg;
extern volatile int stop;

extern FILE* open_file(const char *filepath, const char *mode);
extern void time_copy(struct timespec *tdest, struct timespec tsource);
extern double time_diff(struct timespec tend, struct timespec tstart);
extern int time_str(char *buf, uint len, struct timespec * ts);
extern double time_to_double(struct timespec t);

#ifdef	__cplusplus
}
#endif

#endif	/* MAIN_H */

