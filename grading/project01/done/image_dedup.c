#include "image_dedup.h"
#include "string.h"
#include "imgfs.h"
#include "error.h"

int do_name_and_content_dedup(struct imgfs_file* imgfs_file, uint32_t index)
{
    M_REQUIRE_NON_NULL(imgfs_file);
    // get img_ID of image at index
    char* root_imgID;
    if (imgfs_file->header.max_files > index && imgfs_file->metadata[index].is_valid) {
        root_imgID = imgfs_file->metadata[index].img_id;
    } else {
        return ERR_IMAGE_NOT_FOUND;
    }

    // run through all valid images in imgfs_file->metadata and compare img_id
    int found = 0;
    for (int i = 0; i < imgfs_file->header.max_files; i++) {
        // check that only valid entries are considered and index is skipped
        if (imgfs_file->metadata[i].is_valid && i != index) {
            // two entries with same imgID are not accepted
            if (!strncmp(imgfs_file->metadata[i].img_id, root_imgID, MAX_IMG_ID)) {
                return ERR_DUPLICATE_ID;
            }
            if (!strncmp(imgfs_file->metadata[index].SHA, imgfs_file->metadata[i].SHA, SHA256_DIGEST_LENGTH)) {
                found = 1;
                // need to modify offsets and sizes at index
                for (int j = 0; j < NB_RES; j++) {
                    imgfs_file->metadata[index].offset[j] = imgfs_file->metadata[i].offset[j];
                    imgfs_file->metadata[index].size[j] = imgfs_file->metadata[i].size[j];
                }
            }
        }
    }
    // if no duplicate data (SHA different) --> set ORIG_RES offset to 0
    if (!found) {
        imgfs_file->metadata[index].offset[ORIG_RES] = 0;
    }

    return ERR_NONE;
}
