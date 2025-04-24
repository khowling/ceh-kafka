
# Example of sending and consuming to Azure EventHub with Kafka library

# Build

```
make producer
make consumer
```

##  Devcontainers
docker build -t eh_kafka_producer:0.0.0 -f .devcontainer/Dockerfile.producer .
docker run eh_kafka_producer:0.0.0 

# Run 
## Using Managed Identity  (requires RBAC on Eventhub Hub)
rm producer; make producer
EH_NAME=htest TOPIC=hub1  ./producer

## Using Service Principal (requires RBAC on Eventhub Hub)
CLIENT_ID=xxx
CLIENT_SECRET=xxx
TOKEN_ENDPOINT=https://login.microsoftonline.com/xxx/oauth2/v2.0/token 
EH_NAME=htest  TOPIC=hub1 ./producer


# Static linking
LD_LIBRARY_PATH=/usr/local/lib ./producer