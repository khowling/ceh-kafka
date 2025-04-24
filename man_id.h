#ifndef MAN_ID_H
#define MAN_ID_H

#include <librdkafka/rdkafka.h>

typedef struct {
    const char *client_id;
    const char *eh_name;
} man_id_t;

// Expose the oauth_cb function for use in other files
void oauth_cb(rd_kafka_t *rk, const char *oauthbearer_config, void *opaque);

void set_config(rd_kafka_conf_t *conf, char *key, const char *value);

#endif // MAN_ID_H