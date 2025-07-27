/*
 * Copyright ©2025 Hal Perkins.  All rights reserved.  Permission is
 * hereby granted to students registered for University of Washington
 * CSE 333 for use solely during Spring Quarter 2025 for purposes of
 * the course.  No other use, copying, distribution, or modification
 * is permitted without prior written consent. Copyrights for
 * third-party components of this work must be honored.  Instructors
 * interested in reusing these course materials should contact the
 * author.
 */

#include <stdio.h>
#include <stdlib.h>

#include "CSE333.h"
#include "LinkedList.h"
#include "LinkedList_priv.h"


///////////////////////////////////////////////////////////////////////////////
// LinkedList implementation.

LinkedList* LinkedList_Allocate(void) {
  // Allocate the linked list record.
  LinkedList *ll = (LinkedList *) malloc(sizeof(LinkedList));
  Verify333(ll != NULL);

  // STEP 1: initialize the newly allocated record structure.
  ll->num_elements = 0;
  ll->head = NULL;
  ll->tail = NULL;

  // Return our newly minted linked list.
  return ll;
}

void LinkedList_Free(LinkedList *list,
                     LLPayloadFreeFnPtr payload_free_function) {
  Verify333(list != NULL);
  Verify333(payload_free_function != NULL);

  // STEP 2: sweep through the list and free all of the nodes' payloads
  // (using the payload_free_function supplied as an argument) and
  // the nodes themselves.
  int orig_size = list->num_elements;
  if (orig_size > 0) {
    LinkedListNode* curr = list->head;
    for (int i = 0; i < orig_size; i++) {
      // can't safely access curr's fields after freeing it
      LinkedListNode* next = curr->next;
      payload_free_function(curr->payload);
      free(curr);
      curr = next;
    }
  }
  // free the LinkedList
  free(list);
}

int LinkedList_NumElements(LinkedList *list) {
  Verify333(list != NULL);
  return list->num_elements;
}

void LinkedList_Push(LinkedList *list, LLPayload_t payload) {
  Verify333(list != NULL);

  // Allocate space for the new node.
  LinkedListNode *ln = (LinkedListNode *) malloc(sizeof(LinkedListNode));
  Verify333(ln != NULL);

  // Set the payload
  ln->payload = payload;

  if (list->num_elements == 0) {
    // Degenerate case; list is currently empty
    Verify333(list->head == NULL);
    Verify333(list->tail == NULL);
    ln->next = ln->prev = NULL;
    list->head = list->tail = ln;
    list->num_elements = 1;
  } else {
    // STEP 3: typical case; list has >=1 elements
    // add in new node to front
    ln->prev = NULL;
    ln->next = list->head;
    // update list state
    list->head->prev = ln;
    list->head = ln;
    list->num_elements++;
  }
}

bool LinkedList_Pop(LinkedList *list, LLPayload_t *payload_ptr) {
  Verify333(payload_ptr != NULL);
  Verify333(list != NULL);

  // STEP 4: implement LinkedList_Pop.  Make sure you test for
  // and empty list and fail.  If the list is non-empty, there
  // are two cases to consider: (a) a list with a single element in it
  // and (b) the general case of a list with >=2 elements in it.
  // Be sure to call free() to deallocate the memory that was
  // previously allocated by LinkedList_Push().
  if (list->num_elements == 0) {
    return false;
  } else {
    *payload_ptr = list->head->payload;
    LinkedListNode* old_head = list->head;
    if (list->num_elements == 1) {
      list->head = NULL;
      list->tail = NULL;
    } else {  // num elems >= 2
      list->head = list->head->next;
      list->head->prev = NULL;
    }
    free(old_head);
    list->num_elements--;
    return true;
  }
}

void LinkedList_Append(LinkedList *list, LLPayload_t payload) {
  Verify333(list != NULL);

  // STEP 5: implement LinkedList_Append.  It's kind of like
  // LinkedList_Push, but obviously you need to add to the end
  // instead of the beginning.

  // allocate space
  LinkedListNode* ln = (LinkedListNode*) malloc(sizeof(LinkedListNode));
  Verify333(ln != NULL);

  // set payload
  ln->payload = payload;

  if (list->num_elements == 0) {
    // empty case
    Verify333(list->head == NULL);
    Verify333(list->tail == NULL);
    ln->next = ln->prev = NULL;
    list->head = list->tail = ln;
    list->num_elements = 1;
  } else {
    // >=1 elems
    // set new node state
    ln->prev = list->tail;
    ln->next = NULL;
    // update list state
    list->tail->next = ln;
    list->tail = ln;
    list->num_elements++;
  }
}

