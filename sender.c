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
 * File:   sender.c
 * Author: Sergio
 *
 * Created on May 03, 2015, 10:21 AM
 */

#include "sender.h"
#include "main.h"
#include "read_config.h"
#include "receiver.h"
#include "buzzer.h"
#include <curl/curl.h>

#define SEND_BUF_LENGTH 1024
#define MAX_SEND_CNT 10
#define MAX_RECEIVE_CNT 100

FILE *data_log_file = NULL;
CURL *curl = NULL;
char send_buf[SEND_BUF_LENGTH];

// TODO switch to SIMPLEQ

struct send_entry* populate_entry(struct send_entry *entry, struct timespec tlast, struct timespec trec, double dt, double power, double elapsed_kWh, long pulse_count, long raw_count) {
    time_copy(&entry->tlast, tlast);
    time_copy(&entry->trec, trec);
    entry->dt = dt;
    entry->power = power;
    entry->elapsed_kWh = elapsed_kWh;
    entry->pulse_count = pulse_count;
    entry->raw_count = raw_count;
    entry->entries.tqe_next = NULL;
    entry->entries.tqe_prev = NULL;
    return entry;
}

//FIXME used only in test

struct send_entry * insert_entry(struct server_mapping_entry *map, struct timespec tlast, struct timespec trec, double dt, double power, double elapsed_kWh, long pulse_count, long raw_count) {
    struct send_entry *entry = malloc(sizeof (struct send_entry));
    populate_entry(entry, tlast, trec, dt, power, elapsed_kWh, pulse_count, raw_count);
    TAILQ_INSERT_TAIL(&map->queue, entry, entries);
    map->queue_length++;
    return entry;
}

void remove_entries_from_map(struct server_mapping_entry *map) {
    while (!TAILQ_EMPTY(&map->queue) && map->queued > 0) {
        struct send_entry *p = TAILQ_FIRST(&map->queue);
        TAILQ_REMOVE(&map->queue, p, entries);
        free(p);
        map->queued--;
        map->queue_length--;
    }
    //    L(LOG_DEBUG, "remove map q0=%d q1=%d, l=%ld", c, map->queued, map->queue_length);
    CHECK(map->queued == 0);
    CHECK(map->queue_length >= 0);
}

void remove_entries_from_maps(struct server_entry *server) {
    int j;
    struct server_mapping_entry *map = server->maps;
    for (j = 0; j < server->maps_length; j++, map++) {
        remove_entries_from_map(map);
    }
}

double calc_power(struct source_entry *source, double dt) {
    return (3600000.0 / dt) / source->ppkwh;
}

static void data_store_load() {
    if (cfg.data_store != NULL) {
        FILE *file = open_file(cfg.data_store, "r");
        int i;
        struct source_entry *source = cfg.sources;
        for (i = 0; i < cfg.sources_length; i++, source++) {
            if (fscanf(file, "%ld;%ld;%ld\n", &source->pulse_count, &source->tlast.tv_sec, &source->tlast.tv_nsec) != 3) {
                source->pulse_count = 0;
                source->tlast.tv_sec = 0;
                source->tlast.tv_nsec = 0;
            }
            struct tm t;
            char strd[20];
            strftime(strd, sizeof (strd), "%F %T", localtime_r(&source->tlast.tv_sec, &t));
            L(LOG_DEBUG, "source %d: read last_time=%s, pulse count=%ld from %s", i, strd, source->pulse_count, cfg.data_store);
        }
        fclose(file);
    }
}

static void data_store_save() {
    if (cfg.data_store != NULL) {
        FILE *file = open_file(cfg.data_store, "w");
        int i;
        struct source_entry *source = cfg.sources;
        for (i = 0; i < cfg.sources_length; i++, source++) {
            fprintf(file, "%ld;%ld;%ld\n", source->pulse_count, source->tlast.tv_sec, source->tlast.tv_nsec);
            struct tm t;
            char strd[20];
            strftime(strd, sizeof (strd), "%F %T", localtime_r(&source->tlast.tv_sec, &t));
            L(LOG_DEBUG, "source %i: write last_time=%s, pulse count=%ld to %s", i, strd, source->pulse_count, cfg.data_store);
        }
        fclose(file);
    }
}

static void data_log_open() {
    if (cfg.data_log != NULL) {
        data_log_file = open_file(cfg.data_log, "a+");
        L(LOG_DEBUG, "log data to %s", cfg.data_log);
    }
}

static void data_log_write(struct timespec tnow, double power) {
    if (data_log_file != NULL) {
        fprintf(data_log_file, "%ld;%ld;%f\n", tnow.tv_sec, tnow.tv_nsec, power);
    }
}

static void data_log_close() {
    if (data_log_file != NULL) {
        fclose(data_log_file);
        L(LOG_DEBUG, "close log file %s", cfg.data_log);
    }
}

