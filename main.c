/* 
 * File:   main.c
 * Author: Sergio
 *
 * Created on April 12, 2015, 10:21 AM
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <pwd.h>
#include <mqueue.h>
#include <syslog.h>
#include <curl/curl.h>
#include <wiringPi.h>
#include <libconfig.h>

#include "main.h"

struct cfg_t cfg = {
    .config = NULL,
    .emocms_url = NULL,
    .daemonize = 0,
    .receiver = 1,
    .sender = 1,
    .unlink_queue = 0,
    .queue_size = 0,
    .verbose = 0,
    .pin = 0,
    .api_key = NULL,
    .node_id = 0
};

struct send_queue send_q;
int send_queue_length = 0;
CURL *curl;
FILE *fd_data = NULL;

volatile int stop = 0;
mqd_t mq;
long pulseCount = 0, rawCount = 0;
struct timespec tstart, tlast = {0, 0};
char send_buf[1024];

void parse_opts(int argc, char **argv) {
    int c;
    int value = 0;
    int preceiver = 0, psender = 0;

    while (1) {
        int this_option_optind = optind ? optind : 1;
        int option_index = 0;
        static struct option long_options[] = {
            {"config", required_argument, 0, 'c'},
            {"daemon", no_argument, 0, 'd'},
            {"receiver", no_argument, 0, 'r'},
            {"sender", no_argument, 0, 's'},
            {"unlink-queue", no_argument, 0, 'u'},
            {"queue-size", required_argument, 0, 'q'},
            {"verbose", no_argument, 0, 'v'},
            {"pin", required_argument, 0, 'p'},
            {"api-key", required_argument, 0, 'k'},
            {"node-id", required_argument, 0, 'n'},
            {"emocms-url", required_argument, 0, 'e'},
            {"data-log", required_argument, 0, 'l'},
            {0, 0, 0, 0}
        };

        c = getopt_long(argc, argv, "c:drsuq:vp:k:ne:l:", long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
            case 'u':
                cfg.unlink_queue = 1;
                break;

            case 'v':
                cfg.verbose = 1;
                break;

            case 'd':
                cfg.daemonize = 1;
                break;

            case 'r':
                preceiver = 1;
                break;

            case 's':
                psender = 1;
                break;

            case 'p':
                value = strtol(optarg, NULL, 10);
                if (value > 0) {
                    cfg.pin = value;
                }
                break;

            case 'q':
                value = strtol(optarg, NULL, 10);
                if (value > 0) {
                    cfg.queue_size = value;
                }
                break;

            case 'k':
                cfg.api_key = optarg;
                break;

            case 'c':
                cfg.config = optarg;
                break;

            case 'e':
                cfg.emocms_url = optarg;
                break;

            case 'n':
                value = strtol(optarg, NULL, 10);
                if (value > 0) {
                    cfg.node_id = value;
                }
                break;
            case 'l':
                cfg.datalog = optarg;
                break;

            case '?':
                break;

            default:
                printf("?? getopt returned character code 0%o ??\n", c);
        }
    }
    if (preceiver || psender) {
        cfg.receiver = preceiver;
        cfg.sender = psender;
    }
    if (optind < argc) {
        printf("non-option ARGV-elements: ");
        while (optind < argc)
            printf("%s ", argv[optind++]);
        printf("\n");
    }
}

const char* get_config_file(const char *config_file) {
    const char *dest_file;
    if (config_file == NULL) {
        if (cfg.daemonize) {
            dest_file = "/etc/emonlight.conf";
        } else {
            const char *homedir;
            if ((homedir = getenv("HOME")) == NULL) {
                homedir = getpwuid(getuid())->pw_dir;
            }
            strcpy(send_buf, homedir);
            strcat(send_buf, "/.emonlight");
            dest_file = strdup(send_buf);
        }
    } else {
        dest_file = config_file;
    }
    return dest_file;
}

int read_config(const char *config_file) {
    config_t config; /*Returns all parameters in this structure */
    //    config_setting_t *setting;
    const char *str1;
    int tmp;

    const char *file = get_config_file(config_file);
    if (access(file, F_OK) == 0) {
        /*Initialization */
        config_init(&config);
        /* Read the file. If there is an error, report it and exit. */
        if (!config_read_file(&config, file)) {
            L(LOG_WARNING, "\n%s:%d - %s", config_error_file(&config), config_error_line(&config), config_error_text(&config));
            config_destroy(&config);
            return 1;
        }
        if (cfg.sender) {
            if (cfg.api_key == NULL) {
                if (config_lookup_string(&config, "api-key", &str1))
                    cfg.api_key = strdup(str1);
            }
            if (cfg.emocms_url == NULL) {
                if (config_lookup_string(&config, "emocms-url", &str1))
                    cfg.emocms_url = strdup(str1);
                else {
                    cfg.emocms_url = EMOCMS_URL;
                }
            }
            if (cfg.node_id == 0) {
                if (config_lookup_int(&config, "node-id", &tmp))
                    cfg.node_id = tmp;
                else {
                    cfg.node_id = 1;
                }
            }
            if (cfg.datalog == NULL) {
                if (config_lookup_string(&config, "data-log", &str1))
                    cfg.datalog = strdup(str1);
            }
        }
        if (cfg.receiver) {
            if (cfg.pin == 0) {
                if (config_lookup_int(&config, "gpio-pin", &tmp))
                    cfg.pin = tmp;
                else {
                    cfg.pin = 17;
                }
            }
        }
        if (cfg.queue_size == 0) {
            if (config_lookup_int(&config, "queue-size", &tmp))
                cfg.queue_size = tmp;
            else {
                cfg.queue_size = 1024;
                //TODO verification of SO limits
            }
        }
        config_destroy(&config);
    }
    if (cfg.sender && cfg.api_key == NULL) {
        L(LOG_WARNING, "No value found for api-key.");
        return 2;
    }
    return 0;
}

