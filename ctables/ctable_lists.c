//
// ctable linked list routines
//
//
// $Id$
//


//
// ctable_ListInit - initialize a list
//
inline void
ctable_ListInit (ctable_BaseRow **listPtr)
{
    *listPtr = NULL;
}

//
// ctable_ListEmpty - return 1 if the list is empty, else 0.
//
inline int
ctable_ListEmpty (ctable_BaseRow *list)
{
    return (list == NULL);
}

//
// ctable_ListRemove - remove a row from a list
//
inline void
ctable_ListRemove (ctable_BaseRow *row, int i)
{
    // if there's an object following me, make his prev be my prev
    if (row->_ll_nodes[i].next != NULL) {
        row->_ll_nodes[i].next->_ll_nodes[i].prev = row->_ll_nodes[i].prev;
    }

    // make my prev's next (or header) point to my next
    *row->_ll_nodes[i].prev = row->_ll_nodes[i].next;

    // i'm removed
    row->_ll_nodes[i].prev = NULL;
}

//
// ctable_ListRemoveMightBeTheLastOne - remove a row from a list
//
inline int
ctable_ListRemoveMightBeTheLastOne (ctable_BaseRow *row, int i)
{
    int mightBeTheLastOne;

    // if there's an object following me, make his prev be my prev
    if (row->_ll_nodes[i].next != NULL) {
        // set the next guy's prev ptr to be my prev ptr
        row->_ll_nodes[i].next->_ll_nodes[i].prev = row->_ll_nodes[i].prev;
	mightBeTheLastOne = 0;
    } else {
	// else note that there was nothing following me
        mightBeTheLastOne = 1;
    }

    // make my prev's next (or header) point to my next
    *row->_ll_nodes[i].prev = row->_ll_nodes[i].next;

    // i'm removed
    row->_ll_nodes[i].prev = NULL;

    return mightBeTheLastOne;
}


inline void
ctable_ListInsertHead (ctable_BaseRow **listPtr, ctable_BaseRow *row, int i)
{
    // make the row's prev point to the address of the head pointer. Do this
    // first so it'll be in place before the readers can see it.
    row->_ll_nodes[i].prev = listPtr;
    row->_ll_nodes[i].head = listPtr;

    // make the new row's next point to what the head currently points
    // to, possibly NULL
    if ((row->_ll_nodes[i].next = *listPtr) != NULL) {

        // it wasn't null, make the node pointed to by head's prev
	// point to the new row. We can do this now because the row
	// itself is fully initialised.

        (*listPtr)->_ll_nodes[i].prev = &row->_ll_nodes[i].next;
    }

    // Make the head point to the new row. Do this last, so that
    // all links in row are intact before it's seen by readers.
    *listPtr = row;

// printf ("insert head %lx i %d\n", (long unsigned int)listPtr, i);
}

//
// ctable_ListInsertBefore - insert row2 before row1
//
inline void
ctable_ListInsertBefore (ctable_BaseRow *row1, ctable_BaseRow *row2, int i)
{
    volatile struct ctable_baseRowStruct **row1prev;

    // make row2's head point to row1's head
    row2->_ll_nodes[i].head = row1->_ll_nodes[i].head;

    // make row2's prev point to row1's prev
    row2->_ll_nodes[i].prev = row1->_ll_nodes[i].prev;

    // make row2's next point to row1
    row2->_ll_nodes[i].next = row1;

    // get row1's prev before we overwrite it, because we need to link it last.
    row1prev = (volatile struct ctable_baseRowStruct **)row1->_ll_nodes[i].prev;

    // make row1's prev point to row2's next
    row1->_ll_nodes[i].prev = &row2->_ll_nodes[i].next;

    // make row1's prev's *next* point to row2, and do this last
    *row1prev = row2;
}

//
// ctable_ListInsertAfter - insert row2 after row1
//
inline void
ctable_ListInsertAfter (ctable_BaseRow *row1, ctable_BaseRow *row2, int i) {
    // make row2's head point to row1's head
    row2->_ll_nodes[i].head = row1->_ll_nodes[i].head;

    // make row2's prev point to the address of row1's next
    row2->_ll_nodes[i].prev = &row1->_ll_nodes[i].next;

    // set row2's next pointer to row1's next pointer and see if it's NULL
    if ((row2->_ll_nodes[i].next = row1->_ll_nodes[i].next) != NULL) {

        // it wasn't, make row1's next's prev point to the address of
	// row2's next

        row1->_ll_nodes[i].next->_ll_nodes[i].prev = &row2->_ll_nodes[i].next;
    }

    // make row1's next point to row2. This has to be last, so that the whole
    // structure is in place before a reader can see it.
    row1->_ll_nodes[i].next = row2;
}

