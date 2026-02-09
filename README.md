# cdr_kafka

Asterisk CDR (Call Detail Record) backend that publishes call records as JSON to Apache Kafka via `res_kafka`.

## Overview

Every completed call in Asterisk generates a CDR. This module registers as a CDR backend and publishes each record as a JSON message to a configurable Kafka topic, replacing traditional file or database CDR storage with a real-time event stream.

Each CDR is enriched with system identification fields (`EntityID` and `SystemName`) so that consumers can distinguish which Asterisk instance originated the record — essential in multi-server environments.

### JSON Output

```json
{
  "clid": "\"Alice\" <100>",
  "src": "100",
  "dst": "200",
  "dcontext": "default",
  "channel": "PJSIP/100-00000001",
  "dstchannel": "PJSIP/200-00000002",
  "lastapp": "Dial",
  "lastdata": "PJSIP/200,30",
  "start": "2026-01-15T10:30:00.000-0300",
  "answer": "2026-01-15T10:30:05.000-0300",
  "end": "2026-01-15T10:32:15.000-0300",
  "durationsec": 135,
  "billsec": 130,
  "disposition": "ANSWERED",
  "accountcode": "",
  "amaflags": "DOCUMENTATION",
  "peeraccount": "",
  "linkedid": "1705312200.1",
  "sequence": 0,
  "EntityID": "00:11:22:33:44:55",
  "SystemName": "pbx-01"
}
```

`EntityID` is always present (auto-detected from the network interface MAC address, or set via `entityid` in `asterisk.conf`). `SystemName` is only included when `systemname` is configured in `asterisk.conf`.

Optional fields `uniqueid` and `userfield` can be enabled in the configuration. CDR variables set via `func_cdr` are also included automatically.

## Prerequisites

- Asterisk 18+
- [vsgroup-res_kafka](https://github.com/vsgroup/vsgroup-res_kafka) (`res_kafka.so`) loaded

## Building

Ensure `vsgroup-res_kafka` is checked out alongside this project:

```
parent/
├── vsgroup-res_kafka/
└── vsgroup-cdr_kafka/
```

```bash
make
```

## Installation

```bash
make install
make samples
```

## Configuration

Edit `/etc/asterisk/cdr_kafka.conf`:

```ini
[global]
connection = my-kafka          ; Connection name from kafka.conf (required)
topic = asterisk_cdr           ; Kafka topic (default: asterisk_cdr)
key = linkedid                 ; CDR field for Kafka key (default: empty/none)
loguniqueid = no               ; Include call uniqueid (default: no)
loguserfield = no              ; Include user field (default: no)
```

### Configuration Options

| Option | Default | Description |
|--------|---------|-------------|
| `connection` | *(empty)* | Name of the connection defined in `kafka.conf` for `res_kafka`. Required. |
| `topic` | `asterisk_cdr` | Kafka topic to publish CDR records to. |
| `key` | *(empty)* | CDR field to use as Kafka message key for partitioning. Valid values: `linkedid`, `uniqueid`, `channel`, `dstchannel`, `accountcode`, `src`, `dst`, `dcontext`, `tenantid`. Empty means no key. |
| `loguniqueid` | `no` | When `yes`, adds the `uniqueid` field to the JSON output. |
| `loguserfield` | `no` | When `yes`, adds the `userfield` field to the JSON output. |

## Loading

```
asterisk -rx "module load res_kafka.so"
asterisk -rx "module load cdr_kafka.so"
```

## Verifying

```bash
kafka-console-consumer --bootstrap-server localhost:9092 --topic asterisk_cdr
```

Make a test call and verify that a JSON CDR record appears in the consumer output after hangup.

## Architecture

The module registers itself as a CDR backend via `ast_cdr_register()`. When Asterisk finalizes a CDR, it calls `kafka_cdr_log()` which:

1. Packs all CDR fields into an `ast_json` object
2. Injects `EntityID` and `SystemName` for server identification
3. Appends any CDR variables from `func_cdr`
4. Optionally adds `uniqueid` and `userfield`
5. Serializes to a JSON string
6. Calls `ast_kafka_produce()` to enqueue in librdkafka's internal buffer (non-blocking)

A cached producer (`AO2_GLOBAL_OBJ_STATIC`) avoids mutex contention on every CDR write.

## Project Structure

```
vsgroup-cdr_kafka/
├── cdr_kafka.c                Main module (CDR handler + ACO config)
├── asterisk/
│   └── kafka.h                Public API header from res_kafka
├── documentation/
│   └── cdr_kafka_config-en_US.xml
├── cdr_kafka.conf.sample      Sample configuration
├── Makefile
├── LICENSE                    GPLv2
└── AUTHORS
```

## License

GNU General Public License Version 2. See [LICENSE](LICENSE).
