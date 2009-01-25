/* libnih
 *
 * test_alloc.c - test suite for nih/alloc.c
 *
 * Copyright © 2009 Scott James Remnant <scott@netsplit.com>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <nih/test.h>

#include <malloc.h>
#include <stdlib.h>
#include <string.h>

#include <nih/macros.h>
#include <nih/alloc.h>


static void *
malloc_null (size_t size)
{
	return NULL;
}

void
test_new (void)
{
	void *ptr1, *ptr2;

	TEST_FUNCTION ("nih_new");

	/* Check that nih_new works if we don't give it a parent, the block
	 * should be allocated with the size of the type given.
	 */
	TEST_FEATURE ("with no parent");
	ptr1 = nih_new (NULL, int);

	TEST_ALLOC_SIZE (ptr1, sizeof (int));
	TEST_ALLOC_ORPHAN (ptr1);


	/* Check that nih_new works if we do give a parent. */
	TEST_FEATURE ("with parent");
	ptr2 = nih_new (ptr1, char);

	TEST_ALLOC_SIZE (ptr2, sizeof (char));
	TEST_ALLOC_PARENT (ptr2, ptr1);

	nih_free (ptr1);


	/* Check that nih_new returns NULL if allocation fails. */
	TEST_FEATURE ("with failed allocation");
	__nih_malloc = malloc_null;
	ptr1 = nih_new (NULL, int);
	__nih_malloc = malloc;

	TEST_EQ_P (ptr1, NULL);
}

void
test_alloc (void)
{
	void *ptr1, *ptr2;

	TEST_FUNCTION ("nih_alloc");

	/* Check allocation remembers the size, and is possible without
	 * a parent.
	 */
	TEST_FEATURE ("with no parent");
	ptr1 = nih_alloc (NULL, 8096);
	memset (ptr1, 'x', 8096);

	TEST_ALLOC_SIZE (ptr1, 8096);
	TEST_ALLOC_ORPHAN (ptr1);


	/* Check that allocation with a parent remembers the parent */
	TEST_FEATURE ("with a parent");
	ptr2 = nih_alloc (ptr1, 10);
	memset (ptr2, 'x', 10);

	TEST_ALLOC_SIZE (ptr2, 10);
	TEST_ALLOC_PARENT (ptr2, ptr1);

	nih_free (ptr1);


	/* Check that nih_alloc returns NULL if allocation fails. */
	TEST_FEATURE ("with failed allocation");
	__nih_malloc = malloc_null;
	ptr1 = nih_new (NULL, int);
	__nih_malloc = malloc;

	TEST_EQ_P (ptr1, NULL);
}


static void *
realloc_null (void   *ptr,
	      size_t  size)
{
	return NULL;
}