static void process_receive_entry(struct source_entry *source, struct timespec tnow) {
    double power = 0, elapsedkWh = 0;
    char timestr[31];
    ++source->raw_count;
    double dt = time_diff(tnow, source->tlast);
    if (source->raw_count == 1 || dt > DT_MIN) {
        ++source->pulse_count;
        //Calculate power
        if (source->pulse_count > 1 && dt < DT_MAX) {
            power = calc_power(source, dt);
            // calculate kwh elapsed
            elapsedkWh = 1.0 * source->pulse_count / source->ppkwh; //multiply by 1000 to pulses per wh to kwh convert wh to kwh
            int i;
            for (i = 0; i < source->maps_length; i++) {
                struct server_mapping_entry *map = source->maps[i];
                struct send_entry *entry = malloc(sizeof (struct send_entry));
                populate_entry(entry, source->tlast, tnow, dt, power, elapsedkWh, source->pulse_count, source->raw_count);
                TAILQ_INSERT_TAIL(&map->queue, entry, entries);
                map->queue_length++;
            }
            if (cfg.buzzer_source == source) {
                buzzer_control(power, elapsedkWh, tnow);
            }
            data_log_write(tnow, power);
        }
        time_str(timestr, 31, &tnow);
        L(LOG_DEBUG, "queued %s: DT=%f, P=%ld(%ld), Power=%f, Elapsed kWh=%f", timestr, dt, source->pulse_count, source->raw_count, power, elapsedkWh);
    }
    time_copy(&source->tlast, tnow);
}

int process_receive_queue_source(struct source_entry *source) {
    int cnt = 0;
    if (receive_queue_sem == 0) {
        struct receive_entry *entry;
        while (cnt < MAX_RECEIVE_CNT && (entry = TAILQ_FIRST(&source->queue)) != NULL) {
            process_receive_entry(source, entry->time);
            TAILQ_REMOVE(&source->queue, entry, entries);
            free(entry);
            cnt++;
        }
    }
    return cnt;
}

int process_receive_queue() {
    int i;
    int cnt = 0;
    struct source_entry *source = cfg.sources;
    for (i = 0; i < cfg.sources_length; i++, source++) {
        cnt += process_receive_queue_source(source);
    }
    return cnt;
}

int server_queue_empty(struct server_entry *server) {
    int i = 0;
    struct server_mapping_entry *map = server->maps;
    for (i = 0; i < server->maps_length; i++, map++) {
        if (map->queue_length > 0)
            return 0;
    }
    return 1;
}

long find_minimum_time_in_map_queues(struct server_entry *server) {
    int i;
    struct server_mapping_entry *map = server->maps;
    long t0 = LONG_MAX;
    for (i = 0; i < server->maps_length; i++, map++) {
        if (!TAILQ_EMPTY(&map->queue)) {
            struct send_entry *p = TAILQ_FIRST(&map->queue);
            if (p != NULL && p->tlast.tv_sec < t0) {
                t0 = p->tlast.tv_sec;
            }
        }
    }
    return t0;
}

int build_url_emoncms(struct server_entry *server) {
    // in case of no points were added, avoid to process data and wait for more 
    if (server_queue_empty(server)) {
        return 0;
    }
    int cnt = 0;
    long t0 = find_minimum_time_in_map_queues(server) + 1;
    char *pstr = stpcpy(send_buf, server->url);
    pstr = stpcpy(pstr, EMONCMS_PATH);
    //FIXME force max length for api-key string in configuration
    sprintf(pstr, "?time=%ld&apikey=%s", t0, server->maps->api_key);
    pstr += strlen(pstr);
    pstr = stpcpy(pstr, "&data=[");
    int i;
    struct server_mapping_entry *map = server->maps;
    for (i = 0; i < server->maps_length; i++, map++) {
        int send_cnt = map->queue_length > MAX_SEND_CNT ? MAX_SEND_CNT : map->queue_length;
        if (send_cnt == 0) {
            continue;
        }
        map->queued = 0;
        struct send_entry *p = TAILQ_FIRST(&map->queue);
        //        long t0 = p->tlast.tv_sec + 1;

        TAILQ_FOREACH(p, &map->queue, entries) {
            long t1 = p->tlast.tv_sec + 1;
            long t2 = p->trec.tv_sec;
            struct tm lt;
            char strd[26];
            strftime(strd, sizeof (strd), "%F %T", localtime_r(&t2, &lt));
            //        L(LOG_DEBUG, "prepare %s: DT=%f, P=%ld, Power=%f, Elapsed kWh=%f", strd, p->dt, p->pulseCount, p->power, p->elapsedkWh);
            if (t2 <= t1) {
                sprintf(pstr, "[%ld,%d,%f,%f,%ld],", t1 - t0, map->node_id, p->power, p->elapsed_kWh, p->pulse_count);
            } else {
                sprintf(pstr, "[%ld,%d,%f,%f,%ld],[%ld,%d,%f,%f,%ld],", t1 - t0, map->node_id, p->power, p->elapsed_kWh, p->pulse_count, t2 - t0, map->node_id, p->power, p->elapsed_kWh, p->pulse_count);
            }
            pstr += strlen(pstr);
            ++cnt;
            map->queued++;
            if (strlen(send_buf) > SEND_BUF_LENGTH - 100)
                break;
            if (map->queued >= send_cnt)
                break;
        }
    }
    pstr[strlen(pstr) - 1] = ']';
    pstr += strlen(pstr);
    return cnt;
}

