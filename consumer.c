#include <glib.h>
#include <librdkafka/rdkafka.h>

#include "man_id.h"

static volatile sig_atomic_t run = 1;

/**
 * @brief Signal termination of program
 */
static void stop(int sig) {
    run = 0;
}

int main (int argc, char **argv) {
    rd_kafka_t *consumer;
    rd_kafka_conf_t *conf;
    rd_kafka_resp_err_t err;
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
    set_config(conf, "group.id",          "$Default");
    set_config(conf, "auto.offset.reset", "earliest");

    // Create the Consumer instance.
    consumer = rd_kafka_new(RD_KAFKA_CONSUMER, conf, errstr, sizeof(errstr));
    if (!consumer) {
        g_error("Failed to create new consumer: %s", errstr);
        return 1;
    }
    rd_kafka_poll_set_consumer(consumer);

    // Configuration object is now owned, and freed, by the rd_kafka_t instance.
    conf = NULL;

    // Convert the list of topics to a format suitable for librdkafka.
    rd_kafka_topic_partition_list_t *subscription = rd_kafka_topic_partition_list_new(1);
    rd_kafka_topic_partition_list_add(subscription, topic, RD_KAFKA_PARTITION_UA);

    // Subscribe to the list of topics.
    err = rd_kafka_subscribe(consumer, subscription);
    if (err) {
        g_error("Failed to subscribe to %d topics: %s", subscription->cnt, rd_kafka_err2str(err));
        rd_kafka_topic_partition_list_destroy(subscription);
        rd_kafka_destroy(consumer);
        return 1;
    }

    rd_kafka_topic_partition_list_destroy(subscription);

    // Install a signal handler for clean shutdown.
    signal(SIGINT, stop);

    // Start polling for messages.
    while (run) {
        rd_kafka_message_t *consumer_message;

        consumer_message = rd_kafka_consumer_poll(consumer, 500);
        if (!consumer_message) {
            g_message("Waiting...");
            continue;
        }

        if (consumer_message->err) {
            if (consumer_message->err == RD_KAFKA_RESP_ERR__PARTITION_EOF) {
                /* We can ignore this error - it just means we've read
                 * everything and are waiting for more data.
                 */
            } else {
                g_message("Consumer error: %s", rd_kafka_message_errstr(consumer_message));
                return 1;
            }
        } else {
            g_message("Consumed event from topic %s: key = %.*s value = %s",
                      rd_kafka_topic_name(consumer_message->rkt),
                      (int)consumer_message->key_len,
                      (char *)consumer_message->key,
                      (char *)consumer_message->payload
                      );
        }

        // Free the message when we're done.
        rd_kafka_message_destroy(consumer_message);
    }

    // Close the consumer: commit final offsets and leave the group.
    g_message( "Closing consumer");
    rd_kafka_consumer_close(consumer);

    // Destroy the consumer.
    rd_kafka_destroy(consumer);

    return 0;
}