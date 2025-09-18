#include <stdio.h>
#include <openssl/sha.h>    // for SHA256()
#include "imgfs.h"
#include "image_content.h"  // for get_resolution()
#include <unistd.h> // for fcntl
#include <fcntl.h>  // for fcntl
#include <string.h> // for strncpy

/**
 * @brief Insert image in the imgFS file
 *
 * @param buffer Pointer to the raw image content
 * @param size Image size
 * @param img_id Image ID
 * @return Some error code. 0 if no error.
 */


int do_insert(const char* image_buffer, size_t image_size, const char* img_id, struct imgfs_file* imgfs_file)
{

    M_REQUIRE_NON_NULL(imgfs_file);
    M_REQUIRE_NON_NULL(image_buffer);
    M_REQUIRE_NON_NULL(img_id);

    // check if imgfs_file->file is not opened in write mode
    int fd = fileno(imgfs_file->file);
    // get flags
    int mode = fcntl(fd, F_GETFL) & O_ACCMODE;
    if (mode != O_RDWR) {
        return ERR_IO;
    }

    if (imgfs_file->header.nb_files >= imgfs_file->header.max_files) {
        return ERR_IMGFS_FULL;
    }

    for (int i = 0; i < imgfs_file->header.max_files; i++) {
        if (!(imgfs_file->metadata[i].is_valid)) {

            // set metadata[i] to 0
            memset(&imgfs_file->metadata[i], 0, sizeof(struct img_metadata));
            SHA256(image_buffer, image_size, imgfs_file->metadata[i].SHA);
            strncpy(imgfs_file->metadata[i].img_id, img_id, MAX_IMG_ID);

            imgfs_file->metadata[i].size[ORIG_RES] = (uint32_t)image_size;

            uint32_t h;
            uint32_t w;
            int res = get_resolution(&h, &w, image_buffer, image_size);
            if (res) {
                return ERR_IMGLIB;
            }
            imgfs_file->metadata[i].orig_res[0] = w;
            imgfs_file->metadata[i].orig_res[1] = h;

            res = do_name_and_content_dedup(imgfs_file, i);

            if (res) {
                return res;
            }

            // check if dedup has NOT found another copy --> in that case need to write image_buffer and update offset
            if (imgfs_file->metadata[i].offset[ORIG_RES] == 0) {
                // write image contents (not metadata) to the end of file
                res = fseek(imgfs_file->file, 0, SEEK_END);
                if (res) {
                    return ERR_IO;
                }
                uint64_t offset_ = (uint64_t)ftell(imgfs_file->file);

                res = fwrite(image_buffer, image_size, 1, imgfs_file->file);
                if (res != 1) {
                    return ERR_IO;
                }

                imgfs_file->metadata[i].offset[ORIG_RES] = offset_;
            }

            // finalize metadata
            imgfs_file->metadata[i].is_valid = NON_EMPTY;

            // update header data
            imgfs_file->header.nb_files++;
            imgfs_file->header.version++;

            // write to disk (header-metadatas-imagecontent)
            res = fseek(imgfs_file->file, 0, SEEK_SET);
            if (res) {
                return ERR_IO;
            }
            res = fwrite(&imgfs_file->header, sizeof(struct imgfs_header), 1, imgfs_file->file);
            if (res != 1) {
                return ERR_IO;
            }
            // "your code must not write all the metadata to disk for each operation!" --> only write modified metadata
            res = fseek(imgfs_file->file, i * sizeof(struct img_metadata), SEEK_CUR);
            if (res) {
                int res2 = decr_header(imgfs_file);
                if (res2) {
                    return res2;
                }
                return ERR_IO;
            }
            res = fwrite(&imgfs_file->metadata[i], sizeof(struct img_metadata), 1, imgfs_file->file);
            if (res != 1) {
                int res2 = decr_header(imgfs_file);
                if (res2) {
                    return res2;
                }
                return ERR_IO;
            }

            return ERR_NONE;
        }
    }
    return ERR_IMGFS_FULL;
}

// decrement nb_files and version in header and write to file
int decr_header(struct imgfs_file* imgfs_file)
{
    imgfs_file->header.version--;
    imgfs_file->header.nb_files--;
    int res = fseek(imgfs_file->file, 0, SEEK_SET);
    if (res) {
        return ERR_IO;
    }
    res = fwrite(&imgfs_file->header, sizeof(struct imgfs_header), 1, imgfs_file->file);
    if (res != 1) {
        return ERR_IO;
    }

    return ERR_NONE;
}