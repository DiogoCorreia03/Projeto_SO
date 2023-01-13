#include "../utils/common.h"
#include "../utils/logging.h"
#include "string.h"
#include <stdio.h>
#include <stdlib.h>

Box *getBox(Box *head, char *box_name) {
    Box *current = head;

    while (current != NULL) {
        if (current->box_name == box_name)
            return current;

        current = current->next;
    }
    return NULL;
}

int insertBox(Box *head, char *box_name, int file_handle, uint64_t box_size) {
    Box *new_node = (Box *)malloc(sizeof(Box));
    if (new_node == NULL) {
        WARN("Unable to alloc memory to create Box.\n");
        return -1;
    }

    strcpy(new_node->box_name, box_name);
    new_node->file_handle = file_handle;
    new_node->box_size = box_size;
    new_node->n_publishers = 0;
    new_node->n_subscribers = 0;
    new_node->next = NULL;

    if (head == NULL) {
        head = new_node;
        return 0;
    }

    Box *current = head;

    while (current->next != NULL) {
        if (current->box_name == box_name) {
            free(new_node);
            return -1;
        }

        current = current->next;
    }

    current->next = new_node;

    return 0;
}

int insertionSort(Box *head, char *box_name, uint64_t box_size,
                  uint64_t n_publishers, uint64_t n_subscribers) {
    Box *new_node = (Box *)malloc(sizeof(Box));
    if (new_node == NULL) {
        WARN("Unable to alloc memory to create Box.\n");
        return -1;
    }

    strcpy(new_node->box_name, box_name);
    new_node->file_handle = 0;
    new_node->box_size = box_size;
    new_node->n_publishers = n_publishers;
    new_node->n_subscribers = n_subscribers;
    new_node->next = NULL;

    if (head == NULL || strcmp(box_name, head->box_name) < 0) {
        new_node->next = head;
        head = new_node;
    } else {
        Box *current = head;
        while (current->next != NULL &&
               strcmp(box_name, current->next->box_name) > 0) {
            current = current->next;
        }
        new_node->next = current->next;
        current->next = new_node;
    }

    return 0;
}

int deleteBox(Box *head, char *box_name) {
    Box *curr = head;
    Box *prev = NULL;

    while (curr != NULL && curr->box_name != box_name) {
        prev = curr;
        curr = curr->next;
    }

    if (curr != NULL) {
        if (prev != NULL)
            prev->next = curr->next;

        else {
            head = curr->next;
        }

        free(curr);
        return 0;
    }

    return -1;
}

void destroy_list(Box *head) {
    Box *current = head;
    while (current != NULL) {
        current = current->next;
        free(current);
    }
}

void print_list(Box *head) {
    Box *current = head;
    while (current != NULL) {
        fprintf(stdout, "%s %zu %zu %zu\n", current->box_name,
                current->box_size, current->n_publishers,
                current->n_subscribers);
        current = current->next;
    }
}