void pulse_interrupt(void) {
    struct timespec tnow;
    if (clock_gettime(CLOCK_REALTIME, &tnow)) {
        L(LOG_WARNING, "%s: %s", "Unable to call clock_gettime", strerror(errno));
        return;
    }
    CHECK(0 <= mq_send(mq, (char*) &tnow, sizeof (struct timespec), 0));
    L(LOG_DEBUG, "pulse received");
}

mqd_t open_pulse_queue(int oflag) {
    struct mq_attr attr;
    mqd_t mq;

    /* initialize the queue attributes */
    attr.mq_flags = 0;
    attr.mq_maxmsg = cfg.queue_size;
    attr.mq_msgsize = sizeof (struct timespec);
    attr.mq_curmsgs = 0;

    /* create the message queue */
    mq = mq_open(QUEUE_NAME, oflag, 0644, &attr);
    CHECK((mqd_t) - 1 != mq);
    CHECK(-1 != mq_getattr(mq, &attr));
    L(LOG_INFO, "pulses in queue at startup %d, queue length %d", attr.mq_curmsgs, attr.mq_maxmsg);

    return mq;
}

void cleanup_queue(mqd_t mq) {
    struct mq_attr attr;
    /* cleanup */
    CHECK(-1 != mq_getattr(mq, &attr));
    CHECK((mqd_t) - 1 != mq_close(mq));
    if (cfg.unlink_queue && attr.mq_curmsgs == 0) {
        CHECK((mqd_t) - 1 != mq_unlink(QUEUE_NAME));
        L(LOG_INFO, "queue unlinked");
    }
    L(LOG_INFO, "exit, messages in queue %d", attr.mq_curmsgs);
    closelog();
}

double time_diff(volatile struct timespec tend, volatile struct timespec tstart) {
    double dt = 0.0 + (tend.tv_sec - tstart.tv_sec) + (tend.tv_nsec - tstart.tv_nsec) / 1.0e9;
    return dt;
}

void time_copy(volatile struct timespec *tdest, volatile struct timespec tsource) {
    tdest->tv_sec = tsource.tv_sec;
    tdest->tv_nsec = tsource.tv_nsec;
}

