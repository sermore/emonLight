/* 
 * File:   main.c
 * Author: Sergio
 *
 * Created on April 12, 2015, 10:21 AM
 */
#include <getopt.h>
#include <pwd.h>
#include <sys/types.h>
#include "main.h"

#define YN(v) (v == 1 ? "YES" : "NO")

struct cfg_t cfg = {
    .config = NULL,
    .emocms_url = NULL,
    .daemonize = 0,
    .receiver = 0,
    .sender = 0,
    .unlink_queue = 0,
    .queue_size = 0,
    .verbose = -1,
    .pulse_pin = -1,
    .buzzer_pin = -1,
    .power_soft_threshold = 0,
    .buzzer_test = 0,
    .ppkwh = 0,
    .api_key = NULL,
    .node_id = -1,
    .data_log = NULL,
    .data_log_defaults = 0,
    .pid_path = NULL,
    .data_store = NULL,
    .homedir = NULL
};

mqd_t mq = -1;
int pidFilehandle = -1;

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
            {"receiver", no_argument, 0, 'r'},
            {"sender", no_argument, 0, 's'},
            {"unlink-queue", no_argument, 0, 'u'},
            {"queue-size", required_argument, 0, 'q'},
            {"verbose", no_argument, 0, 'v'},
            {"pulse-pin", required_argument, 0, 'p'},
            {"api-key", required_argument, 0, 'k'},
            {"node-id", required_argument, 0, 'n'},
            {"emocms-url", required_argument, 0, 'e'},
            {"data-log", optional_argument, 0, 'l'},
            {"pid-path", required_argument, 0, 't'},
            {"data-store", required_argument, 0, 'a'},
            {"pulses-per-kilowatt-hour", required_argument, 0, 'w'},
            {"buzzer-pin", required_argument, 0, 'b'},
            {"power-soft-threshold", required_argument, 0, 'o'},
            {"power-soft-threshold-time", required_argument, 0, 'y'},
            {"power-soft-limit", required_argument, 0, 'f'},
            {"power-hard-threshold", required_argument, 0, 'm'},
            {"power-hard-threshold-time", required_argument, 0, 'x'},
            {"power-hard-limit", required_argument, 0, 'g'},
            {"buzzer-test", no_argument, 0, 'z'},
            {"help", no_argument, 0, 'h'},
            {0, 0, 0, 0}
        };

        c = getopt_long(argc, argv, "c:drsuq:vp:k:ne:l::t:a:w:b:o:y:f:m:x:g:zh", long_options, &option_index);
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
                cfg.receiver = 1;
                break;
            case 's':
                cfg.sender = 1;
                break;
            case 'p':
                optarg_to_int(&cfg.pulse_pin, argv[optind]);
                break;
            case 'q':
                optarg_to_int(&cfg.queue_size, argv[optind]);
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
                optarg_to_int(&cfg.node_id, argv[optind]);
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
            case 'w':
                optarg_to_int(&cfg.ppkwh, argv[optind]);
                break;
            case 'b':
                optarg_to_int(&cfg.buzzer_pin, argv[optind]);
                break;
            case 'o':
                optarg_to_int(&cfg.power_soft_threshold, argv[optind]);
                break;
            case 'y':
                optarg_to_int(&cfg.power_soft_threshold_time, argv[optind]);
                break;
            case 'f':
                optarg_to_int(&cfg.power_soft_limit, argv[optind]);
                break;
            case 'm':
                optarg_to_int(&cfg.power_hard_threshold, argv[optind]);
                break;
            case 'x':
                optarg_to_int(&cfg.power_hard_threshold_time, argv[optind]);
                break;
            case 'g':
                optarg_to_int(&cfg.power_hard_limit, argv[optind]);
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
    if (strstr(argv[0], "emonlight-send") != NULL) {
        cfg.sender = 1;
        cfg.receiver = 0;
    } else if (strstr(argv[0], "emonlight-rec") != NULL) {
        cfg.sender = 0;
        cfg.receiver = 1;
    } else if (!cfg.receiver && !cfg.sender) {
        cfg.receiver = 1;
        cfg.sender = 1;
    }
    if (optind < argc) {
        printf("non-option ARGV-elements: ");
        while (optind < argc)
            printf("%s ", argv[optind++]);
        printf("\n");
    }
}

