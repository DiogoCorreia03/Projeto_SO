#include "../utils/common.h"
#include "../utils/logging.h"
#include "string.h"
#include <stdio.h>
#include <stdlib.h>

struct Box *getBox(struct Box *head, char *box_name) {
    struct Box *current = head;

    while (current != NULL) {
        if (current->box_name == box_name)
            return current;

        current = current->next;
    }
    return NULL;
}

int insertBox(struct Box *head, char *box_name, uint64_t box_size) {
    struct Box *new_node = (struct Box *)malloc(sizeof(struct Box));
    if (new_node == NULL) {
        fprintf(stderr,"Unable to alloc memory to create Box.\n");
        return -1;
    }

    strcpy(new_node->box_name, box_name);
    new_node->box_size = box_size;
    new_node->n_publishers = 0;
    new_node->n_subscribers = 0;
    new_node->last = 1;
    new_node->next = NULL;

    if (head == NULL) {
        head = new_node;
        return 0;
    }

    struct Box *current = head;

    while (current->next != NULL) {
        if (current->box_name == box_name) {
            free(new_node);
            return -1;
        }

        current = current->next;
    }

    current->last = 0;
    current->next = new_node;

    return 0;
}

int insertionSort(struct Box *head, char *box_name, uint64_t box_size,
                  uint64_t n_publishers, uint64_t n_subscribers) {
    struct Box *new_node = (struct Box *)malloc(sizeof(struct Box));
    if (new_node == NULL) {
        fprintf(stderr,"Unable to alloc memory to create Box.\n");
        return -1;
    }

    strcpy(new_node->box_name, box_name);
    new_node->box_size = box_size;
    new_node->n_publishers = n_publishers;
    new_node->n_subscribers = n_subscribers;
    new_node->next = NULL;
    new_node->last = 0;

    if (head == NULL || strcmp(box_name, head->box_name) < 0) {
        new_node->next = head;
        head = new_node;
    } else {
        struct Box *current = head;
        while (current->next != NULL &&
               strcmp(box_name, current->next->box_name) > 0) {
            current = current->next;
        }
        new_node->next = current->next;
        current->next = new_node;
    }

    return 0;
}

int deleteBox(struct Box *head, char *box_name) {
    struct Box *curr = head;
    struct Box *prev = NULL;

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

void box_to_string(struct Box *box, char *buffer) {
    memcpy(buffer, &box->last, UINT8_T_SIZE);
    buffer += UINT8_T_SIZE;

    memcpy(buffer, &box->box_name, BOX_NAME_LENGTH);
    buffer += BOX_NAME_LENGTH;

    memcpy(buffer, &box->box_size, sizeof(uint64_t));
    buffer += sizeof(uint64_t);

    memcpy(buffer, &box->n_publishers, sizeof(uint64_t));
    buffer += sizeof(uint64_t);

    memcpy(buffer, &box->n_subscribers, sizeof(uint64_t));

    buffer -= (UINT8_T_SIZE + BOX_NAME_LENGTH +
               sizeof(uint64_t) + sizeof(uint64_t));
}

void destroy_list(struct Box *head) {
    struct Box *current = head;
    while (current != NULL) {
        current = current->next;
        free(current);
    }
}

void print_list(struct Box *head) {
    struct Box *current = head;
    while (current != NULL) {
        fprintf(stdout, "%s %zu %zu %zu\n", current->box_name,
                current->box_size, current->n_publishers,
                current->n_subscribers);
        current = current->next;
    }
}