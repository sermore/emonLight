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
#include "receiver.h"
#include "buzzer.h"
#include <curl/curl.h>

FILE *data_log_file = NULL;
volatile long rawCount = 0;
struct timespec tlast = {0L, 0L};
struct timespec tnow = {0L, 0L};
volatile long pulseCount = 0;

struct send_queue send_q;
int send_queue_length = 0;
CURL *curl = NULL;
char send_buf[1024];

// TODO switch to SIMPLEQ

struct send_entry* populate_entry(struct send_entry *entry, struct timespec tlast, struct timespec trec, double dt, double power, double elapsedkWh, long pulseCount, long rawCount) {
    time_copy(&entry->tlast, tlast);
    time_copy(&entry->trec, trec);
    entry->dt = dt;
    entry->power = power;
    entry->elapsedkWh = elapsedkWh;
    entry->pulseCount = pulseCount;
    entry->rawCount = rawCount;
    entry->entries.tqe_next = NULL;
    entry->entries.tqe_prev = NULL;
    return entry;
}

//FIXME used only in test
struct send_entry * insert_entry(struct timespec tlast, struct timespec trec, double dt, double power, double elapsedkWh, long pulseCount, long rawCount) {
    struct send_entry *entry = malloc(sizeof (struct send_entry));
    populate_entry(entry, tlast, trec, dt, power, elapsedkWh, pulseCount, rawCount);
    TAILQ_INSERT_TAIL(&send_q, entry, entries);
    send_queue_length++;
    return entry;
}

void remove_entries(int cnt) {
    while (!TAILQ_EMPTY(&send_q) && cnt) {
        struct send_entry *p = TAILQ_FIRST(&send_q);
        TAILQ_REMOVE(&send_q, p, entries);
        free(p);
        cnt--;
        send_queue_length--;
    }
    CHECK(cnt == 0);
    CHECK(send_queue_length >= 0);
}

double calc_power(double dt) {
    return (3600000.0 / dt) / cfg.ppkwh;
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

static void data_log_open() {
    if (cfg.data_log != NULL) {
        data_log_file = open_file(cfg.data_log, "a+");
        L(LOG_DEBUG, "log data to %s", cfg.data_log);
    }
}

static void data_log_write(struct send_entry *p) {
    if (data_log_file != NULL) {
        fprintf(data_log_file, "%ld;%ld;%f\n", p->trec.tv_sec, p->trec.tv_nsec, p->power);
    }
}

static void data_log_close() {
    if (data_log_file != NULL) {
        fclose(data_log_file);
        L(LOG_DEBUG, "close log file %s", cfg.data_log);
    }
}

static void process_receive_entry(struct timespec tnow) {
    double power = 0, elapsedkWh = 0;
    char timestr[31];

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
            struct send_entry *entry = malloc(sizeof(struct send_entry));
            populate_entry(entry, tlast, tnow, dt, power, elapsedkWh, pulseCount, rawCount);
            TAILQ_INSERT_TAIL(&send_q, entry, entries);
            receive_queue_sem = 0;
            buzzer_control(power, elapsedkWh, tnow);
            data_log_write(entry);
            send_queue_length++;
        }
        time_str(timestr, 31, &tnow);
        L(LOG_DEBUG, "queued %s: DT=%f, P=%ld(%ld), Power=%f, Elapsed kWh=%f", timestr, dt, pulseCount, rawCount, power, elapsedkWh);
    }
    time_copy(&tlast, tnow);
}

void process_receive_queue() {
    if (receive_queue_sem == 0) {
        struct receive_entry *entry = TAILQ_FIRST(&rec_queue);
        if (entry != NULL) {
            process_receive_entry(entry->time);
            TAILQ_REMOVE(&rec_queue, entry, entries);
            free(entry);
        } else {
            sleep(TIMEOUT / 2);
        }
    }
}

