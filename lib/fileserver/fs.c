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

    file->file_id               = file_id;
    file->size                  = total_size;
    file->fragment_count_total  = fragment_count_total;

    // guarantee they do not get mistaken by fragment 
    file->fragments[0].fragment_id = 0;
    file->fragments[1].fragment_id = 0;
    
    return file;
}

// -----------------------------------------------------------------------------
// Add a fragment to a file
// -----------------------------------------------------------------------------
eFileAddFragStatus xfileserver_add_fragment(
    xFileContainer *file,
    uint8_t fragment_id,
    const void *data,
    uint64_t size
) {
    if (!file) return FRAG_ERR_INVALID_FILE;

    int idx = file->fragments[0].fragment_id == 0
        ? 0
        : 1;

    xFileFragment *frag = &file->fragments[idx];
    
    frag->fragment_id = fragment_id;
    frag->fragment_bytes = malloc(size);

    if (!frag->fragment_bytes) return FRAG_ERR_INVALID_BYTES;

    memcpy(frag->fragment_bytes, data, size);

    frag->fragment_size = size;

    return FRAG_OK;
}

// -----------------------------------------------------------------------------
// 
// -----------------------------------------------------------------------------
xFileContainer *xfileserver_find_file( xFileServer *idx, uint64_t id ) {

    xFileContainer *f = NULL;

    for (int i = 0; i < idx->file_count; i++) {
        f = idx->files + i;
        if ( f->file_id == id ) break;
        f = NULL;
    }

    return f;
}

xFileContainer *xfileserver_find_file_by_name( xFileServer *idx, char *name ) {

    xFileContainer *f = NULL;

    name[255] = '\0'; // make sure last character is 

    for (int i = 0; i < idx->file_count; i++) {
        f = idx->files + i;
        if ( strcmp( f->file_name, name ) == 0 ) break;
        f = NULL;
    }

    return f;
}


// -----------------------------------------------------------------------------
// Free a file container
// -----------------------------------------------------------------------------
void xfileserver_free_file(xFileContainer *file) {
    if (!file) return;

    for (uint8_t i = 0; i < file->fragment_count_total; i++) {
        free(file->fragments[i].fragment_bytes);
    }

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

    printf("\n=== FILE STORAGE Debug ===\n");
    printf("Total files: %u\n\n", fs->file_count);

    for (uint16_t i = 0; i < fs->file_count; i++) {
        const xFileContainer *file = &fs->files[i];
        printf("File #%u: '%s'\n", i, file->file_name);
        printf("  ID: %u | Total size: %llu bytes | Fragments: %u\n",
               file->file_id,
               (unsigned long long)file->size,
               file->fragment_count_total);

        // fragments start at 1
        for (uint8_t f = 1; f <= file->fragment_count_total; f++) {
    
            const xFileFragment *frag = NULL;
            int i = 0;
            for(; i < 2; i++){
                if ( file->fragments[i].fragment_id == f ) {
                    frag = file->fragments+i;
                }
            }
   
            if ( frag == NULL ) {
                printf("    Fragment #%3u | [fragment elsewhere]\n", f);
                continue;
            }

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
