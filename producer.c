#include <glib.h>
#include <librdkafka/rdkafka.h>
#include <stdlib.h> // For getenv()

#include "man_id.h"

#define ARR_SIZE(arr) ( sizeof((arr)) / sizeof((arr[0])) )


/* Optional per-message delivery callback (triggered by poll() or flush())
 * when a message has been successfully delivered or permanently
 * failed delivery (after retries).
 */
static void dr_msg_cb (rd_kafka_t *kafka_handle,
                       const rd_kafka_message_t *rkmessage,
                       void *opaque) {
    if (rkmessage->err) {
        g_error("Message delivery failed: %s", rd_kafka_err2str(rkmessage->err));
    }
}

int main (int argc, char **argv) {
    rd_kafka_t *producer;
    rd_kafka_conf_t *conf;
    char errstr[512];

    const char *eh_name = getenv("EH_NAME");
    const char *client_id = getenv("CLIENT_ID");
    const char *topic = getenv("TOPIC");

    // Required for Service Principle Auth, NOT for Managed Identity
    const char *client_secret = getenv("CLIENT_SECRET");
    const char *token_endpoint = getenv("TOKEN_ENDPOINT");

    // Check if required environment variables are set
    if (!eh_name || !topic) {
        g_error("Required environment variables not set. Please set:\n"
                "TOPIC\n"
                "EH_NAME");
        return 1;
    }

    // Create client configuration
    conf = rd_kafka_conf_new();

    char bootstrap_servers[256];
    snprintf(bootstrap_servers, sizeof(bootstrap_servers), "%s.servicebus.windows.net:9093", eh_name);
    set_config(conf, "bootstrap.servers", bootstrap_servers);
    set_config(conf, "security.protocol", "SASL_SSL");
    set_config(conf, "sasl.mechanism", "OAUTHBEARER");

    if (!client_secret && !token_endpoint ) {
        // If not Client secret and token endpoint provided, assume will use Managed Identity, so reqister the callback
        man_id_t *man_id = g_malloc(sizeof(man_id_t));
        man_id->client_id = client_id;
        man_id->eh_name = eh_name;

        rd_kafka_conf_set_opaque(conf, man_id);
        rd_kafka_conf_set_oauthbearer_token_refresh_cb(conf, oauth_cb);

    } else {
        set_config(conf, "sasl.oauthbearer.method", "OIDC");
        set_config(conf, "sasl.oauthbearer.client.id", client_id);
        set_config(conf, "sasl.oauthbearer.client.secret",  client_secret);
        set_config(conf, "sasl.oauthbearer.token.endpoint.url", token_endpoint);

        char scope[256];
        snprintf(scope, sizeof(scope), "https://%s.servicebus.windows.net/.default", eh_name);
        set_config(conf, "sasl.oauthbearer.scope", scope);
    }

    // Fixed properties
    set_config(conf, "acks", "all");

    // Install a delivery-error callback.
    rd_kafka_conf_set_dr_msg_cb(conf, dr_msg_cb);

    // Create the Producer instance.
    producer = rd_kafka_new(RD_KAFKA_PRODUCER, conf, errstr, sizeof(errstr));
    if (!producer) {
        g_error("Failed to create new producer: %s", errstr);
        return 1;
    }

    // Configuration object is now owned, and freed, by the rd_kafka_t instance.
    conf = NULL;

    // Produce data by selecting random values from these lists.
    int message_count = 10;
    const char *user_ids[6] = {"eabara", "jsmith", "sgarcia", "jbernard", "htanaka", "awalther"};
    const char *products[5] = {"book", "alarm clock", "t-shirts", "gift card", "batteries"};

    for (int i = 0; i < message_count; i++) {
        const char *key =  user_ids[random() % ARR_SIZE(user_ids)];
        const char *value =  products[random() % ARR_SIZE(products)];
        size_t key_len = strlen(key);
        size_t value_len = strlen(value);

        rd_kafka_resp_err_t err;

        const char *header_key = "RicName";
        const char *header_value = "LSEG-XXXX";

        rd_kafka_headers_t *headers = rd_kafka_headers_new(1);
        rd_kafka_header_add(headers, header_key, -1, header_value, strlen(header_value));

        err = rd_kafka_producev(producer,
                    RD_KAFKA_V_TOPIC(topic),
                    RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_COPY),
                    RD_KAFKA_V_KEY((void*)key, key_len),
                    RD_KAFKA_V_VALUE((void*)value, value_len),
                    RD_KAFKA_V_HEADERS(headers),
                    RD_KAFKA_V_OPAQUE(NULL),
                    RD_KAFKA_V_END);

        if (err) {
            g_error("Failed to produce to topic %s: %s", topic, rd_kafka_err2str(err));
            return 1;
        } else {
            g_message("Produced event to topic %s: key = %12s value = %12s", topic, key, value);
        }

        rd_kafka_poll(producer, 0);
    }

    // Block until the messages are all sent.
    g_message("Flushing final messages..");
    rd_kafka_flush(producer, 10 * 1000);

    if (rd_kafka_outq_len(producer) > 0) {
        g_error("%d message(s) were not delivered", rd_kafka_outq_len(producer));
    }

    g_message("%d events were produced to topic %s.", message_count, topic);

    rd_kafka_destroy(producer);

    return 0;
}