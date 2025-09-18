#include "imgfs.h"
#include "error.h"
#include "string.h"
#include "stdio.h"
#include <unistd.h> // for fcntl
#include <fcntl.h>  // for fcntl

int do_delete(const char* img_id, struct imgfs_file* imgfs_file)
{
    M_REQUIRE_NON_NULL(imgfs_file);
    M_REQUIRE_NON_NULL(img_id);
    int res = 0;

    if (imgfs_file == NULL || imgfs_file->metadata == NULL) {
        return ERR_INVALID_ARGUMENT;
    }

    // check if imgfs_file->file is not opened in write mode
    int fd = fileno(imgfs_file->file);
    // get flags
    int mode = fcntl(fd, F_GETFL) & O_ACCMODE;
    if (mode != O_RDWR) {
        return ERR_IO;
    }

    // search img and set is_valid = EMPTY
    int found = 0;
    for (int i = 0; i < imgfs_file->header.max_files; i++) {
        if (strncmp(imgfs_file->metadata[i].img_id, img_id, MAX_IMG_ID) == 0) {
            if (!imgfs_file->metadata[i].is_valid) {    // if img alrdy invalid return error code
                return ERR_IMAGE_NOT_FOUND;
            }
            imgfs_file->metadata[i].is_valid = EMPTY;
            found = 1;
            // modify header
            imgfs_file->header.version++;
            imgfs_file->header.nb_files--;
            break;
        }
    }
    if (!found) {
        return ERR_IMAGE_NOT_FOUND;
    }

    // prepare write to disk
    rewind(imgfs_file->file);
    // check that offset really is 0
    if (ftell(imgfs_file->file) != 0) {
        return ERR_IO;
    }

    // write to disk
    res = fwrite(&imgfs_file->header, sizeof(struct imgfs_header), 1, imgfs_file->file);

    if (ftell(imgfs_file->file) != sizeof(imgfs_file->header)   // check that offset advanced --> append metadata, not overwrite header
        || res != 1) {        // sizeof(imgfs_file->header)
        return ERR_IO;
    }
    res = fwrite(imgfs_file->metadata, sizeof(struct img_metadata), imgfs_file->header.max_files, imgfs_file->file);
    if (res != imgfs_file->header.max_files) {
        return ERR_IO;
    }

    return ERR_NONE;
}