void
test_realloc (void)
{
	void *ptr1, *ptr2, *ptr3;

	TEST_FUNCTION ("nih_realloc");

	/* Check that nih_realloc behaves like nih_alloc if the pointer is
	 * NULL (it should, in fact, just call it)
	 */
	TEST_FEATURE ("as nih_alloc");
	ptr1 = nih_realloc (NULL, NULL, 4096);
	memset (ptr1, 'x', 4096);

	TEST_ALLOC_SIZE (ptr1, 4096);
	TEST_ALLOC_ORPHAN (ptr1);

	nih_free (ptr1);


	/* Check that nih_realloc works if the block doesn't have a parent.
	 */
	TEST_FEATURE ("with no parent");
	ptr1 = nih_alloc (NULL, 4096);
	memset (ptr1, 'x', 4096);

	ptr1 = nih_realloc (ptr1, NULL, 8096);
	memset (ptr1, 'x', 8096);

	TEST_ALLOC_SIZE (ptr1, 8096);
	TEST_ALLOC_ORPHAN (ptr1);


	/* Check that nih_realloc works if the block has a parent, the size
	 * should change but the parent should remain the same.
	 */
	TEST_FEATURE ("with a parent");
	ptr2 = nih_alloc (ptr1, 5);
	memset (ptr2, 'x', 5);

	ptr2 = nih_realloc (ptr2, ptr1, 10);
	memset (ptr2, 'x', 10);

	TEST_ALLOC_SIZE (ptr2, 10);
	TEST_ALLOC_PARENT (ptr2, ptr1);

	nih_free (ptr1);


	/* Check that nih_realloc works if the block being reallocated has
	 * a child.  This is fiddly as they need their parent pointers
	 * adjusted.
	 */
	TEST_FEATURE ("with a child");
	ptr1 = nih_alloc (NULL, 128);
	memset (ptr1, 'x', 128);

	ptr2 = nih_alloc (ptr1, 512);
	memset (ptr2, 'x', 512);

	ptr3 = nih_realloc (ptr1, NULL, 1024);
	memset (ptr3, 'x', 1024);

	TEST_ALLOC_PARENT (ptr2, ptr3);

	nih_free (ptr3);


	/* Check that nih_realloc returns NULL and doesn't alter the block
	 * if the allocator fails.
	 */
	TEST_FEATURE ("with failing realloc");
	ptr1 = nih_alloc (NULL, 10);
	assert (ptr1);
	memset (ptr1, 'x', 10);

	__nih_realloc = realloc_null;
	ptr2 = nih_realloc (ptr1, NULL, 200);
	__nih_realloc = realloc;

	TEST_EQ_P (ptr2, NULL);
	TEST_ALLOC_SIZE (ptr1, 10);

	nih_free (ptr1);
}


static int destructor_was_called;

static int
destructor_called (void *ptr)
{
	destructor_was_called++;

	return 2;
}

static int child_destructor_was_called;

static int
child_destructor_called (void *ptr)
{
	child_destructor_was_called++;

	return 20;
}

void
test_free (void)
{
	void *ptr1, *ptr2;
	int   ret;

	TEST_FUNCTION ("nih_free");

	/* Check that nih_free works if the block has no parent.  The
	 * destructor should get called and nih_free should return that
	 * return value.
	 */
	TEST_FEATURE ("with no parent");
	ptr1 = nih_alloc (NULL, 10);
	nih_alloc_set_destructor (ptr1, destructor_called);
	destructor_was_called = 0;
	ret = nih_free (ptr1);

	TEST_TRUE (destructor_was_called);
	TEST_EQ (ret, 2);


	/* Check that nih_free works if the block has a parent.  The
	 * destructor should get called and nih_free should return that
	 * return value.
	 */
	TEST_FEATURE ("with parent");
	ptr2 = nih_alloc (NULL, 20);

	ptr1 = nih_alloc (ptr2, 10);
	nih_alloc_set_destructor (ptr1, destructor_called);
	destructor_was_called = 0;
	ret = nih_free (ptr1);

	TEST_TRUE (destructor_was_called);
	TEST_EQ (ret, 2);

	nih_free (ptr2);


	/* Check that the destructor on any children also gets called, which
	 * is as good a indication as any that the children are being freed.
	 */
	TEST_FEATURE ("with destructor on child");
	ptr1 = nih_alloc (NULL, 10);
	ptr2 = nih_alloc (ptr1, 10);
	nih_alloc_set_destructor (ptr2, child_destructor_called);
	child_destructor_was_called = 0;
	ret = nih_free (ptr1);

	TEST_TRUE (child_destructor_was_called);
	TEST_EQ (ret, 0);


	/* Check that both destructors on parent and children are called,
	 * and that the return value from nih_free is that of the parent's.
	 */
	TEST_FEATURE ("with child and destructors");
	ptr1 = nih_alloc (NULL, 10);
	ptr2 = nih_alloc (ptr1, 10);
	nih_alloc_set_destructor (ptr1, destructor_called);
	nih_alloc_set_destructor (ptr2, child_destructor_called);
	destructor_was_called = 0;
	child_destructor_was_called = 0;
	ret = nih_free (ptr1);

	TEST_TRUE (destructor_was_called);
	TEST_TRUE (child_destructor_was_called);
	TEST_EQ (ret, 2);
}


