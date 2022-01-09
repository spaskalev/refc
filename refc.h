/*
 * Copyright 2022 Stanislav Paskalev <spaskalev@protonmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software
 * for any purpose with or without fee is hereby granted.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 *
 * This library provided reference counting allocation functions
 * with the option to call a destructor function when a object is
 * about to be freed. The semantics for this library are similar
 * to Objective-C's NSObject.
 *
 * 1. You own any allocation that you create using `refc_allocate`.
 * 2. If you get a `struct refc_ref *` as a parameter you do not own it.
 * 3. If you wish to save it you must own it. To own it call `refc_retain`.
 * 4. If you own it you must call `refc_release` when done with it.
 *
 * This library uses a opaque pointer with a helper accessor function
 * to avoid mixing pointers that are obtained from the library with
 * pointers that are obtained otherwise.
 */

#ifndef REFC_H
#define REFC_H

#include <stddef.h>

/* Incomplete type for opaque pointer purpose */
struct refc_ref;

/* Allocate a reference-counted block with a reference count of 1. */
struct refc_ref *refc_allocate(size_t size);

/*
 * Allocate a reference-counted block with a reference count of 1
 * and associate a destructor function. The destructor will be called
 * upon reaching a reference count of 0, before the block is freed.
 */
struct refc_ref *refc_allocate_dtor(size_t size, void (*destructor)(void *));

/* Increase the reference count of the target block by one. */
void refc_retain(struct refc_ref *ref);

/*
 * Decrement the reference count of the target block by one.
 *
 * Upon reaching a reference count of 0 an associated destructor
 * is called (if any) after which the block is freed.
 */
void refc_release(struct refc_ref *ref);

/* Returns the target block of this reference */
void *refc_access(struct refc_ref *ref);

#endif /* REFC_H */

#ifdef REFC_H_IMPLEMENTATION
#undef REFC_H_IMPLEMENTATION

#include <stdatomic.h>
#include <stddef.h>
#include <stdlib.h>

struct refc_ref {
	/* The reference count for this reference */
	atomic_size_t reference_count;

	/* Associated destructor function. Can be NULL */
	void (*destructor)(void *);

	_Alignas(max_align_t) unsigned char block[];
};

struct refc_ref *refc_allocate(size_t size) {
	return refc_allocate_dtor(size, NULL);
}

struct refc_ref *refc_allocate_dtor(size_t size, void (*destructor)(void *)) {
	struct refc_ref *ref = malloc(sizeof(struct refc_ref) + size);
	if (ref == NULL) {
		return NULL;
	}
	ref->reference_count = 1;
	ref->destructor = destructor;
	return ref;
}

void refc_retain(struct refc_ref *ref) {
	atomic_fetch_add(&ref->reference_count, 1);
}

void refc_release(struct refc_ref *ref) {
	atomic_fetch_sub(&ref->reference_count, 1);
	if (atomic_load(&ref->reference_count) > 0) {
		return;
	}
	if (ref->destructor != NULL) {
		(ref->destructor)(&ref->block);
	}
	free(ref);
}

void *refc_access(struct refc_ref *ref) {
	return &ref->block;
}

#endif /* REFC_H_IMPLEMENTATION */