int build_url_emonlight(struct server_entry *server) {
    // in case of no points were added, avoid to process data and wait for more 
    if (server_queue_empty(server)) {
        return 0;
    }
    int cnt = 0;
    char *pstr = stpcpy(send_buf, "{\"nodes\": [");
    int i;
    struct server_mapping_entry *map = server->maps;
    for (i = 0; i < server->maps_length; i++, map++) {
        int send_cnt = map->queue_length > MAX_SEND_CNT ? MAX_SEND_CNT : map->queue_length;
        if (send_cnt == 0) {
            continue;
        }
        map->queued = 0;
        sprintf(pstr, "{\"k\":\"%s\"", map->api_key);
        pstr += strlen(pstr);
        if (map->node_id != 0) {
            sprintf(pstr, ",\"id\":%d", map->node_id);
            pstr += strlen(pstr);
        }
        pstr = stpcpy(pstr, ",\"d\":[");
        struct send_entry *p;

        TAILQ_FOREACH(p, &map->queue, entries) {
            long t = p->trec.tv_sec;
            struct tm lt;
            char strd[26];
            strftime(strd, sizeof (strd), "%F %T", localtime_r(&t, &lt));
            //        L(LOG_DEBUG, "prepare %s: DT=%f, P=%ld, Power=%f, Elapsed kWh=%f", strd, p->dt, p->pulseCount, p->power, p->elapsedkWh);
            sprintf(pstr, "[%ld,%ld,%f],", p->trec.tv_sec, p->trec.tv_nsec, p->power);
            pstr += strlen(pstr);
            map->queued++;
            cnt++;
            //            L(LOG_DEBUG, "prepare %s: q=%d, c=%d, s=%d, t=%d", strd, map->queued, send_cnt, strlen(send_buf), sizeof(send_buf));
            // FIXME
            if (strlen(send_buf) > SEND_BUF_LENGTH - 100)
                break;
            if (map->queued >= send_cnt)
                break;
        }
        if (map->queued > 0) {
            pstr--;
            pstr[0] = 0;
        }
        pstr = stpcpy(pstr, "]},");
    }
    if (cnt > 0) {
        pstr--;
        pstr[0] = 0;
    }
    pstr = stpcpy(pstr, "]}");
    return cnt;
}

int send_data(struct server_entry *server) {
    if (!curl)
        return 1;

    char *out = NULL;
    if (server->protocol == EMONCMS_REMOTE_ID) {
        curl_easy_setopt(curl, CURLOPT_URL, send_buf);
    } else {
        curl_easy_setopt(curl, CURLOPT_URL, server->url);
        /* set content type */
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Accept: application/json");
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(send_buf));
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, send_buf);
    }
    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    if (res != CURLE_OK) {
        L(LOG_WARNING, "send failed: %s : %s \"%s\"", curl_easy_strerror(res), server->url, send_buf);
    } else {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        if (http_code == 200 && res != CURLE_ABORTED_BY_CALLBACK) {
            L(LOG_DEBUG, "send OK: \"%s %s\"", server->url, send_buf);
        } else {
            L(LOG_WARNING, "send failed: HTTP response %ld : \"%s%s\"", http_code, server->url, send_buf);
        }
    }
    if (out != NULL)
        curl_free(out);
    curl_easy_reset(curl);
    return res == CURLE_OK && http_code == 200;
}

int process_send_queues() {
    int send_cnt = 0;
    int i;
    struct server_entry *server = cfg.servers;
    // FIXME delay server calls based on previous errors
    for (i = 0; i < cfg.servers_length; i++, server++) {
        int srv_cnt = server->protocol == EMONCMS_REMOTE_ID ? build_url_emoncms(server) : build_url_emonlight(server);
        if (srv_cnt > 0) {
            if (send_data(server)) {
                remove_entries_from_maps(server);
                send_cnt += srv_cnt;
            }
        }
    }
    return send_cnt;
}

void sender_init() {
    data_log_open();
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    data_store_load();
}

void sender_at_exit() {
    data_store_save();
    data_log_close();
    if (curl != NULL) {
        curl_easy_cleanup(curl);
    }
    buzzer_at_exit();
}

void sender_loop() {
    struct timespec t = {TIMEOUT, 0};
    do {
        process_receive_queue();
        int send_cnt = process_send_queues();
        if (!stop && send_cnt < MAX_SEND_CNT) {
            nanosleep(&t, NULL);
        }
    } while (!stop);
}