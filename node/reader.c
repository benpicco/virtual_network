/*
 * The olsr.org Optimized Link-State Routing daemon version 2 (olsrd2)
 * Copyright (c) 2004-2013, the olsr.org team - see HISTORY file
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 * * Neither the name of olsr.org, olsrd nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Visit http://www.olsr.org for more information.
 *
 * If you find this software useful feel free to make a donation
 * to the project. For more information see the website or contact
 * the copyright holders.
 *
 */

#include <string.h>
#include <stdio.h>

#include "common/common_types.h"
#include "common/netaddr.h"
#include "rfc5444/rfc5444_reader.h"

#ifdef RIOT
#include "net_help/net_help.h"
#include "compat_misc.h"
#endif

#include "reader.h"

static enum rfc5444_result _cb_blocktlv_packet_okay(
    struct rfc5444_reader_tlvblock_context *cont);

static enum rfc5444_result _cb_blocktlv_address_okay(
    struct rfc5444_reader_tlvblock_context *cont);

/*
 * message consumer entries definition
 * TLV type 0
 * TLV type 1 (mandatory)
 */
static struct rfc5444_reader_tlvblock_consumer_entry _consumer_entries[] = {
  { .type = 0 },
  { .type = 1, .mandatory = true }
};

static struct rfc5444_reader_tlvblock_consumer_entry _consumer_address_entries[] = {
  { .type = 0},
};

static struct rfc5444_reader_tlvblock_consumer _consumer = {
  /* parse message type 1 */
  .msg_id = 1,

  /* use a block callback */
  .block_callback = _cb_blocktlv_packet_okay,
};

static struct rfc5444_reader_tlvblock_consumer _address_consumer = {
  .msg_id = 1,
  .addrblock_consumer = true,
  .block_callback = _cb_blocktlv_address_okay,
};

struct rfc5444_reader reader;

/**
 * This block callback is only called if message tlv type 1 is present,
 * because it was declared as mandatory
 *
 * @param cont
 * @return
 */
static enum rfc5444_result
_cb_blocktlv_packet_okay(struct rfc5444_reader_tlvblock_context *cont) {
  int value;
  struct rfc5444_reader_tlvblock_entry* tlv;
  struct netaddr_str nbuf;

  printf("addr: %s\n", netaddr_to_string(&nbuf, &cont->addr));

  printf("%s()\n", __func__);

  printf("\tmessage type: %d\n", cont->type);

  if (cont->has_origaddr) {
    printf("\torig_addr: %s\n", netaddr_to_string(&nbuf, &cont->orig_addr));
  }

  if (cont->has_seqno) {
    printf("\tseqno: %d\n", cont->seqno);
  }

  /* tlv type 0 was not defined mandatory in block callback entries */
  if (_consumer_entries[0].tlv) {
    /* values of TLVs are not aligned well in memory, so we have to copy them */
    memcpy(&value, _consumer_entries[0].tlv->single_value, sizeof(value));
    printf("\ttlv 0: %d\n", ntohl(value));
  }

  /* tlv type 1 was defined mandatory in block callback entries */
  /* values of TLVs are not aligned well in memory, so we have to copy them */
  tlv = _consumer_entries[1].tlv;
  do {
    memcpy(&value, tlv->single_value, sizeof(value));
    printf("\ttlv 1: %d\n", ntohl(value));
  } while ((tlv = tlv->next_entry));

  return RFC5444_OKAY;
}

static enum rfc5444_result
_cb_blocktlv_address_okay(struct rfc5444_reader_tlvblock_context *cont) {
  struct netaddr_str nbuf;
  struct rfc5444_reader_tlvblock_entry* tlv;
  int value;

  printf("_cb_blocktlv_address_okay()\n");
  printf("addr: %s\n", netaddr_to_string(&nbuf, &cont->addr));

  tlv = _consumer_address_entries[0].tlv;
  while (tlv) {
    memcpy(&value, tlv->single_value, sizeof(value));
    printf("\ttlv 0: %d\n", ntohl(value));
    tlv = tlv->next_entry;
  }

  return RFC5444_OKAY;
}

/**
 * Initialize RFC5444 reader
 */
void
reader_init(void) {
  printf("%s()\n", __func__);

  /* initialize reader */
  rfc5444_reader_init(&reader);

  /* register message consumer */
  rfc5444_reader_add_message_consumer(&reader, &_consumer,
      _consumer_entries, ARRAYSIZE(_consumer_entries));

  rfc5444_reader_add_message_consumer(&reader, &_address_consumer,
      _consumer_address_entries, ARRAYSIZE(_consumer_address_entries));
}

/**
 * Inject a package into the RFC5444 reader
 */
int
reader_handle_packet(void* buffer, size_t length) {
  return rfc5444_reader_handle_packet(&reader, buffer, length);
}

/**
 * Cleanup RFC5444 reader
 */
void
reader_cleanup(void) {
  printf("%s()\n", __func__);

  rfc5444_reader_cleanup(&reader);
}
