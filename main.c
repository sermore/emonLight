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
 * File:   main.c
 * Author: Sergio
 *
 * Created on April 12, 2015, 10:21 AM
 */
#include "main.h"
#include "read_config.h"
#include "sender.h"
#include "receiver.h"
#include "buzzer.h"
#include <getopt.h>
#include <sys/types.h>
#include <fcntl.h>

#define LOG_NAME "emonlight"
#define YN(v) (v == 1 ? "YES" : "NO")

int pidFilehandle = -1;
volatile int stop;

static void optarg_to_int(int *conf, char *option) {
    char *endptr;
    errno = 0;
    int value = strtol(optarg, &endptr, 10);
    if (errno != 0) {
        L(LOG_WARNING, "expected number in parameter %s", option);
        exit(EXIT_FAILURE);
    }
    if (endptr == optarg) {
        L(LOG_WARNING, "no digit were found in parameter %s", option);
        exit(EXIT_FAILURE);
    }
    if (*endptr != '\0') {
        L(LOG_WARNING, "expected number in parameter %s", option);
        exit(EXIT_FAILURE);
    }
    *conf = value;
}

void parse_opts(int argc, char **argv) {
    int c;
    while (1) {
        //        int this_option_optind = optind ? optind : 1;
        int option_index = 0;
        static struct option long_options[] = {
            {"config", required_argument, 0, 'c'},
            {"daemon", no_argument, 0, 'd'},
            {"verbose", no_argument, 0, 'v'},
            {"data-log", optional_argument, 0, 'l'},
            {"pid-path", required_argument, 0, 't'},
            {"data-store", required_argument, 0, 'a'},
            {"buzzer-test", no_argument, 0, 'z'},
            {"help", no_argument, 0, 'h'},
            {0, 0, 0, 0}
        };

        c = getopt_long(argc, argv, "c:dvl::t:a:zh", long_options, &option_index);
        if (c == -1)
            break;
        switch (c) {
            case 'v':
                cfg.verbose = 1;
                break;
            case 'd':
                cfg.daemonize = 1;
                break;
            case 'c':
                cfg.config = optarg;
                break;
            case 'l':
                cfg.data_log_defaults = optarg == NULL;
                cfg.data_log = optarg;
                break;
            case 't':
                cfg.pid_path = optarg;
                break;
            case 'a':
                cfg.data_store = optarg;
                break;
            case 'z':
                cfg.buzzer_test = 1;
                break;
            case 'h':
                cfg.help = 1;
                break;
            case '?':
                L(LOG_WARNING, "unable to parse input parameters");
                exit(2);
                break;
            default:
                L(LOG_WARNING, "?? getopt returned character code 0%o ??\n", c);
                exit(3);
        }
    }
    if (optind < argc) {
        printf("non-option ARGV-elements: ");
        while (optind < argc)
            printf("%s ", argv[optind++]);
        printf("\n");
    }
}

void help() {
    printf("Usage \n"
            "Options:\n"
            "-c, --config=FILE                      read configuration from specified file\n"
            "-d, --daemon                           execute as daemon\n"
            "-v, --verbose                          enable verbose print and log\n"
            "-l, --data-log=FILE                    append to FILE data retrieved\n"
            "-t, --pid-path=PATH                    store pid file in directory PATH\n"
            "-a, --data-store=FILE                  save receiver status on termination\n"
            "-b, --buzzer-pin=NUMBER                use pin NUMBER as output to drive buzzer\n"
            "-z, --buzzer-test                      test buzzer and exit\n"
            "-h, --help                             this\n"
            );
    printf("\nVersion:%s\n"
            "Configuration:\n"
            "configuration file %s\n"
            "perform as daemon %s\n"
            "verbose mode %s\n"
            "data log file %s\n"
            "path for pid file %s\n"
            "data store file %s\n"
            "buzzer control pin %d\n"
            "buzzer source pin %d\n"
            "power soft threshold %d\n"
            "power soft threshold time (seconds) %d\n"
            "power hard threshold %d\n"
            "power hard threshold time (seconds) %d\n",
            VERSION,
            cfg.config, YN(cfg.daemonize), YN(cfg.verbose),
            cfg.data_log, cfg.pid_path,
            cfg.data_store, cfg.buzzer_pin, 
            cfg.buzzer_source != NULL ? cfg.buzzer_source->pin : 0,
            cfg.power_soft_threshold, cfg.power_soft_threshold_time,
            cfg.power_hard_threshold, cfg.power_hard_threshold_time);
    print_sources();
    print_servers();
    exit(EXIT_FAILURE);
}

