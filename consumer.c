#include <glib.h>
#include <librdkafka/rdkafka.h>

#include "common.c"

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

    const char *client_id = getenv("CLIENT_ID");
    const char *client_secret = getenv("CLIENT_SECRET");
    const char *token_endpoint = getenv("TOKEN_ENDPOINT");
    const char *eh_name = getenv("EH_NAME");

    // Check if required environment variables are set
    if (!client_id || !client_secret || !token_endpoint || !eh_name) {
        g_error("Required environment variables not set. Please set:\n"
                "CLIENT_ID\n"
                "CLIENT_SECRET\n"
                "TOKEN_ENDPOINT\n"
                "EH_NAME");
        return 1;
    }
    // Create client configuration
    conf = rd_kafka_conf_new();

    set_config(conf, "sasl.oauthbearer.method", "OIDC");
    set_config(conf, "sasl.oauthbearer.client.id", client_id);
    set_config(conf, "sasl.oauthbearer.client.secret",  client_secret);
    set_config(conf, "sasl.oauthbearer.token.endpoint.url", token_endpoint);
    
    char scope[256];
    snprintf(scope, sizeof(scope), "https://%s.servicebus.windows.net/.default", eh_name);
    set_config(conf, "sasl.oauthbearer.scope", scope);
    //set_config(conf, "sasl.oauthbearer.extensions",         "logicalCluster=<LOGICAL CLUSTER ID>,identityPoolId=<IDENTITY POOL ID>");


    char bootstrap_servers[256];
    snprintf(bootstrap_servers, sizeof(bootstrap_servers), "%s.servicebus.windows.net:9093", eh_name);
    set_config(conf, "bootstrap.servers", bootstrap_servers);

    set_config(conf, "security.protocol", "SASL_SSL");
    set_config(conf, "sasl.mechanism", "OAUTHBEARER");
    set_config(conf, "acks",                    "all");

    // Fixed properties
    set_config(conf, "group.id",          "kafka-c-getting-started");
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
    const char *topic = "hub1";
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