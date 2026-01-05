#ifndef dictu_linked_list_h
#define dictu_linked_list_h
#include "../include/dictu_include.h"
#include <stdio.h>

typedef struct ListNode {
  struct ListNode* prev;
  struct ListNode* next;
  void* data;
} ListNode;

typedef struct LinkedList {
  ListNode* head;
  ListNode* tail;
  size_t length;
} LinkedList;



LinkedList* createList(DictuVM* vm);
ListNode* appendLinkedList(DictuVM* vm, LinkedList* list, void* data);
ListNode* prependLinkedList(DictuVM* vm, LinkedList* list, void* data);
ListNode* find(DictuVM* vm, LinkedList* list, void* data);
void removeListEntry(DictuVM* vm, LinkedList* list, void* data);
void freeList(DictuVM* vm,  LinkedList* list);
void* popFront(DictuVM* vm,  LinkedList* list);
void* popBack(DictuVM* vm,  LinkedList* list);





#endif