# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Asterisk CDR (Call Detail Record) backend module that publishes call records as JSON messages to Apache Kafka topics, with configurable message headers and partitioning keys. Written in C as an Asterisk shared-object module (`cdr_kafka.so`).

## Build Commands

```bash
make                  # Build cdr_kafka.so
make test             # Build test_cdr_kafka.so
make install          # Install module to /usr/lib64/asterisk/modules/ and XML docs
make install-test     # Install test module
make samples          # Install sample config to /etc/asterisk/cdr_kafka.conf
make clean            # Remove .o and .so files
```

The build requires `../vsgroup-res_kafka` to exist as a sibling directory (for the `asterisk/kafka.h` header). Asterisk 18+ development headers must be installed.

## Running Tests

Tests use Asterisk's built-in `AST_TEST_DEFINE` framework (not standalone—they run inside a live Asterisk instance):

```bash
make install-test
asterisk -rx "module load test_cdr_kafka.so"
asterisk -rx "test execute category /cdr/kafka/"
```

## Architecture

**Single-file module**: `cdr_kafka.c` (~520 lines) contains all production code. `test_cdr_kafka.c` (~520 lines) contains 12 unit tests.

**Key patterns used**:
- **AO2 (Asterisk Objects 2)**: Thread-safe reference-counted containers for global config (`confs`) and cached producer (`cached_producer`)
- **ACO (Asterisk Config Objects)**: Declarative configuration framework that maps `cdr_kafka.conf` options to `struct cdr_kafka_global_conf` fields
- **Cached producer**: The Kafka producer is fetched once and cached in an AO2 global object to avoid mutex contention on every CDR write

**Data flow**: CDR event → `kafka_cdr_log()` → build JSON + 5 Kafka headers → `ast_kafka_produce_hdrs()` via res_kafka → librdkafka

**Configuration options** (in `/etc/asterisk/cdr_kafka.conf`): `connection`, `topic`, `key`, `loguniqueid`, `loguserfield`

**Supported CDR key fields** (for Kafka partitioning): linkedid, uniqueid, channel, dstchannel, accountcode, src, dst, dcontext, tenantid — matched case-insensitively in `cdr_get_key_value()`.

## Dependencies

- **Asterisk 18+** — core headers (cdr.h, config_options.h, json.h, module.h, etc.)
- **vsgroup-res_kafka** — sibling project providing Kafka producer/consumer abstraction over librdkafka; must be at `../vsgroup-res_kafka`
- Runtime load order: `res_kafka.so` must load before `cdr_kafka.so`

## Compiler Flags

The Makefile enforces strict warnings: `-Wall -Wextra -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wformat=2`. All functions need proper prototypes and forward declarations.
