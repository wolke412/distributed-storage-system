#include "fs.h"

#include <stdio.h>
#include <stdlib.h>

int xfilenetindex_init(xFileNetworkIndex *net) {
    if (!net) return 0;
    net->file_count = 0;
    net->files = NULL;
    net->TAIL  = NULL;

    return 1;
}

xFileInNetwork *xfilenetindex_new_file(uint16_t file_id, uint64_t fragment_count) {
    xFileInNetwork *file = calloc(1, sizeof(xFileInNetwork));
    if (!file) return NULL;

    file->file_id = file_id;
    file->total_fragments = fragment_count;

    if (fragment_count > 0) {
        file->fragments = calloc(fragment_count, sizeof(xFragmentNetworkPointer));
        if (!file->fragments) {
            free(file);
            return NULL;
        }
    }

    file->next = NULL;
    return file;
}

void xfilenetindex_add_file(xFileNetworkIndex *net, xFileInNetwork *file) {
    if (!net || !file) return;

    if (!net->files) {
        net->files = file;
        net->TAIL = file;
    } else {
        net->TAIL->next = file;
        net->TAIL = file;
    }

    net->file_count++;
}

/**
 *  Finds the file based on its id
 * ------------------------------------------------------------ 
 *  Note:   Since ids are incremental, this could use BS.
 *          The list will always be ordered. 
 */
xFileInNetwork *xfilenetindex_find_file(xFileNetworkIndex *net, uint16_t file_id) {
    if (!net) return NULL;

    xFileInNetwork *curr = net->files;
    while (curr)
    {
        if (curr->file_id == file_id)
            return curr;
        curr = curr->next;
    }
    return NULL;
}

void xfilenetindex_debug(const xFileNetworkIndex *net) {
    if (!net)
    {
        printf("[xfilenetindex] Network index is NULL\n");
        return;
    }

    printf("\n=== FILE INDEX Debug ===\n");
    printf("Total files: %u\n\n", net->file_count);

    if (!net->files)
    {
        printf("  [xfilenetindex] No files registered.\n\n");
        return;
    }

    const xFileInNetwork *file = net->files;
    uint16_t index = 0;

    while (file != NULL) {
        printf("I: %p\n", file);
        printf("File #%u: ID: %u\n", index++, file->file_id);
        printf("  Total fragments: %llu\n",
               (unsigned long long)file->total_fragments);

        if (!file->fragments) {
            printf("    [no fragments allocated]\n");
        } else {
            for (uint64_t f = 0; f < file->total_fragments; f++) {
                const xFragmentNetworkPointer *frag = &file->fragments[f];
                printf("    Fragment #%3llu | fragment_id: %5u | node_id: %10llu\n",
                       (unsigned long long)f,
                       frag->fragment,
                       (unsigned long long)frag->node_id);
            }
        }

        printf("\n");
        file = file->next;
    }
}

