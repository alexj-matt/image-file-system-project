#include "imgfs.h"
#include "imgfscmd_functions.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <vips/vips.h>

/*
This function compute, if necessary, the image in a given format.
the structure is as follow:
-elementary check
-
*/


int lazily_resize(int type, struct imgfs_file *imgfs_file, size_t index)
{
    M_REQUIRE_NON_NULL(imgfs_file);
    int ret = 1;
    if (type < 0 || type > 2) {
        return ERR_INVALID_ARGUMENT;
    }
    if (index < 0 || index > imgfs_file->header.max_files || !imgfs_file->metadata[index].is_valid) {
        return ERR_INVALID_IMGID;
    }

    if (type == ORIG_RES) {
        return ERR_NONE;
    }

    VipsImage *orig_image;
    VipsImage *transformed_image;
    size_t len = 0;
    void *output_buffer;
    if (type == SMALL_RES) {
        if (imgfs_file->metadata[index].size[SMALL_RES] == 0) {
            void *buffer = calloc(1, imgfs_file->metadata[index].size[ORIG_RES]);
            ret = fseek(imgfs_file->file, imgfs_file->metadata[index].offset[ORIG_RES], SEEK_SET);
            if (ret) {
                free(buffer);
                g_object_unref(orig_image);
                g_object_unref(transformed_image);
                return ERR_IO;
            }
            ret = fread(buffer, imgfs_file->metadata[index].size[ORIG_RES], 1, imgfs_file->file);
            if (ret != 1) {
                free(buffer);
                g_object_unref(orig_image);
                g_object_unref(transformed_image);
                return ERR_IO;
            }
            int ret_load = vips_jpegload_buffer(buffer, imgfs_file->metadata[index].size[ORIG_RES], &orig_image, NULL);
            int ret_transform = vips_thumbnail_image(orig_image, &transformed_image, imgfs_file->header.resized_res[2], "height", imgfs_file->header.resized_res[3], NULL);
            int ret = vips_jpegsave_buffer(transformed_image, &output_buffer, &len, NULL);
            ret = fseek(imgfs_file->file, 0, SEEK_END);
            if (ret) {
                return ERR_IO;
            }
            ret = fwrite(output_buffer, len, 1, imgfs_file->file);
            if (ret != 1) {
                free(buffer);
                free(output_buffer);
                g_object_unref(orig_image);
                g_object_unref(transformed_image);
                return ERR_IO;
            }
            imgfs_file->metadata[index].offset[SMALL_RES] = ftell(imgfs_file->file) - len;
            imgfs_file->metadata[index].size[SMALL_RES] = len;
            free(buffer);
            g_object_unref(orig_image);
            g_object_unref(transformed_image);
        } else {
            return ERR_NONE;
        }
    }
    if (type == THUMB_RES) {
        if (imgfs_file->metadata[index].size[THUMB_RES] == 0) {
            void *buffer = calloc(1, imgfs_file->metadata[index].size[ORIG_RES]);
            ret = fseek(imgfs_file->file, imgfs_file->metadata[index].offset[ORIG_RES], SEEK_SET);
            if (ret) {
                free(buffer);
                g_object_unref(orig_image);
                g_object_unref(transformed_image);
                return ERR_IO;
            }
            ret = fread(buffer, imgfs_file->metadata[index].size[ORIG_RES], 1, imgfs_file->file);
            if (ret != 1) {
                free(buffer);
                g_object_unref(orig_image);
                g_object_unref(transformed_image);
                return ERR_IO;
            }
            int ret_load = vips_jpegload_buffer(buffer, imgfs_file->metadata[index].size[ORIG_RES], &orig_image, NULL);
            int ret_transform = vips_thumbnail_image(orig_image, &transformed_image, imgfs_file->header.resized_res[0], "height", imgfs_file->header.resized_res[1], NULL);
            int ret = vips_jpegsave_buffer(transformed_image, &output_buffer, &len, NULL);

            imgfs_file->metadata[index].offset[THUMB_RES] = ftell(imgfs_file->file) - len;
            imgfs_file->metadata[index].size[THUMB_RES] = len;
        } else {
            return ERR_NONE;
        }
    }
    free(output_buffer);
    if(fseek(imgfs_file->file, 0, SEEK_SET)) {
        return ERR_IO;
    }
    ret = fwrite(&imgfs_file->header, sizeof(struct imgfs_header), 1, imgfs_file->file);
    if(ret != 1) {
        return ERR_IO;
    }
    ret = fwrite(imgfs_file->metadata, sizeof(struct img_metadata), 1, imgfs_file->file);
    if(ret != 1) {
        return ERR_IO;
    }
    return ERR_NONE;
}
