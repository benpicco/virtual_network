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

#ifdef DEBUG
#include <string.h>
#endif

#ifdef RIOT
#include "net_help/net_help.h"
#endif

#include "common/common_types.h"
#include "common/netaddr.h"
#include "rfc5444/rfc5444.h"
#include "rfc5444/rfc5444_iana.h"
#include "rfc5444/rfc5444_writer.h"

#include "olsr2_writer.h"
#include "olsr2.h"
#include "constants.h"

static void _cb_addMessageTLVs(struct rfc5444_writer *wr);
static void _cb_addAddresses(struct rfc5444_writer *wr);

static struct rfc5444_writer_content_provider _message_content_provider = {
	.msg_type = RFC5444_MSGTYPE_HELLO,
	.addMessageTLVs = _cb_addMessageTLVs,
	.addAddresses = _cb_addAddresses,
};

static struct rfc5444_writer_tlvtype _olsrv2_addrtlvs[] = {
  [IDX_ADDRTLV_NBR_ADDR_TYPE] =  { .type = RFC5444_ADDRTLV_NBR_ADDR_TYPE },
  [IDX_ADDRTLV_GATEWAY]       =  { .type = RFC5444_ADDRTLV_GATEWAY },
#ifdef DEBUG
  [IDX_ADDRTLV_NODE_NAME] = { .type = RFC5444_TLV_NODE_NAME },
#endif
};

struct rfc5444_writer writer;
struct rfc5444_writer_target interface;

/**
 * Callback to add message TLVs to a RFC5444 message
 * @param wr
 */
static void
_cb_addMessageTLVs(struct rfc5444_writer *wr) {
	uint8_t time_encoded;

	time_encoded = rfc5444_timetlv_encode(REFRESH_INTERVAL);
  	rfc5444_writer_add_messagetlv(wr, RFC5444_MSGTLV_VALIDITY_TIME, 0,
    	&time_encoded, sizeof(time_encoded));

  	time_encoded = rfc5444_timetlv_encode(HOLD_TIME);
  	rfc5444_writer_add_messagetlv(wr, RFC5444_MSGTLV_INTERVAL_TIME, 0,
     	&time_encoded, sizeof(time_encoded));
#ifdef DEBUG
    rfc5444_writer_add_messagetlv(wr, RFC5444_TLV_NODE_NAME, 0, node_name, strlen(node_name));
#endif
}

static void
_cb_addAddresses(struct rfc5444_writer *wr) {
	struct nhdp_node* selector;

  /* add all neighbors */
	get_next_routing_mpr_selector_reset();
	while ((selector = get_next_routing_mpr_selector())) {
		struct rfc5444_writer_address *address = rfc5444_writer_add_address(wr, _message_content_provider.creator, selector->addr, false);
#ifdef DEBUG
    if (selector->name)
      rfc5444_writer_add_addrtlv(wr, address, &_olsrv2_addrtlvs[IDX_ADDRTLV_NODE_NAME], selector->name, strlen(selector->name), false);
#endif    
	}
}

/**
 * Callback to define the message header for a RFC5444 message
 * @param wr
 * @param message
 */
static void
_cb_addMessageHeader(struct rfc5444_writer *wr, struct rfc5444_writer_message *message) {
	/* originator, not hopcount, no hoplimit, sequence number */
	rfc5444_writer_set_msg_header(wr, message, true, false, false, true);
	rfc5444_writer_set_msg_originator(wr, message, netaddr_get_binptr(&local_addr));
}

/**
 * Initialize RFC5444 writer
 * @param ptr pointer to "send_packet" function
 */
void
olsr_writer_init(write_packet_func_ptr ptr) {
  struct rfc5444_writer_message *_msg;

  writer.msg_buffer = msg_buffer;
  writer.msg_size   = sizeof(msg_buffer);
  writer.addrtlv_buffer = msg_addrtlvs;
  writer.addrtlv_size   = sizeof(msg_addrtlvs);

  interface.packet_buffer = packet_buffer;
  interface.packet_size   = sizeof(packet_buffer);
  interface.sendPacket = ptr;

  /* initialize writer */
  rfc5444_writer_init(&writer);

  /* register a target (for sending messages to) in writer */
  rfc5444_writer_register_target(&writer, &interface);

  /* register a message content provider */
  rfc5444_writer_register_msgcontentprovider(&writer, &_message_content_provider, _olsrv2_addrtlvs, ARRAYSIZE(_olsrv2_addrtlvs));

  /* register message type 1 with 16 byte addresses */
  _msg = rfc5444_writer_register_message(&writer, RFC5444_MSGTYPE_TC, false, 16);
  _msg->addMessageHeader = _cb_addMessageHeader;
}

void olsr_writer_tick(void) {
	printf("[olsr_writer_tick]\n");

	/* send message */
	rfc5444_writer_create_message_alltarget(&writer, RFC5444_MSGTYPE_TC);
	rfc5444_writer_flush(&writer, &interface, false);
}

/**
 * Cleanup RFC5444 writer
 */
void
olsr_writer_cleanup(void) {
  rfc5444_writer_cleanup(&writer);
}