double time_diff(struct timespec tend, struct timespec tstart) {
    double dt = 0.0 + (tend.tv_sec - tstart.tv_sec) + (tend.tv_nsec - tstart.tv_nsec) / 1.0e9;
    return dt;
}

void time_copy(struct timespec *tdest, struct timespec tsource) {
    tdest->tv_sec = tsource.tv_sec;
    tdest->tv_nsec = tsource.tv_nsec;
}

// buf needs to store 30 characters

int time_str(char *buf, uint len, struct timespec * ts) {
    int ret;
    struct tm t;

    tzset();
    if (localtime_r(&(ts->tv_sec), &t) == NULL)
        return 1;

    ret = strftime(buf, len, "%F %T", &t);
    if (ret == 0)
        return 2;
    len -= ret - 1;

    ret = snprintf(&buf[strlen(buf)], len, ".%09ld", ts->tv_nsec);
    if (ret >= len)
        return 3;

    return 0;
}

double time_to_double(struct timespec t) {
    return 0.0 + t.tv_sec + t.tv_nsec / 1.0e9;
}

static void sig_handler(int signo) {
    L(LOG_DEBUG, "received termination request");
    stop = 1;
}

FILE *open_file(const char *filepath, const char *mode) {
    FILE *file;
    if ((file = fopen(filepath, mode)) == NULL) {
        L(LOG_DEBUG, "error opening file %s: %s", filepath, strerror(errno));
        exit(-1);
        //            // separate file name from path
        //            char *ptr = strrchr(filepath, '/');
        //            CHECK(ptr != NULL);
        //            char *filename = ptr++;
        //            CHECK(strlen(filename) > 0);
        //            char path[ptr - filepath];
        //            strncpy(path, filepath, ptr - filepath);
        //            CHECK(mkpath(path, 00755) == 0);            
    }
    return file;
}

static int pidfile(char *str) {
    return sprintf(str, "%s/%s.pid", cfg.pid_path, LOG_NAME);
}

static void daemonize() {
    char str[200];
    pidfile(str);

    /* Ensure only one copy */
    pidFilehandle = open(str, O_RDWR | O_CREAT, 0644);

    if (pidFilehandle == -1) {
        /* Couldn't open lock file */
        L(LOG_WARNING, "Could not open PID lock file %s, exiting", str);
        exit(EXIT_FAILURE);
    }

    /* Try to lock file */
    if (lockf(pidFilehandle, F_TLOCK, 0) == -1) {
        /* Couldn't get lock on lock file */
        L(LOG_WARNING, "Could not lock PID lock file %s, exiting", str);
        exit(EXIT_FAILURE);
    }

    if (daemon(0, 0) == -1) {
        L(LOG_WARNING, "Cannot daemonize");
        exit(EXIT_FAILURE);
    }

    /* Get and format PID */
    sprintf(str, "%d\n", getpid());
    /* write pid to lockfile */
    write(pidFilehandle, str, strlen(str));

    L(LOG_DEBUG, "daemonized");
}

static void at_exit(void) {

    sender_at_exit();
    receiver_at_exit();

    // close and remove pidfile in case of daemon
    if (pidFilehandle != -1) {
        close(pidFilehandle);
        char str[200];
        pidfile(str);
        remove(str);
    }

    L(LOG_DEBUG, "terminated");
    closelog();
}

// -------------------------------------------------------------------------
// main

int main(int argc, char **argv) {

    atexit(at_exit);
    CHECK(signal(SIGINT, sig_handler) != SIG_ERR);
    CHECK(signal(SIGTERM, sig_handler) != SIG_ERR);
    CHECK(signal(SIGCHLD, SIG_IGN) != SIG_ERR);
    CHECK(signal(SIGHUP, SIG_IGN) != SIG_ERR);

    parse_opts(argc, argv);
    if (read_config(cfg.config) != 0) {
        exit(EXIT_FAILURE);
    }

    if (cfg.help) {
        help();
        exit(EXIT_SUCCESS);
    }

    if (!cfg.verbose) {
        setlogmask(LOG_UPTO(LOG_WARNING));
    }
    openlog(LOG_NAME, LOG_PID | LOG_CONS, LOG_USER);

    if (cfg.daemonize)
        daemonize();

    if (cfg.config != NULL)
        L(LOG_INFO, "read config from %s", cfg.config);

    receiver_init();
    sender_init();
    buzzer_init();

    buzzer_test();
    if (cfg.buzzer_test) {
        exit(EXIT_FAILURE);
    }

    // main loop
    stop = 0;
    do {
        sender_loop();
    } while (!stop);

    return 0;
}
