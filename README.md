# Azure Event Hub Kafka C/C++ Example

This project demonstrates how to produce and consume messages to and from Azure Event Hub using the Kafka protocol in C/C++. It provides example producer and consumer applications that connect to Azure Event Hub, supporting both Azure Managed Identity and Service Principal authentication. The project is designed for learning, prototyping, and as a starting point for integrating Azure Event Hub with C/C++ applications using Kafka-compatible clients.

## Features
- Example C/C++ producer and consumer for Azure Event Hub via Kafka protocol
- Supports authentication via Azure Managed Identity or Service Principal
- Ready-to-use Makefile for building both producer and consumer
- Docker and Devcontainer support for easy development and testing
- Instructions for static and dynamic linking
- Troubleshooting and environment setup guidance

## Build

```bash
make producer
make consumer
```

## Usage

### Producer

#### Using Managed Identity (requires RBAC on Event Hub)
```bash
rm producer; make producer
EH_NAME=htest TOPIC=hub1 ./producer
```

#### Using Service Principal (requires RBAC on Event Hub)
```bash
CLIENT_ID=xxx
CLIENT_SECRET=xxx
TOKEN_ENDPOINT=https://login.microsoftonline.com/xxx/oauth2/v2.0/token
EH_NAME=htest TOPIC=hub1 ./producer
```

### Consumer

#### Using Managed Identity
```bash
make consumer
export EH_NAME=<your-event-hub-namespace>
export TOPIC=<your-event-hub-name>
./consumer
```

#### Using Service Principal
```bash
export CLIENT_ID=<your-client-id>
export CLIENT_SECRET=<your-client-secret>
export TOKEN_ENDPOINT=https://login.microsoftonline.com/<tenant-id>/oauth2/v2.0/token
export EH_NAME=<your-event-hub-namespace>
export TOPIC=<your-event-hub-name>
./consumer
```

## Environment Variables

- **EH_NAME**: Event Hub namespace name (e.g. `htest`).
- **TOPIC**: Event Hub name (topic/partition).
- **CLIENT_ID**: (Service Principal only) Azure AD Application (Client) ID.
- **CLIENT_SECRET**: (Service Principal only) Azure AD Client Secret.
- **TOKEN_ENDPOINT**: (Service Principal only) OAuth2 token endpoint URL.
- **AZURE_LOG_LEVEL**: (Optional) Set to `debug` for verbose Azure SDK logs.

## Prerequisites

- C/C++ compiler (`gcc`, `g++`)
- Build tools: `make`, `cmake`
- Azure C/C++ SDK dependencies (via `vcpkg` or manual install):
  - `azure-messaging-eventhubs-kafka`
  - `azure-identity`
- `librdkafka` (from `vcpkg` or system package)

## Dependencies Installation (using vcpkg)

```bash
vcpkg install azure-messaging-eventhubs-kafka azure-identity
```
Ensure `vcpkg` is integrated in your environment (e.g., via `export VCPKG_ROOT=~/vcpkg`).

## Docker / Devcontainer Usage

Build and run producer image:

```bash
docker build -t eh_kafka_producer:0.0.0 -f .devcontainer/Dockerfile.producer .
docker run --network=host eh_kafka_producer:0.0.0
```
To add a consumer image, create a similar `Dockerfile.consumer` and repeat.

## Static Linking

When building with static libraries installed under `/usr/local/lib`:

```bash
LD_LIBRARY_PATH=/usr/local/lib ./producer
```
Similar approach applies for `consumer` binary.

## Troubleshooting

- Verify Azure RBAC permissions on the Event Hub namespace and hub.
- Confirm network connectivity to Azure Event Hubs endpoint.
- Enable debug logs: `export AZURE_LOG_LEVEL=debug`.
- Use `valgrind` or `cppcheck` for runtime memory and static analysis.

## References
- [Azure Event Hubs for Apache Kafka](https://learn.microsoft.com/en-us/azure/event-hubs/event-hubs-for-kafka-ecosystem-overview)
- [librdkafka documentation](https://github.com/edenhill/librdkafka)
- [Azure SDK for C++](https://github.com/Azure/azure-sdk-for-cpp)

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.