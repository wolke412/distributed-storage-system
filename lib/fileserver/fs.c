#include "fs.h"




// -----------------------------------------------------------------------------
// Initialize the index
// -----------------------------------------------------------------------------
int xfileserver_init(xFileServer *fs) {
  if ( fs == NULL ) return 0;

  fs->file_count = 0;
  fs->files = malloc( sizeof(xFileContainer) * FILE_SERVER_MAX_FILES );

  return 1;
}

// -----------------------------------------------------------------------------
// Add a file to the index
// -----------------------------------------------------------------------------
xFileContainer *xfileserver_add_file(
    xFileServer *index,
    const char *file_name,
    uint16_t file_id,
    uint64_t total_size,
    uint8_t fragment_count_total
) {
    if (!index || !index->files) return NULL;

    xFileContainer *file = &index->files[index->file_count++];

    memset(file, 0, sizeof(xFileContainer));

    strncpy(file->file_name, file_name, sizeof(file->file_name) - 1);

    file->file_id = file_id;
    file->size = total_size;
    file->fragment_count_total = fragment_count_total;

    if (!file->fragments) return NULL;

    return file;
}

// -----------------------------------------------------------------------------
// Add a fragment to a file
// -----------------------------------------------------------------------------
int xfileserver_add_fragment(
    xFileContainer *file,
    uint8_t fragment_index,
    const void *data,
    uint64_t size
) {
    if (!file || !file->fragments) return -1;
    if (fragment_index >= file->fragment_count_total) return -2;

    xFileFragment *frag = &file->fragments[fragment_index];
    frag->fragment_id = fragment_index;
    frag->fragment_bytes = malloc(size);
    if (!frag->fragment_bytes) return -3;

    memcpy(frag->fragment_bytes, data, size);
    frag->fragment_size = size;
    return 0;
}

// -----------------------------------------------------------------------------
// Free a file container
// -----------------------------------------------------------------------------
void xfileserver_free_file(xFileContainer *file) {
    if (!file) return;

    if (file->fragments) {
        for (uint8_t i = 0; i < file->fragment_count_total; i++) {
            free(file->fragments[i].fragment_bytes);
        }
        free(file->fragments);
    }

    free(file->fragment_bytes);
    memset(file, 0, sizeof(xFileContainer));
}

// -----------------------------------------------------------------------------
// Free the index
// -----------------------------------------------------------------------------
void xfileserver_free_fs(xFileServer *fs) {
    if (!fs) return;

    for (uint16_t i = 0; i < fs->file_count; i++) {
        xfileserver_free_file(&fs->files[i]);
    }

    free(fs->files);
    free(fs);
}

// -----------------------------------------------------------------------------
// Debug: Print the contents of the local file index
// -----------------------------------------------------------------------------
void xfileserver_debug(const xFileServer *fs) {

    if (!fs) {
        printf("[xfileserver] Index is NULL\n");
        return;
    }

    printf("\n=== xLocalFileIndex Debug ===\n");
    printf("Total files: %u\n\n", fs->file_count);

    for (uint16_t i = 0; i < fs->file_count; i++) {
        const xFileContainer *file = &fs->files[i];
        printf("File #%u: '%s'\n", i, file->file_name);
        printf("  ID: %u | Total size: %llu bytes | Fragments: %u\n",
               file->file_id,
               (unsigned long long)file->size,
               file->fragment_count_total);

        if (!file->fragments) {
            printf("  (no fragments loaded)\n\n");
            continue;
        }

        for (uint8_t f = 0; f < file->fragment_count_total; f++) {
            const xFileFragment *frag = &file->fragments[f];
            printf("    Fragment #%3u | size: %8llu bytes \t| preview: \"",
                   frag->fragment_id,
                   (unsigned long long)frag->fragment_size);

            // preview first few bytes
            size_t preview_len = frag->fragment_size < 10 ? frag->fragment_size : 10;
            for (size_t j = 0; j < preview_len; j++) {
                char c = frag->fragment_bytes[j];
                putchar(isprint((unsigned char)c) ? c : '.');
            }
            printf("\"...\n");
        }
        printf("\n");
    }
}