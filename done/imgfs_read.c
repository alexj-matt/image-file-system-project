#include "error.h"
#include <openssl/sha.h>   // for SHA256_DIGEST_LENGTH
#include <stdint.h>        // for uint32_t, uint64_t
#include <stdio.h>         // for FILE
#include "imgfs.h"
#include "image_content.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h> // for fcntl
#include <fcntl.h>  // for fcntl

int do_read(const char* img_id, int resolution, char** image_buffer, uint32_t* image_size, struct imgfs_file* imgfs_file)
{
    M_REQUIRE_NON_NULL(img_id);
    M_REQUIRE_NON_NULL(imgfs_file);
    M_REQUIRE_NON_NULL(image_buffer);
    M_REQUIRE_NON_NULL(image_size);
    M_REQUIRE_NON_NULL(imgfs_file);

    int ret = 0;
    for (int i = 0; i < imgfs_file->header.max_files; i++) {
        if(!strncmp(imgfs_file->metadata[i].img_id, img_id, MAX_IMG_ID)) {
            if (!(imgfs_file->metadata[i].is_valid)) {
                return ERR_INVALID_IMGID;
            }
            if (imgfs_file->metadata[i].offset[resolution] == NULL || imgfs_file->metadata[i].size[resolution] == NULL) {
                // check if imgfs_file->file is not opened in write mode
                int fd = fileno(imgfs_file->file);
                // get flags
                int mode = fcntl(fd, F_GETFL) & O_ACCMODE;
                if (mode != O_RDWR) {
                    return ERR_IO;
                }
                lazily_resize(resolution, imgfs_file, i);
            }
            *image_buffer = calloc(1, imgfs_file->metadata[i].size[resolution]);
            if (image_buffer == NULL) {
                return ERR_OUT_OF_MEMORY;
            }
            *image_size = imgfs_file->metadata[i].size[resolution];
            ret = fseek(imgfs_file->file, imgfs_file->metadata[i].offset[resolution], SEEK_SET);
            if(ret) {
                free(*image_buffer);
                return ERR_IO;
            }
            ret = fread(*image_buffer, *image_size, 1, imgfs_file->file);
            if(ret != 1) {
                free(*image_buffer);
                return ERR_IO;
            }
            *image_size = imgfs_file->metadata[i].size[resolution];
            return ERR_NONE;
        }
    }
    return ERR_IMAGE_NOT_FOUND;
}