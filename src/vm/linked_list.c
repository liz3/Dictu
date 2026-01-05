#include "linked_list.h"
#include "memory.h"

LinkedList *createList(DictuVM *vm) {
    LinkedList *entry = ALLOCATE(vm, LinkedList, 1);
    entry->head = NULL;
    entry->tail = NULL;
    entry->length = 0;
    return entry;
}
ListNode *appendLinkedList(DictuVM *vm, LinkedList *list, void *data) {
    ListNode *node = ALLOCATE(vm, ListNode, 1);
    node->data = data;
    node->next = NULL;
    node->prev = NULL;
    if (list->length == 0) {
        list->head = node;
        list->tail = node;
    } else {
        ListNode *tail = list->tail;
        tail->next = node;
        node->prev = tail;
        list->tail = node;
    }
    list->length += 1;

    return node;
}
ListNode *prependLinkedList(DictuVM *vm, LinkedList *list, void *data) {
    ListNode *node = ALLOCATE(vm, ListNode, 1);
    node->data = data;
    node->next = NULL;
    node->prev = NULL;
    if (list->length == 0) {
        list->head = node;
        list->tail = node;
    } else {
        ListNode *head = list->head;
        node->next = head;
        head->prev = node;
        list->head = node;
    }
    list->length += 1;

    return node;
}
ListNode *find(DictuVM *vm, LinkedList *list, void *data) {
    (void)vm;
    ListNode *current = list->head;
    while (current != NULL) {
        if (current->data == data) {
            return current;
        }
        current = current->next;
    }

    return NULL;
}

void removeListEntry(DictuVM *vm, LinkedList *list, void *data) {
    ListNode *entry = find(vm, list, data);
    if (entry == NULL) {
        return;
    }
    if (entry->prev) {
        if (entry->next) {
            entry->prev->next = entry->next;
            entry->next->prev = entry->prev;
        } else {
            list->tail = entry->prev;
            list->tail->next = NULL;
        }
    } else {
        if (entry->next) {
            list->head = entry->next;
            list->head->prev = NULL;
        } else {
            list->tail = NULL;
            list->head = NULL;
        }
    }
    list->length -= 1;
    FREE(vm, ListNode, entry);
}
void *popFront(DictuVM *vm, LinkedList *list) {

    if (list->length == 0) {
        return NULL;
    }
    ListNode *n = list->head;
    void *data = n->data;
    if (n->next) {
        list->head = n->next;
        n->next->prev = NULL;
    } else {
        list->head = NULL;
        list->tail = NULL;
    }
    FREE(vm, ListNode, n);
    list->length -= 1;
    return data;
}
void *popBack(DictuVM *vm, LinkedList *list) {
    (void)vm;
    if (list->length == 0) {
        return NULL;
    }
    ListNode *n = list->tail;
    void *data = n->data;

    if (n->prev) {
        list->tail = n->prev;
        list->tail->next = NULL;
    } else {
        list->head = NULL;
        list->tail = NULL;
    }
    FREE(vm, ListNode, n);

    list->length -= 1;
    return data;
}
void freeList(DictuVM *vm, LinkedList *list) {
    ListNode *current = list->head;
    while (current != NULL) {
        ListNode *n = current->next;
        FREE(vm, ListNode, current);
        current = n;
    }
    FREE(vm, LinkedList, list);
}