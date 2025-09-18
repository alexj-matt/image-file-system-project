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
    int res = ERR_NONE;

    // check if imgfs_file->file is not opened in write mode
    int fd = fileno(imgfs_file->file);
    // get flags
    int mode = fcntl(fd, F_GETFL) & O_ACCMODE;
    if (mode != O_RDWR) {
        return ERR_IO;
    }

    if(imgfs_file->header.nb_files == 0) {
        return ERR_IMAGE_NOT_FOUND;
    }

    // search img and set is_valid = EMPTY
    for (int i = 0; i < imgfs_file->header.max_files; i++) {
        if (strncmp(imgfs_file->metadata[i].img_id, img_id, MAX_IMG_ID) == 0) {
            if (!imgfs_file->metadata[i].is_valid) {    // if img alrdy invalid return error code
                return ERR_IMAGE_NOT_FOUND;
            }
            imgfs_file->metadata[i].is_valid = EMPTY;
            // modify header
            imgfs_file->header.version++;
            imgfs_file->header.nb_files--;
            fseek(imgfs_file->file, 0, SEEK_SET);
            res = fwrite(&imgfs_file->header, sizeof(struct imgfs_header), 1, imgfs_file->file);
            if (res != 1) {
                return ERR_IO;
            }
            fseek(imgfs_file->file,i*sizeof(struct img_metadata),SEEK_CUR);
            res = fwrite(&imgfs_file->metadata[i], sizeof(struct img_metadata), 1, imgfs_file->file);
            if (res != 1) {
                return ERR_IO;
            }
            return ERR_NONE;
        }
    }
    return ERR_IMAGE_NOT_FOUND;
}