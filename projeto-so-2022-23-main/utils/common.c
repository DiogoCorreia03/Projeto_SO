#include <stdio.h>
#include <stdlib.h>
#include "../utils/common.h"
#include "string.h"

Box* getBox(struct Box *head, char *box_name) {
    Box *current = head;

    while(current != NULL) {

        if (current->box_name == box_name) 
            return current;
    }
    return NULL;
}

int insertBox(struct Box *head, char *box_name, int file_handle, uint64_t box_size) {
    Box *new_node = (struct Node *)malloc(sizeof(Box));

    strcpy(new_node->box_name, box_name);
    new_node->file_handle = file_handle;
    new_node->box_size = box_size;
    new_node->n_publishers = 0;
    new_node->n_subscribers = 0;

    new_node->next = NULL;

    Box *current = head;

    while (current->next != NULL) {
        if (current->box_name == box_name) {
            free(current);
            return -1;
        }

        current = current->next;
    }
    current->next = new_node;

    return 0;
}