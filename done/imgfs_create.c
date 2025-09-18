#include <stdio.h>         // for FILE
#include "imgfs.h"
#include "util.h"
#include <stdlib.h>        // for calloc
#include "error.h"

// for example call through "./imgfscmd create my_new_fs"
// imgfs_file arg has "max_files" and "resized_res" already set
int do_create(const char* imgfs_filename, struct imgfs_file* imgfs_file)
{
    int res = 0;
    M_REQUIRE_NON_NULL(imgfs_filename);
    M_REQUIRE_NON_NULL(imgfs_file);
    strcpy(imgfs_file->header.name, CAT_TXT);
    imgfs_file->file = fopen(imgfs_filename, "wb");
    if(imgfs_file->file == NULL) {
        return ERR_INVALID_FILENAME;
    }
    imgfs_file->header.version = 0;
    imgfs_file->header.nb_files = 0;
    imgfs_file->metadata = calloc(imgfs_file->header.max_files, sizeof(struct img_metadata));
    if(imgfs_file->metadata == NULL) {
        return ERR_OUT_OF_MEMORY;
    }
    res = fwrite(&imgfs_file->header, sizeof(struct imgfs_header), 1, imgfs_file->file);
    if(res != 1) {
        free(imgfs_file->metadata);
        imgfs_file->metadata = NULL;
        return ERR_IO;
    }
    if(imgfs_file->header.max_files<0) {
        return ERR_MAX_FILES;
    }
    res = fwrite(imgfs_file->metadata, sizeof(struct img_metadata), imgfs_file->header.max_files, imgfs_file->file);
    if(res != imgfs_file->header.max_files) {
        free(imgfs_file->metadata);
        imgfs_file->metadata = NULL;
        return ERR_IO;
    }


    printf("%u item(s) written\n", imgfs_file->header.max_files + 1);
    return ERR_NONE;
}