#include "imgfs.h"
#include "imgfscmd_functions.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <vips/vips.h>

// "You are responsible for freeing the buffer with g_free() when you are done with it."
// -->  when calling vips_jpegsave_buffer(transformed_image, &output_buffer, &len, NULL)
//      output_buffer points to a memory address that was allocated by vips_jpegsave_buffer (not the same as pointed to by transformed_image)
//      --> free this memory with g_free(output_buffer);
int lazily_resize(int type, struct imgfs_file *imgfs_file, size_t index)
{
    M_REQUIRE_NON_NULL(imgfs_file);
    int ret = 1;
    if (type < 0 || type > 2) {
        return ERR_RESOLUTIONS;
    }
    if (index < 0 || index > imgfs_file->header.max_files || !imgfs_file->metadata[index].is_valid) {
        return ERR_INVALID_IMGID;
    }

    if (type == ORIG_RES) {
        return ERR_NONE;
    }

    int orig_img_size = imgfs_file->metadata[index].size[ORIG_RES];
    VipsImage *orig_image = NULL;
    VipsImage *transformed_image = NULL;
    size_t len = 0;
    void* output_buffer = NULL;
    if (type == SMALL_RES) {
        if (imgfs_file->metadata[index].size[SMALL_RES] == 0) {
            void *buffer = calloc(1, orig_img_size);
            if(buffer==NULL) {
                if (orig_image) {
                    g_object_unref(orig_image);
                }
                if (transformed_image) {
                    g_object_unref(transformed_image);
                }
                return ERR_OUT_OF_MEMORY;
            }
            ret = fseek(imgfs_file->file, imgfs_file->metadata[index].offset[ORIG_RES], SEEK_SET);
            if (ret) {
                free(buffer);
                goto clean;
            }
            ret = fread(buffer, orig_img_size, 1, imgfs_file->file);
            if (ret != 1) {
                free(buffer);
                goto clean;
            }
            ret = vips_jpegload_buffer(buffer, orig_img_size, &orig_image, NULL);
            if (ret) {
                free(buffer);
                goto clean;
            }
            ret = vips_thumbnail_image(orig_image, &transformed_image, imgfs_file->header.resized_res[2], "height", imgfs_file->header.resized_res[3], NULL);
            if (ret) {
                free(buffer);
                goto clean;
            }
            ret = vips_jpegsave_buffer(transformed_image, &output_buffer, &len, NULL);
            if (ret) {
                free(buffer);
                goto clean;
            }
            ret = fseek(imgfs_file->file, 0, SEEK_END);
            if (ret) {
                free(buffer);
                g_free(output_buffer);
                goto clean;
            }
            ret = fwrite(output_buffer, len, 1, imgfs_file->file);
            if (ret != 1) {
                free(buffer);
                g_free(output_buffer);
                goto clean;
            }
            imgfs_file->metadata[index].offset[SMALL_RES] = ftell(imgfs_file->file) - len;
            imgfs_file->metadata[index].size[SMALL_RES] = len;
            free(buffer);
            g_free(output_buffer);
            if (orig_image) {
                g_object_unref(orig_image);
            }
            if (transformed_image) {
                g_object_unref(transformed_image);
            }
        } else {
            return ERR_NONE;
        }
    }
    if (type == THUMB_RES) {
        if (imgfs_file->metadata[index].size[THUMB_RES] == 0) {
            void *buffer = calloc(1, orig_img_size);
            if(buffer==NULL) {
                if (orig_image) {
                    g_object_unref(orig_image);
                }
                if (transformed_image) {
                    g_object_unref(transformed_image);
                }
                return ERR_OUT_OF_MEMORY;
            }
            ret = fseek(imgfs_file->file, imgfs_file->metadata[index].offset[ORIG_RES], SEEK_SET);
            if (ret) {
                free(buffer);
                goto clean;
            }
            ret = fread(buffer, orig_img_size, 1, imgfs_file->file);
            if (ret != 1) {
                free(buffer);
                goto clean;
            }
            ret = vips_jpegload_buffer(buffer, orig_img_size, &orig_image, NULL);
            if (ret) {
                free(buffer);
                goto clean;
            }
            ret = vips_thumbnail_image(orig_image, &transformed_image, imgfs_file->header.resized_res[0], "height", imgfs_file->header.resized_res[1], NULL);
            if (ret) {
                free(buffer);
                goto clean;
            }
            ret = vips_jpegsave_buffer(transformed_image, &output_buffer, &len, NULL);
            if (ret) {
                free(buffer);
                goto clean;
            }
            ret = fseek(imgfs_file->file, 0, SEEK_END);
            if (ret) {
                free(buffer);
                g_free(output_buffer);
                goto clean;
            }
            ret = fwrite(output_buffer, len, 1, imgfs_file->file);
            if (ret != 1) {
                free(buffer);
                g_free(output_buffer);
                goto clean;
            }
            imgfs_file->metadata[index].offset[THUMB_RES] = ftell(imgfs_file->file) - len;
            imgfs_file->metadata[index].size[THUMB_RES] = len;
            free(buffer);
            g_free(output_buffer);
            if (orig_image) {
                g_object_unref(orig_image);
            }
            if (transformed_image) {
                g_object_unref(transformed_image);
            }
        } else {
            return ERR_NONE;
        }
    }
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

clean:
    if (orig_image) {
        g_object_unref(orig_image);
    }
    if (transformed_image) {
        g_object_unref(transformed_image);
    }
    return ERR_IO;
}

// --- PROVIDED ---
int get_resolution(uint32_t *height, uint32_t *width,
                   const char *image_buffer, size_t image_size)
{
    M_REQUIRE_NON_NULL(height);
    M_REQUIRE_NON_NULL(width);
    M_REQUIRE_NON_NULL(image_buffer);

    VipsImage* original = NULL;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
    const int err = vips_jpegload_buffer((void*) image_buffer, image_size,
                                         &original, NULL);
#pragma GCC diagnostic pop
    if (err != ERR_NONE) return ERR_IMGLIB;

    *height = (uint32_t) vips_image_get_height(original);
    *width  = (uint32_t) vips_image_get_width (original);

    g_object_unref(VIPS_OBJECT(original));
    return ERR_NONE;
}
// -----------------
