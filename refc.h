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

/*
 * Links two references into a parent-child relation.
 * Used to track reference cycles when REFC_H_DEBUG is defined.
 * Does nothing when REFC_H_DEBUG is not defined.
 *
 * Returns 1 if the link is successful.
 * Returns 0 if a cycle is found or the link cannot be established.
 * This makes it suitable to be used in an assert macro.
 *
 * When REFC_H_DEBUG is not defined refc_link is a macro for (1).
 */
#ifdef REFC_H_DEBUG
int refc_link(struct refc_ref *parent, struct refc_ref *child);
#else
#define refc_link(P, C) (1)
#endif

/*
 * Unlinks two references from a parent-child relation.
 * Used to track reference cycles when REFC_H_DEBUG is defined.
 *
 * Returns 1 when the unlinking is successful.
 * Returns 0 when there is no such link.
 *
 * When REFC_H_DEBUG is not defined refc_unlink is an empty macro.
 */
#ifdef REFC_H_DEBUG
int refc_unlink(struct refc_ref *parent, struct refc_ref *child);
#else
#define refc_unlink(P, C)
#endif

#endif /* REFC_H */

#ifdef REFC_H_IMPLEMENTATION
#undef REFC_H_IMPLEMENTATION

#include <stdatomic.h>
#include <stddef.h>
#include <stdlib.h>

#ifdef REFC_H_DEBUG
struct ListNode {
	struct ListNode *next;
	struct refc_ref * _Atomic value;
};
#endif

struct refc_ref {
	/* The reference count for this reference */
	atomic_size_t reference_count;

	/* Associated destructor function. Can be NULL */
	void (*destructor)(void *);

#ifdef REFC_H_DEBUG
	/* Contains child links */
	struct ListNode * _Atomic links;
#endif

	/* The memory block returned by refc_access */
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

#ifdef REFC_H_DEBUG
    ref->links = NULL;
#endif

	return ref;
}

void refc_retain(struct refc_ref *ref) {
	atomic_fetch_add(&(ref->reference_count), 1);
}

void refc_release(struct refc_ref *ref) {
	atomic_fetch_sub(&(ref->reference_count), 1);
	if (atomic_load(&(ref->reference_count)) > 0) {
		return;
	}
	if (ref->destructor != NULL) {
		(ref->destructor)(&ref->block);
	}
#ifdef REFC_H_DEBUG
    struct ListNode *head = atomic_load(&(ref->links));
    struct ListNode *next;
    while (head) {
        next = head->next;
        free(head);
        head = next;
    }
#endif
	free(ref);
}

void *refc_access(struct refc_ref *ref) {
	return &ref->block;
}

#ifdef REFC_H_DEBUG
int find_in_lists(struct ListNode *head, struct refc_ref *match) {
    while(head) {
        struct refc_ref *value = atomic_load(&(head->value));
        if (value == match) {
            return 1;
        }
        if (find_in_lists(atomic_load(&(value->links)), match)) {
            return 1;
        }
        head = head->next;
    }
    return 0;
}

int refc_link(struct refc_ref *parent, struct refc_ref *child) {
    /*
     * Check the list of links for a cycle
     */
	struct ListNode *head = atomic_load(&(child->links));
	if (find_in_lists(head, parent)) {
        return 0;
    }

    /*
     * No cycles found, append the child to the parent's list of links
     */
    struct ListNode *new_head = malloc(sizeof(struct ListNode));
    if (new_head == NULL) {
        return 0;
    }
    atomic_store(&(new_head->value), child);
    do {
        head = (struct ListNode *) atomic_load(&parent->links);
        new_head->next = head;
    } while (!atomic_compare_exchange_weak(&(parent->links), &head, new_head));
	return 1;
}

int refc_unlink(struct refc_ref *parent, struct refc_ref *child) {
    /*
     * Remove the child from the parent's list of links.
     * As deleting from a lock-free linked list is a tricky matter
     * this implementation simply sets the particular node to NULL,
     * which will not match any existing child reference.
     */
	struct ListNode *head = atomic_load(&(parent->links));
    while(head) {
        if (child == atomic_load(&(head->value))) {
            atomic_store(&(head->value), NULL);
            return 1;
        }
        head = head->next;
    }
    return 0;
}
#endif

#endif /* REFC_H_IMPLEMENTATION */
