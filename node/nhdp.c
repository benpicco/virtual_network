#include <stdlib.h>
#include <stdio.h>

#include "list.h"
#include "nhdp.h"

struct nhdp_node_2_hop {
	struct nhdp_node_2_hop* next;

	struct nhdp_node* mpr;

	struct netaddr* addr;
	uint8_t linkstatus;
};

struct nhdp_node* n_head = 0;
struct nhdp_node* get_nn_head = 0;

struct nhdp_node_2_hop* n2_head = 0;

struct netaddr* _netaddr_cpy (struct netaddr* addr) {
	struct netaddr* addr_new = malloc(sizeof(struct netaddr));
	return memcpy(addr_new, addr, sizeof(struct netaddr));
}

struct nhdp_node* add_neighbor(struct netaddr* addr, uint8_t linkstatus) {
	struct nhdp_node* new_n = list_find_memcmp(n_head, addr);

	if (!new_n) {
		new_n = list_add_head(&n_head);
		new_n->addr = _netaddr_cpy(addr);
		new_n->linkstatus = linkstatus;
		new_n->mpr_neigh = 0;
	}

	return new_n;
}

int add_2_hop_neighbor(struct nhdp_node* node, struct netaddr* addr, uint8_t linkstatus) {
	struct nhdp_node_2_hop* n2_node;

	if (!memcmp(addr, &local_addr, sizeof local_addr))
		return -ADD_2_HOP_IS_LOCAL;

	if(list_find_memcmp(n_head, addr))
		return -ADD_2HOP_IS_NEIGHBOR;

	if ((n2_node = list_find_memcmp(n2_head, addr))) {
		if (n2_node->mpr == node || n2_node->mpr->mpr_neigh > node->mpr_neigh + 1)
			return -ADD_2_HOP_OK;

		printf("switching MPR\n");
		n2_node->mpr->mpr_neigh--;
		n2_node->mpr = node;
		node->mpr_neigh++;
		return -ADD_2_HOP_OK;
	}

	n2_node = list_add_head(&n2_head);
	n2_node->mpr = node;
	n2_node->addr = _netaddr_cpy(addr);
	n2_node->linkstatus = linkstatus;

	node->mpr_neigh++;

	return ADD_2_HOP_OK;
}

void remove_neighbor(struct nhdp_node* node) {
	// todo: update mpr
	if (node)
		list_remove(&n_head, node);
}

void get_next_neighbor_reset(void) {
	get_nn_head = n_head;
}

struct nhdp_node* get_next_neighbor(void) {
	struct nhdp_node* ret = get_nn_head;

	if (get_nn_head) {
		get_nn_head = get_nn_head->next;
	}
	
	return ret;
}

void print_neighbors(void) {
	struct netaddr_str nbuf;
	struct netaddr_str nbuf2;

	struct nhdp_node* node;
	get_next_neighbor_reset();
	while ((node = get_next_neighbor())) {
		printf("neighbor: %s (mpr for %d nodes)\n", netaddr_to_string(&nbuf, node->addr), node->mpr_neigh);
	}

	struct nhdp_node_2_hop* n2 = n2_head;
	while((n2)) {
		printf("\t%s -> %s -> (O)\n", netaddr_to_string(&nbuf2, n2->addr), netaddr_to_string(&nbuf, n2->mpr->addr));
		n2 = n2->next;
	}
}