int build_url_emoncms() {
    // in case of no points were added, avoid to process data and wait for more 
    if (TAILQ_EMPTY(&send_q))
        return 0;
    int send_cnt = send_queue_length > 20 ? 20 : send_queue_length;
    int cnt = 0;
    char *pstr = stpcpy(send_buf, cfg.url);
    pstr = stpcpy(pstr, EMONCMS_PATH);
    pstr = stpcpy(pstr, "?apikey=");
    pstr = stpcpy(pstr, cfg.api_key);
    pstr = stpcpy(pstr, "&data=[");

    struct send_entry * p = TAILQ_FIRST(&send_q);
    long t0 = p->tlast.tv_sec + 1;

    TAILQ_FOREACH(p, &send_q, entries) {
        long t1 = p->tlast.tv_sec + 1;
        long t2 = p->trec.tv_sec;
        struct tm lt;
        char strd[26];
        strftime(strd, sizeof (strd), "%F %T", localtime_r(&t2, &lt));
//        L(LOG_DEBUG, "prepare %s: DT=%f, P=%ld, Power=%f, Elapsed kWh=%f", strd, p->dt, p->pulseCount, p->power, p->elapsedkWh);
        if (t2 <= t1) {
            sprintf(pstr, "[%ld,%d,%f,%f,%ld],", t1 - t0, cfg.node_id, p->power, p->elapsedkWh, p->pulseCount);
        } else {
            sprintf(pstr, "[%ld,%d,%f,%f,%ld],[%ld,%d,%f,%f,%ld],", t1 - t0, cfg.node_id, p->power, p->elapsedkWh, p->pulseCount, t2 - t0, cfg.node_id, p->power, p->elapsedkWh, p->pulseCount);
        }
        pstr += strlen(pstr);
        ++cnt;
        if (strlen(send_buf) > sizeof (send_buf) - 100)
            break;
        if (cnt >= send_cnt)
            break;
    }
    pstr[strlen(pstr) - 1] = ']';
    pstr += strlen(pstr);
    sprintf(pstr, "&time=%ld", t0);
    return cnt;
}

int build_url_emonlight() {
    // in case of no points were added, avoid to process data and wait for more 
    if (TAILQ_EMPTY(&send_q))
        return 0;
    int send_cnt = send_queue_length > 20 ? 20 : send_queue_length;
    int cnt = 0;
    char buffer[1024];
    memset(buffer,0, sizeof(buffer));
    char *token = curl_easy_escape(curl, cfg.api_key, strlen(cfg.api_key));
    sprintf(send_buf, "token=%s", token);
    curl_free(token);
    if (cfg.node_id > 0) {
        sprintf(buffer, "node_id=%d", cfg.node_id);
        sprintf(send_buf, "%s&%s", send_buf, buffer);
    }
    struct send_entry * p = TAILQ_FIRST(&send_q);

    TAILQ_FOREACH(p, &send_q, entries) {
        long t = p->trec.tv_sec;
        struct tm lt;
        char strd[26];
        strftime(strd, sizeof (strd), "%F %T", localtime_r(&t, &lt));
//        L(LOG_DEBUG, "prepare %s: DT=%f, P=%ld, Power=%f, Elapsed kWh=%f", strd, p->dt, p->pulseCount, p->power, p->elapsedkWh);
        sprintf(buffer, "%ld,%ld", p->trec.tv_sec, p->trec.tv_nsec);
        char * param  = curl_easy_escape(curl, buffer, strlen(buffer));
        sprintf(send_buf, "%s&epoch_time[]=%s", send_buf, param);        
        curl_free(param);
        sprintf(buffer, "%f", p->power);
        param  = curl_easy_escape(curl, buffer, strlen(buffer));
        sprintf(send_buf, "%s&power=%s", send_buf, param);        
        curl_free(param);
        ++cnt;
        if (strlen(send_buf) > sizeof (send_buf) - 100)
            break;
        if (cnt >= send_cnt)
            break;
    }
    return cnt;
}

int send_data() {
    if (!curl)
        return 1;
    
    char *out = NULL;
    if (cfg.remote == EMONCMS_REMOTE_ID) {
        curl_easy_setopt(curl, CURLOPT_URL, send_buf);
    } else {
        curl_easy_setopt(curl, CURLOPT_URL, cfg.url);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(send_buf));
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, send_buf);
    }
    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    if (res != CURLE_OK) {
        L(LOG_WARNING, "send failed: %s : \"%s\"", curl_easy_strerror(res), send_buf);
    } else {
        curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &http_code);
        if (http_code == 200 && res != CURLE_ABORTED_BY_CALLBACK) {
            L(LOG_DEBUG, "send OK: \"%s\"", send_buf);
        } else {
            L(LOG_WARNING, "send failed: HTTP response %ld : \"%s\"", http_code, send_buf);
        }
    }
    if (out != NULL)
        curl_free(out);
    curl_easy_reset(curl);
    return res != CURLE_OK || http_code != 200;
}

void sender_init() {
    TAILQ_INIT(&send_q);
    data_log_open();
    curl = curl_easy_init();    
    buzzer_setup();
    if (cfg.buzzer_test) {
        buzzer_test();
        exit(EXIT_FAILURE);
    }
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
    struct timespec tout, tnow;
    CHECK(clock_gettime(CLOCK_REALTIME, &tout) != -1);
    do {
        process_receive_queue();
        CHECK(clock_gettime(CLOCK_REALTIME, &tnow) != -1);
    } while (tnow.tv_sec < (tout.tv_sec + TIMEOUT) && !stop);
    while (send_queue_length > 0) {
        int cnt = cfg.remote == EMONCMS_REMOTE_ID ? build_url_emoncms() : build_url_emonlight();
        if (cnt > 0) {
            if (!send_data()) {
                remove_entries(cnt);
            } else {
                sleep(TIMEOUT * 2);
            }
        }
    }
}