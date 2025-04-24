ALL: producer consumer

CFLAGS=-Wall $(shell pkg-config --cflags glib-2.0 rdkafka)
LDLIBS=$(shell pkg-config --libs glib-2.0 rdkafka) -lcurl -lssl

producer: producer.c man_id.c
	$(CC) $(CFLAGS) -o producer producer.c man_id.c $(LDLIBS)

consumer: consumer.c man_id.c
	$(CC) $(CFLAGS) -o consumer consumer.c man_id.c $(LDLIBS)


# Static
#LDLIBS=$(shell pkg-config --libs glib-2.0)  /usr/lib64/librdkafka.a -lcurl -lpthread -ldl -lz -lssl -lcrypto -lsasl2 -lzstd -lm


#ALL: producer consumer
#
#CFLAGS=-Wall $(shell pkg-config --cflags glib-2.0)
#LDLIBS=/usr/lib64/librdkafka.a $(shell pkg-config --libs --static glib-2.0) -lcurl -lpthread -ldl -lz -lssl -lcrypto -lsasl2 -lzstd -lm -llz4

#producer: producer.c common.c
#	$(CC) $(CFLAGS) -static -o producer producer.c common.c $(LDLIBS)
#
#consumer: consumer.c common.c
#	$(CC) $(CFLAGS) -static -o consumer consumer.c common.c $(LDLIBS)