void LinkedList_Sort(LinkedList *list, bool ascending,
                     LLPayloadComparatorFnPtr comparator_function) {
  Verify333(list != NULL);
  if (list->num_elements < 2) {
    // No sorting needed.
    return;
  }

  // We'll implement bubblesort! Nnice and easy, and nice and slow :)
  int swapped;
  do {
    LinkedListNode *curnode;

    swapped = 0;
    curnode = list->head;
    while (curnode->next != NULL) {
      int compare_result = comparator_function(curnode->payload,
                                               curnode->next->payload);
      if (ascending) {
        compare_result *= -1;
      }
      if (compare_result < 0) {
        // Bubble-swap the payloads.
        LLPayload_t tmp;
        tmp = curnode->payload;
        curnode->payload = curnode->next->payload;
        curnode->next->payload = tmp;
        swapped = 1;
      }
      curnode = curnode->next;
    }
  } while (swapped);
}


///////////////////////////////////////////////////////////////////////////////
// LLIterator implementation.

LLIterator* LLIterator_Allocate(LinkedList *list) {
  Verify333(list != NULL);

  // OK, let's manufacture an iterator.
  LLIterator *li = (LLIterator *) malloc(sizeof(LLIterator));
  Verify333(li != NULL);

  // Set up the iterator.
  li->list = list;
  li->node = list->head;

  return li;
}

void LLIterator_Free(LLIterator *iter) {
  Verify333(iter != NULL);
  free(iter);
}

bool LLIterator_IsValid(LLIterator *iter) {
  Verify333(iter != NULL);
  Verify333(iter->list != NULL);

  return (iter->node != NULL);
}

bool LLIterator_Next(LLIterator *iter) {
  Verify333(iter != NULL);
  Verify333(iter->list != NULL);
  Verify333(iter->node != NULL);

  // STEP 6: try to advance iterator to the next node and return true if
  // you succeed, false otherwise
  // Note that if the iterator is already at the last node,
  // you should move the iterator past the end of the list
  iter->node = iter->node->next;
  return iter->node != NULL;
}

void LLIterator_Get(LLIterator *iter, LLPayload_t *payload) {
  Verify333(iter != NULL);
  Verify333(iter->list != NULL);
  Verify333(iter->node != NULL);

  *payload = iter->node->payload;
}

bool LLIterator_Remove(LLIterator *iter,
                       LLPayloadFreeFnPtr payload_free_function) {
  Verify333(iter != NULL);
  Verify333(iter->list != NULL);
  Verify333(iter->node != NULL);

  // STEP 7: implement LLIterator_Remove.  This is the most
  // complex function you'll build.  There are several cases
  // to consider:
  // - degenerate case: the list becomes empty after deleting.
  // - degenerate case: iter points at head
  // - degenerate case: iter points at tail
  // - fully general case: iter points in the middle of a list,
  //                       and you have to "splice".
  //
  // Be sure to call the payload_free_function to free the payload
  // the iterator is pointing to, and also free any LinkedList
  // data structure element as appropriate.

  // iter->node is different when its time to free
  LinkedListNode* remove = iter->node;
  if (iter->list->num_elements == 1) {
    iter->list->head = iter->list->tail = NULL;
    iter->list->num_elements = 0;
    iter->node = NULL;
    payload_free_function(remove->payload);
    free(remove);
    return false;
  }
  if (iter->node == iter->list->head) {
    iter->list->head = iter->list->head->next;
    iter->list->head->prev = NULL;

    LLIteratorRewind(iter);

    payload_free_function(remove->payload);
    free(remove);
    iter->list->num_elements--;
  } else if (iter->node == iter->list->tail) {
    LinkedListNode* new_tail = iter->list->tail->prev;

    LLPayload_t payload_ptr;
    payload_free_function(remove->payload);
    LLSlice(iter->list, &payload_ptr);

    iter->node =  new_tail;
    // slice decremented num elems and freed remove node, don't repeat
  } else {  // middle
    LinkedListNode* prev = iter->node->prev;
    LinkedListNode* next = iter->node->next;
    prev->next = next;
    next->prev = prev;

    iter->node = next;

    payload_free_function(remove->payload);
    free(remove);
    iter->list->num_elements--;
  }
  return true;
}


///////////////////////////////////////////////////////////////////////////////
// Helper functions

bool LLSlice(LinkedList *list, LLPayload_t *payload_ptr) {
  Verify333(payload_ptr != NULL);
  Verify333(list != NULL);

  // STEP 8: implement LLSlice.
  LinkedListNode* old_tail = list->tail;
  if (list->num_elements == 0) {
    return false;
  } else {
    *payload_ptr = list->tail->payload;
    if (list->num_elements == 1) {
      list->head = NULL;
      list->tail = NULL;
    } else {  // num elems >= 2
      list->tail = list->tail->prev;
      list->tail->next = NULL;
    }
    free(old_tail);
    list->num_elements--;
    return true;
  }
}

void LLIteratorRewind(LLIterator *iter) {
  iter->node = iter->list->head;
}
