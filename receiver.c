
#include "main.h"
#include <wiringPi.h>

#define CLOCKID CLOCK_REALTIME
#define SIG_UP SIGRTMIN
#define SIG_DOWN (SIGRTMIN+1)

volatile long rawCount = 0;
struct timespec tlast = {0L, 0L};
struct timespec tnow = {0L, 0L};
volatile long pulseCount = 0;
volatile short buzzer_enabled = 0;
volatile int buzzer_pulses = 0, buzzer_cnt = 10;
timer_t buzzer_timerup, buzzer_timerdown;
double soft_power_acc = 0;
long soft_power_time = 0;
double hard_power_acc = 0;
long hard_power_time;


double calc_power(double dt) {
    return (3600000.0 / dt) / cfg.ppkwh;
}

static void buzzer_timer_handler(int sig, siginfo_t *si, void *uc) {
    //printf("Caught signal %d BC=%d, BP=%d\n", sig, buzzer_cnt, buzzer_pulses);
    if (sig == SIG_UP && buzzer_cnt < buzzer_pulses) {
        digitalWrite(cfg.buzzer_pin, 1);
    } else if (sig == SIG_DOWN) {
        digitalWrite(cfg.buzzer_pin, 0);
        ++buzzer_cnt;
        if (buzzer_cnt >= 10) {
            struct itimerspec its;
            its.it_value.tv_sec = 0L;
            its.it_value.tv_nsec = 0L;
            its.it_interval.tv_sec = 0L;
            its.it_interval.tv_nsec = 0L;
            CHECK(timer_settime(buzzer_timerup, 0, &its, NULL) != -1);
            CHECK(timer_settime(buzzer_timerdown, 0, &its, NULL) != -1);
            buzzer_cnt = 0;
            buzzer_enabled = 0;
        }
    }
}

void buzzer_timer_setup(timer_t *buzzer_timer, int sig, void *handler) {
    struct sigaction sa;
    sigset_t mask;
    struct sigevent sev;
    //L(LOG_DEBUG, "Establishing handler for signal %d\n", sig);
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = handler;
    sigemptyset(&sa.sa_mask);
    CHECK(sigaction(sig, &sa, NULL) != -1);

    /* Block timer signal temporarily */
    //L(LOG_DEBUG, "Blocking signal %d\n", sig);
    sigemptyset(&mask);
    sigaddset(&mask, sig);
    CHECK(sigprocmask(SIG_SETMASK, &mask, NULL) != -1);

    /* Create the timer */

    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = sig;
    sev.sigev_value.sival_ptr = buzzer_timer;
    CHECK(timer_create(CLOCKID, &sev, buzzer_timer) != -1);
    //printf("timer ID is 0x%lx\n", (long) buzzer_timer);
    CHECK(sigprocmask(SIG_UNBLOCK, &mask, NULL) != -1);
}

static void buzzer_setup() {
    if (cfg.buzzer_pin != -1) {
        buzzer_pulses = 0;
        buzzer_timer_setup(&buzzer_timerup, SIG_UP, buzzer_timer_handler);
        buzzer_timer_setup(&buzzer_timerdown, SIG_DOWN, buzzer_timer_handler);
    }
}

int calc_buzzer_pulses(double power, double elapsedkWh) {
    int hs_limit = power >= cfg.power_hard_threshold;
    double dp_kwh = elapsedkWh - (hs_limit ? hard_power_acc : soft_power_acc);
    double max_p_kwh = 1.0 * (hs_limit ? cfg.power_hard_limit : cfg.power_soft_limit) / 1000;
    int buzzer_pulses = (hs_limit ? 4 : 1) + 3.0 * (dp_kwh / max_p_kwh);
    L(LOG_DEBUG, "power usage warning %d, hard=%d, dp=%f, %%%f", buzzer_pulses, hs_limit, dp_kwh, 100 * dp_kwh / max_p_kwh);
    return buzzer_pulses;
}

