/**
 * List 
 */
#include "tools/list.h"

/**
 * Init list
 */
void list_init(list_t *list) {
    list->first = list->last = (list_node_t *)0;
    list->count = 0;
}

/**
 * Insert the specified entry at the head of the specified linked list
 */
void list_insert_first(list_t *list, list_node_t *node) {
    // set up the preceding and succeeding nodes for the node to be inserted, with nothing preceding it
    node->next = list->first;
    node->pre = (list_node_t *)0;

    // if it's empty, you need to set both 'first' and 'last' to point to itself
    if (list_is_empty(list)) {
        list->last = list->first = node;
    } else {
        // otherwise, set the first node
        list->first->pre = node;

        // modify first
        list->first = node;
    }

    list->count++;
}


/**
 * Insert the node into tail of specific list
 */
void list_insert_last(list_t *list, list_node_t *node) {
    // set node
    node->pre = list->last;
    node->next = (list_node_t*)0;

    // if list is empty, first/last points to the only node
    if (list_is_empty(list)) {
        list->first = list->last = node;
    } else {
        // otherwise, modify last to the node
        list->last->next = node;

        // node becomes the new successor node
        list->last = node;
    }

    list->count++;
}

/**
 * Remove  head of specific list
 */
list_node_t* list_remove_first(list_t *list) {
    // if entry is empty, return empty
    if (list_is_empty(list)) {
        return (list_node_t*)0;
    }

    // retrieve first node
    list_node_t * remove_node = list->first;

    // Move 'first' one towards the end of the list, skipping the one that was just moved. If there is no successor, set 'first' to 0
    list->first = remove_node->next;
    if (list->first == (list_node_t *)0) {
        // node is the last node
        list->last = (list_node_t*)0;
    } else {
        //  non-last nodes, clear the predecessor of the successor to 0
        remove_node->next->pre = (list_node_t *)0;
    }

    // adjust 'node' itself to 0 because there is no successor node
    remove_node->next = remove_node->pre = (list_node_t*)0;

    // adjust the count value simultaneously
    list->count--;
    return remove_node;
}

/**
 * @brief Remove the entry from the specified linked list without checking if the node is within the list
 */
list_node_t * list_remove(list_t *list, list_node_t *remove_node) {
    // if it's head, move forward
    if (remove_node == list->first) {
        list->first = remove_node->next;
    }

    // if it's tail, move back
    if (remove_node == list->last) {
        list->last = remove_node->pre;
    }

    // if have pre, modify it to next
    if (remove_node->pre) {
        remove_node->pre->next = remove_node->next;
    }

    // if have next, modify it to next
    if (remove_node->next) {
        remove_node->next->pre = remove_node->pre;
    }

    // clear node pre
    remove_node->pre = remove_node->next = (list_node_t*)0;
    --list->count;
    return remove_node;
}