void sig_handler(int signo) {
    if (cfg.receiver && !cfg.sender) {
        cleanup_queue(mq);
        exit(0);
    } else {
        L(LOG_DEBUG, "received termination request");
        stop = 1;
    }
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
// TODO switch to SIMPLEQ

struct send_entry * insert_entry(struct timespec tlast, struct timespec trec, double dt, double power, double elapsedkWh, long pulseCount) {
    struct send_entry *entry = malloc(sizeof (struct send_entry));
    time_copy(&entry->tlast, tlast);
    time_copy(&entry->trec, trec);
    entry->dt = dt;
    entry->power = power;
    entry->elapsedkWh = elapsedkWh;
    entry->pulseCount = pulseCount;
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
    return (3600.0 / dt) / PPWH;
}

void process_data() {
    double power = 0, elapsedkWh = 0, dt = -1;
    struct timespec trec, tout_mq;
    ssize_t bytes_read;
    char timestr[31];

    CHECK(clock_gettime(CLOCK_REALTIME, &tout_mq) != -1);
    tout_mq.tv_sec += TIMEOUT / 2;
    bytes_read = mq_timedreceive(mq, (char*) &trec, sizeof (struct timespec), NULL, &tout_mq);
    if (bytes_read >= 0) {
        if (pulseCount == 0) {
            time_copy(&tstart, trec);
        }
        ++rawCount;
        dt = time_diff(trec, tlast);
        if (rawCount == 1 || dt > DT_MIN) {
            ++pulseCount;
            //Calculate power
            if (pulseCount > 1) {
                power = calc_power(dt);
                //Find kwh elapsed
                elapsedkWh = (1.0 * pulseCount / (PPWH * 1000)); //multiply by 1000 to pulses per wh to kwh convert wh to kwh
                insert_entry(tlast, trec, dt, power, elapsedkWh, pulseCount);
            }
            time_str(timestr, 31, &trec);
            L(LOG_DEBUG, "receive %s: DT=%f, P=%ld(%ld), Power=%f, Elapsed kWh=%f", timestr, dt, pulseCount, rawCount, power, elapsedkWh);
            if (fd_data != NULL)
                fprintf(fd_data, "%ld,%d,%f,%f,%ld,%ld\n", trec.tv_sec, trec.tv_nsec, power, elapsedkWh, pulseCount, rawCount);
        }
        time_copy(&tlast, trec);
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

void run_as_sender() {
    L(LOG_DEBUG, "running as sender");

    TAILQ_INIT(&send_q);
    curl = curl_easy_init();
    if (cfg.datalog != NULL) {
        fd_data = fopen(cfg.datalog, "a+");
        L(LOG_DEBUG, "log data to %s", cfg.datalog);
    }

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

    cleanup_queue(mq);
    curl_easy_cleanup(curl);
    if (fd_data != NULL) {
        fclose(fd_data);
        fd_data = NULL;
    }
}

void run_as_receiver() {
    L(LOG_DEBUG, "running as receiver");

    // sets up the wiringPi library
    //    if (wiringPiSetupGpio() < 0) {
    //        fprintf(stderr, "Unable to setup wiringPi: %s\n", strerror(errno));
    //        return 1;
    //    }
    //    pinMode(17, INPUT);
    //    pullUpDnControl(17, PUD_UP);

    if (wiringPiSetupSys() < 0) {
        L(LOG_WARNING, "Unable to setup wiringPi: %s", strerror(errno));
        exit(1);
    }

    // set Pin 17/0 generate an interrupt on high-to-low transitions
    // and attach myInterrupt() to the interrupt
    if (wiringPiISR(cfg.pin, INT_EDGE_FALLING, &pulse_interrupt) < 0) {
        L(LOG_WARNING, "Unable to setup ISR: %s", strerror(errno));
        exit(1);
    }

    if (!cfg.sender) {
        do {
            sleep(INT_MAX);
        } while (1);
    }
}

// -------------------------------------------------------------------------
// main

int main(int argc, char **argv) {

    CHECK(signal(SIGINT, sig_handler) != SIG_ERR);
    CHECK(signal(SIGTERM, sig_handler) != SIG_ERR);
    CHECK(signal(SIGCHLD, SIG_IGN) != SIG_ERR);
    CHECK(signal(SIGHUP, SIG_IGN) != SIG_ERR);

    parse_opts(argc, argv);
    if (read_config(cfg.config)) {
        return 1;
    }

    if (!cfg.verbose)
        LOG_UPTO(LOG_WARNING);
    openlog(cfg.receiver && cfg.sender ? "emonlight" : cfg.receiver ? "emonlight-rec" : "emonlight-send", LOG_PID | LOG_CONS, LOG_USER);

    if (cfg.daemonize) {
        CHECK(daemon(0, 0) == 0);
        L(LOG_DEBUG, "daemonized");
    }

    int oflag = cfg.receiver && !cfg.sender ? O_WRONLY : !cfg.receiver && cfg.sender ? O_RDONLY : O_RDWR;
    mq = open_pulse_queue(oflag | O_CREAT);

    if (cfg.receiver) {
        run_as_receiver();
    }
    if (cfg.sender) {
        run_as_sender();
    }
    return 0;
}