void
test_discard (void)
{
	void *ptr1, *ptr2;
	int   ret;

	TEST_FUNCTION ("nih_discard");

	/* Check that nih_discard works if the block has no parent, freeing
	 * the object.  The destructor should get called and nih_discard
	 * should return that return value.
	 */
	TEST_FEATURE ("with no parent");
	ptr1 = nih_alloc (NULL, 10);
	nih_alloc_set_destructor (ptr1, destructor_called);
	destructor_was_called = 0;
	ret = nih_discard (ptr1);

	TEST_TRUE (destructor_was_called);
	TEST_EQ (ret, 2);


	/* Check that nih_discard does nothing it the block has a parent.
	 */
	TEST_FEATURE ("with parent");
	ptr2 = nih_alloc (NULL, 20);

	ptr1 = nih_alloc (ptr2, 10);
	nih_alloc_set_destructor (ptr1, destructor_called);
	destructor_was_called = 0;
	ret = nih_discard (ptr1);

	TEST_FALSE (destructor_was_called);
	TEST_EQ (ret, 0);

	nih_free (ptr2);


	/* Check that the destructor on any children also gets called, which
	 * is as good a indication as any that the children are being freed.
	 */
	TEST_FEATURE ("with destructor on child");
	ptr1 = nih_alloc (NULL, 10);
	ptr2 = nih_alloc (ptr1, 10);
	nih_alloc_set_destructor (ptr2, child_destructor_called);
	child_destructor_was_called = 0;
	ret = nih_discard (ptr1);

	TEST_TRUE (child_destructor_was_called);
	TEST_EQ (ret, 0);


	/* Check that both destructors on parent and children are called,
	 * and that the return value from nih_discard is that of the parent's.
	 */
	TEST_FEATURE ("with child and destructors");
	ptr1 = nih_alloc (NULL, 10);
	ptr2 = nih_alloc (ptr1, 10);
	nih_alloc_set_destructor (ptr1, destructor_called);
	nih_alloc_set_destructor (ptr2, child_destructor_called);
	destructor_was_called = 0;
	child_destructor_was_called = 0;
	ret = nih_discard (ptr1);

	TEST_TRUE (destructor_was_called);
	TEST_TRUE (child_destructor_was_called);
	TEST_EQ (ret, 2);
}


void
test_ref (void)
{
	void *ptr1, *ptr2, *ptr3;

	TEST_FUNCTION ("nih_ref");


	/* Check that we can add a reference to an object that has no
	 * parent.
	 */
	TEST_FEATURE ("with no existing parent");
	ptr1 = nih_alloc (NULL, 100);
	memset (ptr1, 'x', 100);

	ptr2 = nih_alloc (NULL, 100);
	memset (ptr2, 'y', 100);

	nih_ref (ptr1, ptr2);

	TEST_ALLOC_PARENT (ptr1, ptr2);

	nih_free (ptr2);


	/* Check that we can add a reference to an object that already has
	 * a parent, and that both shall be parents afterwards.
	 */
	TEST_FEATURE ("with existing parent");
	ptr1 = nih_alloc (NULL, 100);
	memset (ptr1, 'x', 100);

	ptr2 = nih_alloc (ptr1, 100);
	memset (ptr2, 'y', 100);

	ptr3 = nih_alloc (NULL, 100);
	memset (ptr2, 'z', 100);

	nih_ref (ptr2, ptr3);

	TEST_ALLOC_PARENT (ptr2, ptr1);
	TEST_ALLOC_PARENT (ptr2, ptr3);

	nih_free (ptr1);
	nih_free (ptr3);
}