static void buzzer_control(double power, double elapsedkWh) {
    if (cfg.buzzer_pin == -1)
        return;

    // data for hard and soft power limits
    if (tnow.tv_sec >= cfg.power_soft_threshold_time + soft_power_time) {
        soft_power_time = tnow.tv_sec;
        soft_power_acc = elapsedkWh;
    }
    if (tnow.tv_sec >= cfg.power_hard_threshold_time + hard_power_time) {
        hard_power_time = tnow.tv_sec;
        hard_power_acc = elapsedkWh;
    }
    //L(LOG_DEBUG, "soft p=%f, dt=%ld, hard p=%f, dt=%ld", soft_power_acc, tnow.tv_sec - soft_power_time, hard_power_acc, tnow.tv_sec - hard_power_time);

    if (power >= cfg.power_soft_threshold || power >= cfg.power_hard_threshold) {
        if (!buzzer_enabled) {
            // buzzer config
            buzzer_enabled = 1;
            buzzer_cnt = 0;
            buzzer_pulses = calc_buzzer_pulses(power, elapsedkWh);
            long q = 70;
            struct itimerspec its;
            its.it_value.tv_sec = 0L;
            its.it_value.tv_nsec = q * 1000000;
            its.it_interval.tv_sec = 0L;
            its.it_interval.tv_nsec = q * 3000000L;
            CHECK(timer_settime(buzzer_timerup, 0, &its, NULL) != -1);
            its.it_value.tv_nsec = q * 2000000;
            its.it_interval.tv_nsec = q * 3000000L;
            CHECK(timer_settime(buzzer_timerdown, 0, &its, NULL) != -1);
        }
    }
}

static void buzzer_test() {
    buzzer_control(cfg.power_soft_threshold, 0.7 * cfg.power_soft_limit / 1000);
    int i;
    for (i = 0; i < 10; i++) {
        sleep(1);
    }
}

static void pulse_interrupt(void) {
    double power = 0, elapsedkWh = 0;
    struct send_entry entry;
    char timestr[31];

    if (clock_gettime(CLOCK_REALTIME, &tnow)) {
        L(LOG_WARNING, "%s: %s", "Unable to call clock_gettime", strerror(errno));
        return;
    }

    ++rawCount;
    double dt = time_diff(tnow, tlast);
    if (rawCount == 1 || dt > DT_MIN) {
        ++pulseCount;
        //Calculate power
        if (pulseCount > 1 && dt < DT_MAX) {
            power = calc_power(dt);
            // calculate kwh elapsed
            elapsedkWh = 1.0 * pulseCount / cfg.ppkwh; //multiply by 1000 to pulses per wh to kwh convert wh to kwh
            populate_entry(&entry, tlast, tnow, dt, power, elapsedkWh, pulseCount, rawCount);
            buzzer_control(power, elapsedkWh);
            CHECK(0 <= mq_send(mq, (char*) &entry, sizeof (struct send_entry), 0));
        }
        time_str(timestr, 31, &tnow);
        L(LOG_DEBUG, "receive %s: DT=%f, P=%ld(%ld), Power=%f, Elapsed kWh=%f", timestr, dt, pulseCount, rawCount, power, elapsedkWh);
    }
    time_copy(&tlast, tnow);
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

void receiver_init() {
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
        exit(EXIT_FAILURE);
    }

    buzzer_setup();
    if (cfg.buzzer_test) {
        buzzer_test();
        exit(EXIT_FAILURE);
    }

    // set Pin 17/0 generate an interrupt on high-to-low transitions
    // and attach myInterrupt() to the interrupt
    if (wiringPiISR(cfg.pulse_pin, INT_EDGE_FALLING, &pulse_interrupt) < 0) {
        L(LOG_WARNING, "Unable to setup ISR: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    data_store_load();
}

void receiver_at_exit() {
    data_store_save();
    if (cfg.buzzer_pin != -1) {
        digitalWrite(cfg.buzzer_pin, 0);
    }
}

void receiver_loop() {
    if (!cfg.sender) {
        sleep(TIMEOUT);
    }
}