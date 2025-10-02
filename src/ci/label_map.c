#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "label_map.h"

static void free_entry(Entry *e);
static void free_entries(Entry *e);
static unsigned long hash_function(char *s);
static Entry *entry_init(char *id, Command *command);

bool label_map_init(LabelMap *map, int capacity) {
    map->entries = calloc(capacity, sizeof(Entry *));
    if (!map->entries) {
        return false;
    }
    map->capacity = capacity;
    return true;
}

/**
 * @brief Frees the resources for a `singular` entry.
 *
 * @param e The pointer to the entry to free.
 */
static void free_entry(Entry *e) {
    if (!e) {
        return;
    }
    free(e->id);
    free(e);
}

/**
 * @brief Frees the given list of entries.
 *
 * @param e A pointer to the first entry to free.
 */
static void free_entries(Entry *e) {
    if (!e) {
        return;
    }
    free_entries(e->next);
    free_entry(e);
}

void label_map_free(LabelMap *map) {
    for (int i = 0; i < map->capacity; i++) {
        free_entries(map->entries[i]);
    }
    free(map->entries);
    map->entries = NULL;
    map->capacity = 0;
}

/**
 * @brief Returns a hash of the specified id.
 *
 * @param s The string to hash.
 * @return The hash of `s`
 */
static unsigned long hash_function(char *s) {
    unsigned long i = 0;
    for (int j = 0; s[j]; j++) {
        i += s[j];
    }

    return i;
}

/**
 * @brief Initializes the given entry's state.
 *
 * @param id The id associated with this entry.
 * @param command The command associated with this entry.
 * @return True if the entry was initialized successfully, false otherwise.
 */
static Entry *entry_init(char *id, Command *command) {
    Entry *new_entry = malloc(sizeof(Entry));
    if (!new_entry) return NULL;

    new_entry->id = malloc(strlen(id) + 1);
    if (!new_entry->id) {
        free(new_entry);
        return NULL;
    }
    
    strcpy(new_entry->id, id);
    new_entry->command = command;
    new_entry->next = NULL;
    return new_entry;
}

bool put_label(LabelMap *map, char *id, Command *command) {
    unsigned long hash = hash_function(id) % map->capacity;
    Entry **bucket = &map->entries[hash];
    Entry *current = *bucket;
    while (current) {
        if (strcmp(current->id, id) == 0) return false;
        current = current->next;
    }
    Entry *new_entry = entry_init(id, command);
    if (!new_entry) return false;
    new_entry->next = *bucket;
    *bucket = new_entry;
    return true;
}

Entry *get_label(LabelMap *map, char *id) {
    unsigned long hash = hash_function(id) % map->capacity;
    Entry *entry = map->entries[hash];
    while (entry) {
        if (strcmp(entry->id, id) == 0) {
            return entry;
        }
        entry = entry->next;
    }
    return NULL;
}