void
test_unref (void)
{
	void *ptr1, *ptr2, *ptr3;

	TEST_FUNCTION ("nih_unref");


	/* Check that we can remove a reference from an object with multiple
	 * parents, which means the object will not be freed.
	 */
	TEST_FEATURE ("with multiple parents");
	ptr1 = nih_alloc (NULL, 100);
	memset (ptr1, 'x', 100);

	ptr2 = nih_alloc (ptr1, 100);
	memset (ptr2, 'y', 100);

	ptr3 = nih_alloc (NULL, 100);
	memset (ptr2, 'z', 100);

	nih_ref (ptr2, ptr3);

	nih_alloc_set_destructor (ptr2, destructor_called);
	destructor_was_called = 0;

	nih_unref (ptr2, ptr1);

	TEST_FALSE (destructor_was_called);
	TEST_ALLOC_PARENT (ptr2, ptr3);

	nih_free (ptr1);
	nih_free (ptr3);


	/* Check that when we remove the last reference from an object,
	 * the object is freed.
	 */
	TEST_FEATURE ("with last parent");
	ptr1 = nih_alloc (NULL, 100);
	memset (ptr1, 'x', 100);

	ptr2 = nih_alloc (ptr1, 100);
	memset (ptr2, 'y', 100);

	nih_alloc_set_destructor (ptr2, destructor_called);
	destructor_was_called = 0;

	nih_unref (ptr2, ptr1);

	TEST_TRUE (destructor_was_called);

	nih_free (ptr1);
}


void
test_parent (void)
{
	void *ptr1, *ptr2, *ptr3;

	TEST_FUNCTION ("nih_alloc_parent");


	/* Check that nih_alloc_parent returns TRUE when the passed object
	 * is a child of the passed parent.
	 */
	TEST_FEATURE ("with child and parent");
	ptr1 = nih_alloc (NULL, 10);
	ptr2 = nih_alloc (ptr1, 10);

	TEST_TRUE (nih_alloc_parent (ptr2, ptr1));

	nih_free (ptr1);


	/* Check that nih_alloc_parent returns TRUE when the passed object
	 * has a parent and NULL is passed.
	 */
	TEST_FEATURE ("with child and NULL");
	ptr1 = nih_alloc (NULL, 10);
	ptr2 = nih_alloc (ptr1, 10);

	TEST_TRUE (nih_alloc_parent (ptr2, ptr1));

	nih_free (ptr1);


	/* Check that nih_alloc_parent returns FALSE when the passed object
	 * is a child but not of the passed parent.
	 */
	TEST_FEATURE ("with child and wrong parent");
	ptr1 = nih_alloc (NULL, 10);
	ptr2 = nih_alloc (ptr1, 10);
	ptr3 = nih_alloc (NULL, 10);

	TEST_FALSE (nih_alloc_parent (ptr2, ptr3));

	nih_free (ptr1);
	nih_free (ptr3);


	/* Check that nih_alloc_parent returns FALSE when the passed object
	 * is an orphan.
	 */
	TEST_FEATURE ("with orphan");
	ptr1 = nih_alloc (NULL, 10);
	ptr2 = nih_alloc (NULL, 10);

	TEST_FALSE (nih_alloc_parent (ptr2, ptr1));

	nih_free (ptr1);
	nih_free (ptr2);


	/* Check that nih_alloc_parent returns FALSE when the passed object
	 * is an orphan and NULL is passed.
	 */
	TEST_FEATURE ("with orphan and NULL");
	ptr1 = nih_alloc (NULL, 10);

	TEST_FALSE (nih_alloc_parent (ptr1, NULL));

	nih_free (ptr1);
}


int
main (int   argc,
      char *argv[])
{
	test_new ();
	test_alloc ();
	test_realloc ();
	test_free ();
	test_discard ();
	test_ref ();
	test_unref ();
	test_parent ();

	return 0;
}
