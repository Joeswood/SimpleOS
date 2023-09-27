/**
 * Linklist
 */
#ifndef LIST_H
#define LIST_H


#define offset_in_parent(parent_type, node_name)    \
    ((uint32_t)&(((parent_type*)0)->node_name))


#define offset_to_parent(node, parent_type, node_name)   \
    ((uint32_t)node - offset_in_parent(parent_type, node_name))


#define list_node_parent(node, parent_type, node_name)   \
        ((parent_type *)(node ? offset_to_parent((node), parent_type, node_name) : 0))

/**
 * Linklist Node
 */
typedef struct _list_node_t {
    struct _list_node_t* pre;           // previous node
    struct _list_node_t* next;         // next node
}list_node_t;

/**
 * Init head node
 */
static inline void list_node_init(list_node_t *node) {
    node->pre = node->next = (list_node_t *)0;
}

/**
 * Retrieve previous node
 */
static inline list_node_t * list_node_pre(list_node_t *node) {
    return node->pre;
}

/**
 * Retrieve next node
 */
static inline list_node_t * list_node_next(list_node_t *node) {
    return node->next;
}


typedef struct _list_t {
    list_node_t * first;            // head
    list_node_t * last;             // tail
    int count;                        // node count
}list_t;

void list_init(list_t *list);

/**
 * if LL is empty
 */
static inline int list_is_empty(list_t *list) {
    return list->count == 0;
}

/**
 * Node count
 */
static inline int list_count(list_t *list) {
    return list->count;
}

/**
 * Retrieve first node of list
 */
static inline list_node_t* list_first(list_t *list) {
    return list->first;
}

/**
 * Retrieve last node of list
 */
static inline list_node_t* list_last(list_t *list) {
    return list->last;
}

void list_insert_first(list_t *list, list_node_t *node);
void list_insert_last(list_t *list, list_node_t *node);
list_node_t* list_remove_first(list_t *list);
list_node_t* list_remove(list_t *list, list_node_t *node);

#endif /* LIST_H */