static const char *get_homedir() {
    if (cfg.homedir == NULL) {
        if ((cfg.homedir = getenv("HOME")) == NULL) {
            cfg.homedir = getpwuid(getuid())->pw_dir;
        }
    }
    return cfg.homedir;
}

const char* get_config_file(const char *config_file) {
    const char *dest_file;
    if (config_file == NULL) {
        if (cfg.daemonize) {
            dest_file = "/etc/emonlight.conf";
        } else {
            char buf[2048];
            strcpy(buf, get_homedir());
            strcat(buf, "/.emonlight");
            // FIXME test for string length
            dest_file = strdup(buf);
        }
    } else {
        dest_file = config_file;
    }
    return dest_file;
}

int read_config(const char *config_file) {
    config_t config;
    const char *str;
    int tmp;

    cfg.config = get_config_file(config_file);
    if (access(cfg.config, F_OK) == 0) {
        /*Initialization */
        config_init(&config);
        /* Read the file. If there is an error, report it and exit. */
        if (!config_read_file(&config, cfg.config)) {
            L(LOG_WARNING, "\n%s:%d - %s", config_error_file(&config), config_error_line(&config), config_error_text(&config));
            config_destroy(&config);
            return 1;
        }
        if (cfg.sender) {
            if (cfg.api_key == NULL) {
                if (config_lookup_string(&config, "api-key", &str))
                    cfg.api_key = strdup(str);
            }
            if (cfg.emocms_url == NULL) {
                if (config_lookup_string(&config, "emocms-url", &str))
                    cfg.emocms_url = strdup(str);
                else {
                    cfg.emocms_url = EMOCMS_URL;
                }
            }
            if (cfg.node_id == -1) {
                if (config_lookup_int(&config, "node-id", &tmp))
                    cfg.node_id = tmp;
                else {
                    cfg.node_id = 1;
                }
            }
        }
        if (cfg.receiver) {
            if (cfg.pulse_pin == -1) {
                if (config_lookup_int(&config, "pulse-pin", &tmp))
                    cfg.pulse_pin = tmp;
                else {
                    cfg.pulse_pin = 17;
                }
            }
            if (cfg.buzzer_pin == -1) {
                if (config_lookup_int(&config, "buzzer-pin", &tmp)) {
                    cfg.buzzer_pin = tmp;
                } else {
                    // FIXME
                    cfg.buzzer_pin = 3;
                }
            }
            if (cfg.data_log == NULL) {
                if (config_lookup_string(&config, "data-log", &str)) {
                    cfg.data_log = strdup(str);
                } else if (cfg.data_log_defaults) {
                    const char *home = get_homedir();
                    if (cfg.daemonize || home == NULL) {
                        cfg.data_log = "/var/lib/emonlight/emonlight-data.log";
                    } else {
                        cfg.data_log = malloc(strlen(home) + strlen("/emonlight-data.log"));
                        strcpy(cfg.data_log, home);
                        strcat(cfg.data_log, "/emonlight-data.log");
                    }
                }
            }
            if (cfg.data_store == NULL) {
                if (config_lookup_string(&config, "data-store", &str)) {
                    cfg.data_store = strdup(str);
                } else {
                    const char *home = get_homedir();
                    if (cfg.daemonize || home == NULL) {
                        cfg.data_store = "/var/lib/emonlight/emonlight-data";
                    } else {
                        cfg.data_store = malloc(strlen(home) + strlen("/.emonlight-data"));
                        strcpy(cfg.data_store, home);
                        strcat(cfg.data_store, "/.emonlight-data");
                    }
                }
            }
            if (cfg.ppkwh == 0) {
                if (config_lookup_int(&config, "pulses-per-kilowatt-hour", &tmp)) {
                    cfg.ppkwh = tmp;
                } else {
                    cfg.ppkwh = 1000;
                }
            }
            if (cfg.power_soft_threshold == 0) {
                if (config_lookup_int(&config, "power-soft-threshold", &tmp)) {
                    cfg.power_soft_threshold = tmp;
                } else {
                    cfg.power_soft_threshold = 3300;
                }
            }
            if (cfg.power_soft_threshold_time == 0) {
                if (config_lookup_int(&config, "power-soft-threshold-time", &tmp)) {
                    cfg.power_soft_threshold_time = tmp;
                } else {
                    cfg.power_soft_threshold_time = 3600 * 3;
                }
            }
            if (cfg.power_soft_limit == 0) {
                if (config_lookup_int(&config, "power-soft-limit", &tmp)) {
                    cfg.power_soft_limit = tmp;
                } else {
                    cfg.power_soft_limit = 4000;
                }
            }
            if (cfg.power_hard_threshold == 0) {
                if (config_lookup_int(&config, "power-hard-threshold", &tmp)) {
                    cfg.power_hard_threshold = tmp;
                } else {
                    cfg.power_hard_threshold = 4000;
                }
            }
            if (cfg.power_hard_threshold_time == 0) {
                if (config_lookup_int(&config, "power-hard-threshold-time", &tmp)) {
                    cfg.power_hard_threshold_time = tmp;
                } else {
                    cfg.power_hard_threshold_time = 240;
                }
            }
            if (cfg.power_hard_limit == 0) {
                if (config_lookup_int(&config, "power-hard-limit", &tmp)) {
                    cfg.power_hard_limit = tmp;
                } else {
                    cfg.power_hard_limit = 267;
                }
            }
        }
        if (cfg.queue_size == 0) {
            if (config_lookup_int(&config, "queue-size", &tmp)) {
                cfg.queue_size = tmp;
            } else {
                cfg.queue_size = 1024;
                //TODO verification of SO limits
            }
        }
        if (cfg.verbose == -1) {
            if (config_lookup_int(&config, "verbose", &tmp)) {
                cfg.verbose = tmp;
            } else {
                cfg.verbose = 0;
            }
        }
        if (cfg.pid_path == NULL) {
            if (config_lookup_string(&config, "pid-path", &str))
                cfg.pid_path = strdup(str);
            else {
                cfg.pid_path = cfg.daemonize ? "/var/run" : "/tmp";
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

void help() {
    printf("Usage \n"
            "Options:\n"
            "-c, --config=FILE                      read configuration from specified file\n"
            "-d, --daemon                           execute as daemon\n"
            "-r, --receiver                         act a receiver only, read pulses and queue power information\n"
            "-s, --sender                           act as sender only, read data from queue and send to data-logger site\n"
            "-u, --unlink-queue                     unlink queue if empty at program termination\n"
            "-q, --queue-size=NUMBER                set queue length to NUMBER\n"
            "-v, --verbose                          enable verbose print and log\n"
            "-p, --pulse-pin=NUMBER                 use pin NUMBER as input for reading pulse signal, use GPIO numbering\n"
            "-k --api-key=API_KEY                   use API_KEY for communication to emoncms.org\n"
            "-n --node-id=NODE_NUMBER               use NODE_NUMBER as node-id for emoncms.org\n"
            "-e, --emocms-url=URL                   use URL to connecto to emoncms.org instance\n"
            "-l, --data-log=FILE                    append to FILE data retrieved\n"
            "-t, --pid-path=PATH                    store pid file in directory PATH\n"
            "-a, --data-store=FILE                  save receiver status on termination\n"
            "-w, --pulses-per-kilowatt-hour=NUM     calculate power considering NUM pulses for 1 kWh\n"
            "-b, --buzzer-pin=NUMBER                use pin NUMBER as output to drive buzzer\n"
            "-o, --power-soft-threshold=NUM         use NUM as power soft threshold\n"
            "-y, --power-soft-threshold-time=NUM    timer limit for power exceeding soft threshold\n"
            "-m, --power-hard-threshold=NUM         use NUM as power hard threshold\n"
            "-x, --power-hard-threshold-time=NUM    timer limit for power exceeding soft threshold\n"
            "-z, --buzzer-test                      test buzzer and exit\n"
            "-h, --help                             this\n"
            );
    printf("Configuration:\n"
            "configuration file %s\n"
            "perform as daemon %s\n"
            "act as receiver %s\n"
            "act as sender %s\n"
            "unlink queue on termination %s\n"
            "queue size %d\n"
            "verbose mode %s\n"
            "pulse pin %d\n"
            "api key %s\n"
            "node id %d\n"
            "emoncms URL %s\n"
            "data log file %s\n"
            "path for pid file %s\n"
            "data store file %s\n"
            "pulses per kilowatt per hour %d\n"
            "buzzer output pin %d\n"
            "power soft threshold %d\n"
            "power soft threshold time (seconds) %d\n"
            "power hard threshold %d\n"
            "power hard threshold time (seconds) %d\n",
            cfg.config, YN(cfg.daemonize), YN(cfg.receiver), YN(cfg.sender), 
            YN(cfg.unlink_queue), cfg.queue_size, YN(cfg.verbose), 
            cfg.pulse_pin, cfg.api_key, cfg.node_id, cfg.emocms_url,
            cfg.data_log, cfg.pid_path, cfg.data_store, cfg.ppkwh, cfg.buzzer_pin,
            cfg.power_soft_threshold, cfg.power_soft_threshold_time,
            cfg.power_hard_threshold, cfg.power_hard_threshold_time);
    exit(EXIT_FAILURE);
}

mqd_t queue_open(int oflag) {
    struct mq_attr attr;
    mqd_t mq;

    /* initialize the queue attributes */
    attr.mq_flags = 0;
    attr.mq_maxmsg = cfg.queue_size;
    attr.mq_msgsize = sizeof (struct send_entry);
    attr.mq_curmsgs = 0;

    /* create the message queue */
    mq = mq_open(QUEUE_NAME, oflag, 0644, &attr);
    CHECK((mqd_t) - 1 != mq);
    CHECK(-1 != mq_getattr(mq, &attr));
    L(LOG_DEBUG, "pulses in queue at startup %ld, queue length %ld, msg size %ld", attr.mq_curmsgs, attr.mq_maxmsg, attr.mq_msgsize);

    return mq;
}

static void queue_cleanup(mqd_t mq) {
    struct mq_attr attr;
    /* cleanup */
    CHECK(-1 != mq_getattr(mq, &attr));
    CHECK((mqd_t) - 1 != mq_close(mq));
    if (cfg.unlink_queue && attr.mq_curmsgs == 0) {
        CHECK((mqd_t) - 1 != mq_unlink(QUEUE_NAME));
        L(LOG_DEBUG, "queue unlinked");
    }
    L(LOG_DEBUG, "exit, messages in queue %ld", attr.mq_curmsgs);
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

struct send_entry* populate_entry(struct send_entry *entry, struct timespec tlast, struct timespec trec, double dt, double power, double elapsedkWh, long pulseCount) {
    time_copy(&entry->tlast, tlast);
    time_copy(&entry->trec, trec);
    entry->dt = dt;
    entry->power = power;
    entry->elapsedkWh = elapsedkWh;
    entry->pulseCount = pulseCount;
    entry->entries.tqe_next = NULL;
    entry->entries.tqe_prev = NULL;
    return entry;
}

static void sig_handler(int signo) {
    L(LOG_DEBUG, "received termination request");
    if (cfg.receiver)
        receiver_sig_handler(signo);
    if (cfg.sender)
        sender_sig_handler(signo);
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

static const char *log_name() {
    return (cfg.receiver && cfg.sender) ? "emonlight" : (cfg.receiver ? "emonlight-rec" : "emonlight-send");
}

static int pidfile(char *str) {
    return sprintf(str, "%s/%s.pid", cfg.pid_path, log_name());
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

    if (cfg.sender) {
        sender_at_exit();
    }

    if (cfg.receiver) {
        receiver_at_exit();
    }

    if (mq != -1) {
        queue_cleanup(mq);
    }

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
    openlog(log_name(), LOG_PID | LOG_CONS, LOG_USER);

    if (cfg.daemonize)
        daemonize();

    if (cfg.config != NULL)
        L(LOG_INFO, "read config from %s", cfg.config);

    int oflag = cfg.receiver && !cfg.sender ? O_WRONLY : !cfg.receiver && cfg.sender ? O_RDONLY : O_RDWR;
    mq = queue_open(oflag | O_CREAT);

    if (cfg.receiver) {
        receiver_run();
    }
    if (cfg.sender) {
        sender_run();
    }

    return 0;
}
