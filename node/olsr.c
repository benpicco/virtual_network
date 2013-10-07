#include <stdlib.h>
#include <time.h>

#include "common/netaddr.h"

#include "olsr.h"
#include "util.h"
#include "debug.h"
#include "routing.h"

struct free_node* free_nodes_head = 0;

int olsr_node_cmp(struct olsr_node* a, struct olsr_node* b) {
	return netaddr_cmp(a->addr, b->addr);
}

void add_olsr_node(struct netaddr* addr, struct netaddr* last_addr, uint8_t vtime, uint8_t distance, char* name) {
	struct olsr_node* n = get_node(addr);

	if (!n) {
		DEBUG("new olsr node: %s, last hop: %s - distance: %d", 
			netaddr_to_string(&nbuf[0], addr),
			netaddr_to_string(&nbuf[1], last_addr),
			distance);
		n = calloc(1, sizeof(struct olsr_node));
		n->addr = netaddr_dup(addr);
		n->last_addr = netaddr_reuse(last_addr);
		n->expires = time(0) + vtime;
		n->distance = distance;
#ifdef ENABLE_DEBUG
		n->name = name;
#endif
		n->node.key = n->addr;
		avl_insert(&olsr_head, &n->node);
		add_free_node(&free_nodes_head, n);

		return;
	}

	/* diverging from the spec to save a little space (spec says keep all paths) */
	// TODO: change this when handling timeouts
	if (n->distance < distance) {
		DEBUG("discarding longer (%d > %d) route for %s (%s) via %s",
			distance, n->distance,
			n->name, netaddr_to_string(&nbuf[0], n->addr),
			netaddr_to_string(&nbuf[1], last_addr));
		return;
	}

	DEBUG("updating TC entry for %s (%s)", n->name, netaddr_to_string(&nbuf[0], n->addr));

	/* we found a shorter route */
	if (netaddr_cmp(last_addr, n->last_addr) != 0) {
		DEBUG("shorter route found");
		n->last_addr = netaddr_reuse(last_addr);
		add_free_node(&free_nodes_head, n);
		// TODO: also add all nodes routing through this one
	}

	n->expires = time(0) + vtime;
}

bool is_known_msg(struct netaddr* addr, uint16_t seq_no) {
	struct olsr_node* node = get_node(addr);
	if (!node)
		return false;

	uint16_t tmp = node->seq_no;
	node->seq_no = seq_no;
	if (seq_no > tmp || (seq_no < 100 && tmp > 65400))
		return false;

	return true;
}

void olsr_update() {
	DEBUG("update routing table (%s pending nodes)", free_nodes_head ? "some" : "no");
	fill_routing_table(&free_nodes_head);
}

#ifdef ENABLE_DEBUG
void print_topology_set() {
	DEBUG();
	DEBUG("---[ Topology Set ]--");

	struct olsr_node* node;
	avl_for_each_element(&olsr_head, node, node) {
		DEBUG("%s (%s) => %s; %d hops, next: %s, %zd s [%d]",
			netaddr_to_string(&nbuf[0], node->addr),
			node->name,
			netaddr_to_string(&nbuf[1], node->last_addr),
			node->distance,
			netaddr_to_string(&nbuf[2], node->next_addr),
			node->expires - time(0),
			node->seq_no
			);
	}
	DEBUG("---------------------");
	DEBUG();
}
#else
void print_topology_set() {}
#endif