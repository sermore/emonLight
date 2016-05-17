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
 * File:   read_config.c
 * Author: Sergio
 *
 * Created on May 9, 2016, 22:18 PM
 */

#include "read_config.h"
#include "main.h"
#include <pwd.h>

struct cfg_t cfg = {
    .config = NULL,
    .daemonize = -1,
    .verbose = -1,
    .buzzer_pin = -1,
    .buzzer_source = NULL,
    .power_soft_threshold = 0,
    .power_soft_threshold_time = 0,
    .power_hard_threshold = 0,
    .power_hard_threshold_time = 0,
    .buzzer_test = 0,
    .data_log = NULL,
    .data_log_defaults = 0,
    .pid_path = NULL,
    .data_store = NULL,
    .homedir = NULL,
    .sources = NULL,
    .sources_length = 0,
    .servers = NULL,
    .servers_length = 0
};

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
        if (cfg.daemonize == 1) {
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

struct source_entry *find_source_from_pin(int pin) {
    int i = 0;
    struct source_entry *s = cfg.sources;
    for (i = 0; i < cfg.sources_length; i++, s++) {
        if (s->pin == pin) {
            return s;
        }
    }
    return NULL;
}

int read_source_settings(const config_setting_t *settings, struct source_entry *source) {
    int ok = 1;
    if (!config_setting_lookup_int(settings, "pin", &source->pin)) {
        L(LOG_ERR, "\npin is missing inside sources configuration");
        ok = 0;
    }
    if (!config_setting_lookup_int(settings, "pulses-per-kilowatt-hour", &source->ppkwh)) {
        L(LOG_ERR, "\npulses-per-kilowatt-hour is missing inside sources configuration");
        ok = 0;
    }
    if (ok) {
        TAILQ_INIT(&source->queue);
        source->maps = NULL;
        source->maps_length = 0;
        source->tlast.tv_sec = 0;
        source->tlast.tv_nsec = 0;
        source->raw_count = 0;
        source->pulse_count = 0;
    }
//    L(LOG_DEBUG, "\nread source settings at line %d status: %d", config_setting_source_line(settings), ok);
    return ok;
}

int read_sources(const config_t* config) {
    int ok = 1;
    config_setting_t *settings = config_lookup(config, "sources");
    if (settings != NULL && config_setting_is_list(settings)) {
        int i, n = config_setting_length(settings);
//        L(LOG_DEBUG, "\nread source list length %d at line %d", n, config_setting_source_line(settings));
        cfg.sources = malloc(n * sizeof (struct source_entry));
        cfg.sources_length = n;
        struct source_entry *source = cfg.sources;
        for (i = 0; i < n; i++, source++) {
            config_setting_t *source_settings = config_setting_get_elem(settings, i);
            if (source_settings != NULL && config_setting_is_group(source_settings)) {
                if (!read_source_settings(source_settings, source)) {
                    L(LOG_ERR, "\nunable to read source configuration group");
                    ok = 0;
                }
            } else {
                L(LOG_ERR, "\nunable to read source configuration group at index %d", i);
                ok = 0;
            }
        }
    } else {
        L(LOG_ERR, "\nunable to read source configuration list");
        ok = 0;
    }
    if (!ok && cfg.sources != NULL) {
        free(cfg.sources);
        cfg.sources = NULL;
    }
    return ok;
}

int read_mapping_settings(const config_setting_t *settings, struct server_mapping_entry *map) {
    int ok = 1;
    int pin;
    if (!config_setting_lookup_int(settings, "pin", &pin)) {
        L(LOG_ERR, "\npin is missing inside mapping configuration");
        ok = 0;
    }
    if (pin <= 0) {
        L(LOG_ERR, "\npin %d incorrect inside mapping configuration", pin);
        ok = 0;
    }
    map->source = find_source_from_pin(pin);
    if (map->source == NULL) {
        L(LOG_ERR, "\npin %d not found in sources configuration", pin);
        ok = 0;
    }
    if (!config_setting_lookup_int(settings, "node-id", &map->node_id)) {
        L(LOG_INFO, "\nnode-id is missing inside mapping configuration");
        map->node_id = 0;
    }
    const char *str;
    if (!config_setting_lookup_string(settings, "api-key", &str)) {
        L(LOG_ERR, "\napi-key is missing inside mapping configuration");
        ok = 0;
    } else {
        map->api_key = strdup(str);
    }
    if (ok) {
        // count the mappings associated to each source
        map->source->maps_length++;
        TAILQ_INIT(&map->queue);
        map->queue_length = 0;
        map->queued = 0;
    }
//    L(LOG_DEBUG, "\nmap settings at line %d read status: %d", config_setting_source_line(settings), ok);
    return ok;
}

int read_mapping(const config_setting_t* settings, struct server_entry *server) {
    int ok = 1;
    config_setting_t *maps_settings = config_setting_get_member(settings, "map");
    if (maps_settings != NULL && config_setting_is_list(maps_settings)) {
        int i, n = config_setting_length(maps_settings);
//        L(LOG_DEBUG, "read map list length %d at line %d", n, config_setting_source_line(maps_settings));
        server->maps = malloc(n * sizeof (struct server_mapping_entry));
        server->maps_length = n;
        struct server_mapping_entry *map = server->maps;
        for (i = 0; i < n; i++, map++) {
            config_setting_t *map_settings = config_setting_get_elem(maps_settings, i);
//            L(LOG_DEBUG, "read map config %d at line %d", i, config_setting_source_line(map_settings));
            if (map_settings != NULL && config_setting_is_group(map_settings)) {
                if (!read_mapping_settings(map_settings, map)) {
                    L(LOG_ERR, "\nerrors in mapping configuration group at index %d.", i);
                    ok = 0;
                }
            } else {
                L(LOG_ERR, "\nunable to read mapping configuration group at index %d.", i);
                ok = 0;
            }
        }
    } else {
        L(LOG_ERR, "\nunable to read mapping configuration list.");
        ok = 0;
    }
    if (!ok && server->maps != NULL) {
        free(server->maps);
        server->maps = NULL;
    }
    return ok;
}

int read_server_settings(const config_setting_t *settings, struct server_entry *server) {
    int ok = 1;
    const char *str;
    if (config_setting_lookup_string(settings, "url", &str)) {
        server->url = strdup(str);
    } else {
        L(LOG_ERR, "\nurl is missing inside sources configuration.");
        ok = 0;
    }
    if (config_setting_lookup_string(settings, "protocol", &str)) {
        server->protocol = strcasecmp(EMONLIGHT_REMOTE, str) == 0 ? EMONLIGHT_REMOTE_ID : (strcasecmp(EMONCMS_REMOTE, str) == 0 ? EMONCMS_REMOTE_ID : 0);
        if (server->protocol == 0) {
            L(LOG_ERR, "\nprotocol '%s' unknown.", str);
            ok = 0;
        }
    } else {
        server->protocol = EMONCMS_REMOTE_ID;
    }
    if (!read_mapping(settings, server)) {
        L(LOG_ERR, "\nunable to read map configuration.");
        ok = 0;
    }
    return ok;
}

void collect_source_mappings() {
    int i;
    struct source_entry *source = cfg.sources;
    for (i = 0; i < cfg.sources_length; i++, source++) {
        int s_cnt = 0;
        source->maps = malloc(source->maps_length * sizeof (struct server_mapping_entry*));
        int j;
        struct server_entry *server = cfg.servers;
        for (j = 0; j < cfg.servers_length; j++, server++) {
            int k;
            struct server_mapping_entry *map = server->maps;
            for (k = 0; k < server->maps_length; k++, map++) {
                if (map->source == source) {
                    source->maps[s_cnt++] = map;
                }
            }
        }
        CHECK(source->maps_length == s_cnt);
    }
}

int read_servers(const config_t* config) {
    int ok = 1;
    config_setting_t *settings = config_lookup(config, "servers");
    if (settings != NULL && config_setting_is_list(settings)) {
//        L(LOG_DEBUG, "read servers config at line %d", config_setting_source_line(settings));
        int i, n = config_setting_length(settings);
        cfg.servers = malloc(n * sizeof (struct server_entry));
        cfg.servers_length = n;
        struct server_entry *server = cfg.servers;
        for (i = 0; i < n; i++, server++) {
            config_setting_t *s = config_setting_get_elem(settings, i);
//            L(LOG_DEBUG, "read server config at line %d", config_setting_source_line(s));
            if (s != NULL && config_setting_is_group(s)) {
                if (!read_server_settings(s, server)) {
                    L(LOG_ERR, "\nunable to read server configuration group");
                    ok = 0;
                }
            } else {
                L(LOG_ERR, "\nunable to read server configuration group at index %d inside server at line %d", i, config_setting_source_line(settings));
                ok = 0;
            }
        }
    } else {
        L(LOG_ERR, "\nunable to read server configuration list");
        ok = 0;
    }
    if (!ok && cfg.servers != NULL) {
        free(cfg.servers);
        cfg.servers = NULL;
        return 0;
    }
    collect_source_mappings();
    return ok;
}

void print_sources() {
    printf("sources(%d):\n", cfg.sources_length);
    int i;
    struct source_entry *s = cfg.sources;
    for (i = 0; i < cfg.sources_length; i++, s++) {
        printf("\tsource %d: pin: %d, ppkwh: %d\n", i, s->pin, s->ppkwh);
    }
}

void print_servers() {
    printf("servers(%d):\n", cfg.servers_length);
    int i;
    struct server_entry *srv = cfg.servers;
    for (i = 0; i < cfg.servers_length; i++, srv++) {
        printf("\tserver %d: url: %s, protocol: %s\n", i, srv->url, srv->protocol == EMONLIGHT_REMOTE_ID ? EMONLIGHT_REMOTE : EMONCMS_REMOTE);
        int j = 0;
        struct server_mapping_entry *map = srv->maps;
        for (j = 0; j < srv->maps_length; j++, map++) {
            printf("\t\tmap %d: pin: %d, node-id: %d, api-key: %s\n", j, map->source->pin, map->node_id, map->api_key);
        }
    }
}

int read_config(const char *config_file) {
    config_t config;
    const char *str;
    int tmp;

    cfg.config = get_config_file(config_file);
    if (access(cfg.config, F_OK) != 0) {
        L(LOG_ERR, "\nUnable to access configuration file '%s'", cfg.config);
        return 1;
    }
    int ok = 1;
    /*Initialization */
    config_init(&config);
    /* Read the file. If there is an error, report it and exit. */
    if (!config_read_file(&config, cfg.config)) {
        L(LOG_WARNING, "\n%s:%d - %s", config_error_file(&config), config_error_line(&config), config_error_text(&config));
        config_destroy(&config);
        return 1;
    }
    if (cfg.daemonize == -1) {
        if (config_lookup_bool(&config, "daemonize", &tmp)) {
            cfg.daemonize = tmp;
        } else {
            cfg.daemonize = 0;
        }
    }
    if (cfg.verbose == -1) {
        if (config_lookup_bool(&config, "verbose", &tmp)) {
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
    if (!read_sources(&config)) {
        return 2;
    }
//    print_sources();
    if (!read_servers(&config)) {
        return 3;
    }
//    print_servers();
    
    if (cfg.buzzer_pin == -1) {
        if (config_lookup_int(&config, "buzzer-pin", &tmp)) {
            cfg.buzzer_pin = tmp;
        }
    }
    if (config_lookup_int(&config, "buzzer-source", &tmp)) {
        cfg.buzzer_source = find_source_from_pin(tmp);
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
    config_destroy(&config);
    return ok ? 0 : 4;
}
