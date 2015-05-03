
#include <curl/curl.h>
#include "main.h"

struct send_queue send_q;
int send_queue_length = 0;
CURL *curl = NULL;

volatile int stop = 0;
char send_buf[1024];

// TODO switch to SIMPLEQ

struct send_entry * insert_entry(struct timespec tlast, struct timespec trec, double dt, double power, double elapsedkWh, long pulseCount) {
    struct send_entry *entry = malloc(sizeof (struct send_entry));
    populate_entry(entry, tlast, trec, dt, power, elapsedkWh, pulseCount);
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

void process_data() {
    struct send_entry *entry = malloc(sizeof (struct send_entry));
    struct timespec tout_mq;
    ssize_t bytes_read;
    CHECK(clock_gettime(CLOCK_REALTIME, &tout_mq) != -1);
    tout_mq.tv_sec += TIMEOUT / 2;
    bytes_read = mq_timedreceive(mq, (char*) entry, sizeof (struct send_entry), NULL, &tout_mq);
    if (bytes_read >= 0) {
        TAILQ_INSERT_TAIL(&send_q, entry, entries);
        send_queue_length++;
    }
}

int build_url() {
    // in case of no points were added, avoid to process data and wait for more 
    if (TAILQ_EMPTY(&send_q))
        return 0;
    int send_cnt = send_queue_length > 20 ? 20 : send_queue_length;
    int cnt = 0;
    char *pstr = stpcpy(send_buf, cfg.emocms_url);
    pstr = stpcpy(pstr, EMOCMS_PATH);
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
        L(LOG_DEBUG, "prepare %s: DT=%f, P=%ld, Power=%f, Elapsed kWh=%f", strd, p->dt, p->pulseCount, p->power, p->elapsedkWh);
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

int send_data() {
    if (!curl)
        return 1;

    curl_easy_setopt(curl, CURLOPT_URL, send_buf);
    //    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        L(LOG_WARNING, "send failed: %s : \"%s\"", curl_easy_strerror(res), send_buf);
    } else {
        L(LOG_DEBUG, "send OK: \"%s\"", send_buf);
    }
    curl_easy_reset(curl);
    return res != CURLE_OK;
}

void sender_run() {
    L(LOG_DEBUG, "running as sender");

    TAILQ_INIT(&send_q);
    curl = curl_easy_init();

    do {
        struct timespec tout, tnow;
        CHECK(clock_gettime(CLOCK_REALTIME, &tout) != -1);
        do {
            process_data();
            CHECK(clock_gettime(CLOCK_REALTIME, &tnow) != -1);
        } while (tnow.tv_sec < tout.tv_sec + TIMEOUT && !stop);
        while (send_queue_length > 0) {
            int cnt = build_url();
            if (cnt > 0) {
                if (!send_data()) {
                    remove_entries(cnt);
                } else {
                    sleep(TIMEOUT * 2);
                }
            }
        }
    } while (!stop);
}

void sender_at_exit() {
    if (curl != NULL) {
        curl_easy_cleanup(curl);
    }
}

void sender_sig_handler(int signo) {
    stop = 1;
}
