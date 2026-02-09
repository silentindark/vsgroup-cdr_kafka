#
# Makefile for Asterisk CDR Kafka module
# Copyright (C) 2024, Wazo Communication Inc.
#
# This program is free software, distributed under the terms of
# the GNU General Public License Version 2. See the LICENSE file
# at the top of the source tree.
#

ASTLIBDIR:=$(shell awk '/moddir/{print $$3}' /etc/asterisk/asterisk.conf 2> /dev/null)
ifeq ($(strip $(ASTLIBDIR)),)
	MODULES_DIR:=$(INSTALL_PREFIX)/usr/lib64/asterisk/modules
else
	MODULES_DIR:=$(INSTALL_PREFIX)$(ASTLIBDIR)
endif
ifeq ($(strip $(DOCDIR)),)
	DOCUMENTATION_DIR:=$(INSTALL_PREFIX)/usr/share/asterisk/documentation/thirdparty
else
	DOCUMENTATION_DIR:=$(INSTALL_PREFIX)$(DOCDIR)
endif
INSTALL = install
ASTETCDIR = $(INSTALL_PREFIX)/etc/asterisk
SAMPLENAME = cdr_kafka.conf.sample
CONFNAME = $(basename $(SAMPLENAME))

TARGET = cdr_kafka.so
OBJECTS = cdr_kafka.o
TEST_TARGET = test_cdr_kafka.so
TEST_OBJECTS = test_cdr_kafka.o
CFLAGS += -I../vsgroup-res_kafka
CFLAGS += -DHAVE_STDINT_H=1
CFLAGS += -Wall -Wextra -Wno-unused-parameter -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Winit-self -Wmissing-format-attribute \
          -Wformat=2 -g -fPIC -D_GNU_SOURCE -D'AST_MODULE="cdr_kafka"' -D'AST_MODULE_SELF_SYM=__internal_cdr_kafka_self'
LDFLAGS = -Wall -shared

.PHONY: install install-test test clean

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@ $(LIBS)

test_cdr_kafka.o: test_cdr_kafka.c
	$(CC) -c $(CFLAGS) -D'AST_MODULE="test_cdr_kafka"' \
	    -D'AST_MODULE_SELF_SYM=__internal_test_cdr_kafka_self' -o $@ $<

$(TEST_TARGET): $(TEST_OBJECTS)
	$(CC) $(LDFLAGS) $(TEST_OBJECTS) -o $@ $(LIBS)

test: $(TEST_TARGET)

%.o: %.c $(HEADERS)
	$(CC) -c $(CFLAGS) -o $@ $<

install: $(TARGET)
	mkdir -p $(DESTDIR)$(MODULES_DIR)
	mkdir -p $(DESTDIR)$(DOCUMENTATION_DIR)
	install -m 644 $(TARGET) $(DESTDIR)$(MODULES_DIR)
	install -m 644 documentation/* $(DESTDIR)$(DOCUMENTATION_DIR)
	@echo " +-------- cdr_kafka installed ---------+"
	@echo " +                                             +"
	@echo " + cdr_kafka has successfully been installed   +"
	@echo " + If you would like to install the sample     +"
	@echo " + configuration file run:                     +"
	@echo " +                                             +"
	@echo " +              make samples                   +"
	@echo " +---------------------------------------------+"

install-test: $(TEST_TARGET)
	mkdir -p $(DESTDIR)$(MODULES_DIR)
	install -m 644 $(TEST_TARGET) $(DESTDIR)$(MODULES_DIR)

clean:
	rm -f $(OBJECTS) $(TEST_OBJECTS)
	rm -f $(TARGET) $(TEST_TARGET)

samples:
	$(INSTALL) -m 644 $(SAMPLENAME) $(DESTDIR)$(ASTETCDIR)/$(CONFNAME)
	@echo " ------- cdr_kafka config installed ---------"
