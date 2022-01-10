
#define REFC_H_IMPLEMENTATION
#define REFC_H_DEBUG
#include "refc.h"

#include <assert.h>

int dtor_called = 0;

void dtor(void *ref) {
	dtor_called = 1;
}

int main() {
	struct refc_ref *ref = refc_allocate_dtor(512, &dtor);
	assert(ref != NULL);

	void *block = refc_access(ref);
	assert(block != NULL);

	refc_release(ref);

	assert(dtor_called == 1);


	struct refc_ref *parent = refc_allocate(512);
	struct refc_ref *child = refc_allocate(512);
	assert(refc_link(parent, child));
	assert(refc_link(child, parent) == 0);

	assert(refc_unlink(parent, child));
	assert(refc_unlink(parent, child) == 0);

	refc_release(child);
	refc_release(parent);

	struct refc_ref *nodes[5];
	for (size_t i = 0; i < 5; i++) {
		nodes[i] = refc_allocate(512);
	}
	assert(refc_link(nodes[0], nodes[1]));
	assert(refc_link(nodes[0], nodes[2]));
	assert(refc_link(nodes[1], nodes[2]));
	assert(refc_link(nodes[2], nodes[3]));
	assert(refc_link(nodes[2], nodes[4]));
	assert(refc_link(nodes[4], nodes[1]) == 0);

	for (size_t i = 0; i < 5; i++) {
		refc_release(nodes[i]);
	}
}
