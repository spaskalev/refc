
#define REFC_H_IMPLEMENTATION
#include "refc.h"

#include <assert.h>

int dtor_called = 0;

void dtor(void *ref) {
	dtor_called = 1;
}

int main() {
	struct refc_ref *ref = refc_allocate_dtor(512, &dtor);
	assert(ref != NULL);

	void *block = refc_lock(ref);
	assert(block != NULL);

	refc_unlock(ref);
	refc_release(ref);

	assert(dtor_called == 1);
}
