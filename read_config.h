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
 * File:   read_config.h
 * Author: sergio
 *
 * Created on May 10, 2016, 7:58 PM
 */

#ifndef READ_CONFIG_H
#define READ_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <libconfig.h>
#include <syslog.h>
#include <sys/queue.h>

#define EMONCMS_REMOTE "emoncms"
#define EMONLIGHT_REMOTE "emonlight"
#define EMONCMS_REMOTE_ID 1
#define EMONLIGHT_REMOTE_ID 2

#define L(log_level, fmt, ...) \
    if (cfg.verbose) { \
        fprintf(log_level > LOG_INFO ? stderr : stdout, fmt, ##__VA_ARGS__); \
        fprintf(log_level > LOG_INFO ? stderr : stdout, "\n"); \
        syslog(log_level, fmt, ##__VA_ARGS__); \
    }

struct receive_entry {
    struct timespec time;
    TAILQ_ENTRY(receive_entry) entries;
};
TAILQ_HEAD(receive_queue, receive_entry);

struct send_entry {
    struct timespec tlast;
    struct timespec trec;
    double dt;
    double power;
    double elapsed_kWh;
    long pulse_count;
    long raw_count;
    TAILQ_ENTRY(send_entry) entries;
};
TAILQ_HEAD(send_queue, send_entry);

struct source_entry {
    int pin;
    int ppkwh;
    struct receive_queue queue;
    struct timespec tlast;
    long pulse_count;
    long raw_count;
    struct server_mapping_entry **maps;
    int maps_length;
};

struct server_mapping_entry {
    int node_id;
    const char *api_key;
    struct source_entry *source;
//    struct server_entry *server;
    struct send_queue queue;
    long queue_length;
    int queued;
};

struct server_entry {
    const char *url;
    short protocol;
    struct server_mapping_entry *maps;
    int maps_length;
};
    
struct cfg_t {
    const char *config;
    short daemonize;
    short verbose;
    char *data_log;
    short data_log_defaults;
    const char *pid_path;
    char *data_store;
    const char *homedir;
    short help;
    int buzzer_pin;
    struct source_entry *buzzer_source;
    int power_soft_threshold;
    int power_soft_threshold_time;
    int power_hard_threshold;
    int power_hard_threshold_time;
    short buzzer_test;
    struct source_entry *sources;
    int sources_length;
    struct server_entry *servers;
    int servers_length;
};

extern int read_config(const char *config_file);
extern void print_sources();
extern void print_servers();
//extern int read_mapping(const config_setting_t* settings, struct server_entry *server);
//extern int read_mapping_settings(const config_setting_t *settings);
//extern struct server_entry *read_server_settings(const config_setting_t *settings);
//extern int read_servers(const config_t* config);
//extern struct source_entry *read_source_settings(const config_setting_t *settings);
//extern int read_sources(const config_t* config);

#ifdef __cplusplus
}
#endif

#endif /* READ_CONFIG_H */
