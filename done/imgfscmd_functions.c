
/**
 * @file imgfscmd_functions.c
 * @brief imgFS command line interpreter for imgFS core commands.
 *
 * @author Mia Primorac
 */

#define _OPEN_SYS_ITOA_EXT

#include "imgfs.h"
#include "imgfscmd_functions.h"
#include "util.h"   // for _unused
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdio.h>


// default values
static const uint32_t default_max_files = 128;
static const uint16_t default_thumb_res =  64;
static const uint16_t default_small_res = 256;

// max values
static const uint16_t MAX_THUMB_RES = 128;
static const uint16_t MAX_SMALL_RES = 512;
static const uint32_t MAX_NB_FILE = 4294967295;

static void create_name(const char* img_id, int resolution, char** new_name);
static int write_disk_image(const char *filename, const char *image_buffer, uint32_t image_size);
static int read_disk_image(const char *path, char **image_buffer, uint32_t *image_size);

/**********************************************************************
 * Displays some explanations.
 ********************************************************************** */
int help(int useless _unused, char** useless_too _unused)
{
    /* **********************************************************************
     * TODO WEEK 08: WRITE YOUR CODE HERE.
     * **********************************************************************
     */

    printf("imgfscmd [COMMAND] [ARGUMENTS]\n");
    printf("  help: displays this help.\n");
    printf("  list <imgFS_filename>: list imgFS content.\n");
    printf("  create <imgFS_filename> [options]: create a new imgFS.\n");
    printf("      options are:\n");
    printf("          -max_files <MAX_FILES>: maximum number of files.\n");
    printf("                                  default value is %u\n", default_max_files);
    printf("                                  maximum value is %u\n", MAX_NB_FILE);
    printf("          -thumb_res <X_RES> <Y_RES>: resolution for thumbnail images.\n");
    printf("                                  default value is %ux%u\n", default_thumb_res, default_thumb_res);
    printf("                                  maximum value is %ux%u\n", MAX_THUMB_RES, MAX_THUMB_RES);
    printf("          -small_res <X_RES> <Y_RES>: resolution for small images.\n");
    printf("                                  default value is %ux%u\n", default_small_res, default_small_res);
    printf("                                  maximum value is %ux%u\n", MAX_SMALL_RES, MAX_SMALL_RES);
    printf("  read   <imgFS_filename> <imgID> [original|orig|thumbnail|thumb|small]:\n");
    printf("      read an image from the imgFS and save it to a file.\n");
    printf("      default resolution is \"original\".\n");
    printf("  insert <imgFS_filename> <imgID> <filename>: insert a new image in the imgFS.\n");
    printf("  delete <imgFS_filename> <imgID>: delete image imgID from imgFS.\n");

    return ERR_NONE;
}

/**********************************************************************
 * Opens imgFS file and calls do_list().
 ********************************************************************** */
int do_list_cmd(int argc, char** argv)
{
    /* **********************************************************************
     * TODO WEEK 07: WRITE YOUR CODE HERE.
     * **********************************************************************
     */
    M_REQUIRE_NON_NULL(argv);
    if(argc>1) {
        return ERR_INVALID_COMMAND;
    }
    if (argc!=1) {
        return ERR_INVALID_ARGUMENT;
    }

    struct imgfs_file structure;
    int o_res = do_open(argv[0], "rb", &structure);
    if (o_res != ERR_NONE) {
        return o_res;
    }
    int l_res = do_list(&structure, STDOUT, NULL);
    if (l_res != ERR_NONE) {

        return l_res;
    }
    do_close(&structure);
    return ERR_NONE;
}

/**********************************************************************
 * Prepares and calls do_create command.
********************************************************************** */

int do_create_cmd(int argc, char** argv)
{
    M_REQUIRE_NON_NULL(argv);

    if(argc<1) {
        return ERR_NOT_ENOUGH_ARGUMENTS;
    }
    struct imgfs_file imgfs_file;
    // set default header values
    imgfs_file.header.version = 0;
    imgfs_file.header.nb_files = 0;
    imgfs_file.header.max_files = default_max_files;
    imgfs_file.header.resized_res[0] = default_thumb_res;
    imgfs_file.header.resized_res[1] = default_thumb_res;
    imgfs_file.header.resized_res[2] = default_small_res;
    imgfs_file.header.resized_res[3] = default_small_res;

    // check args for header-changing flags
    for(int i = 1; i<argc; i++) {
        if(!strcmp(argv[i], "-max_files")) {
            // minimum 1 value
            if (argc > i + 1) {
                uint32_t mf = atouint32(argv[i+1]);
                imgfs_file.header.max_files = mf;
                i++;    //skip next argument
                continue;
            } else {
                do_close(&imgfs_file);
                return ERR_NOT_ENOUGH_ARGUMENTS;
            }
        }
        if ((!strcmp(argv[i], "-thumb_res"))
            || (!strcmp(argv[i], "-small_res"))) {
            // minimum 2 values
            if (argc > i + 2) {
                uint16_t width = atouint16(argv[i+1]);
                uint16_t height = atouint16(argv[i+2]);
                // thumb_res case
                if (!strcmp(argv[i], "-thumb_res")) {
                    if (width == 0 || width > MAX_THUMB_RES
                        ||  height == 0 || height > MAX_THUMB_RES) {
                        return ERR_RESOLUTIONS;
                    } else {
                        imgfs_file.header.resized_res[0] = width;
                        imgfs_file.header.resized_res[1] = height;
                        i+=2;   //skip next two arguments
                        continue;
                    }
                }
                // small_res case
                if (!strcmp(argv[i], "-small_res")) {
                    if (    width == 0 || width > MAX_SMALL_RES
                            ||  height == 0 || height > MAX_SMALL_RES) {
                        return ERR_RESOLUTIONS;
                    } else {
                        imgfs_file.header.resized_res[2] = width;
                        imgfs_file.header.resized_res[3] = height;
                        i+=2;   //skip next two arguments
                        continue;
                    }
                }
            } else {
                do_close(&imgfs_file);
                return ERR_NOT_ENOUGH_ARGUMENTS;
            }
        } else {
            return ERR_INVALID_ARGUMENT;
        }
    }

    // no need to fopen bcs file is opened in do_create
    int c_res = do_create(argv[0], &imgfs_file);
    if (c_res != ERR_NONE) {
        do_close(&imgfs_file);
        return c_res;
    }
    print_header(&imgfs_file.header);
    do_close(&imgfs_file);
    return ERR_NONE;
}



/**********************************************************************
 * Deletes an image from the imgFS.
 */
int do_delete_cmd(int argc, char** argv)
{
    M_REQUIRE_NON_NULL(argv);

    if (argc < 2) {
        return ERR_NOT_ENOUGH_ARGUMENTS;
    }

    if (argv[1] == NULL || strlen(argv[1]) == 0 || strlen(argv[1]) >= MAX_IMG_ID) {
        return ERR_INVALID_IMGID;
    }

    struct imgfs_file structure;
    int o_res = do_open(argv[0], "rb+", &structure);    // '+' necessary for fwrite
    if (o_res != ERR_NONE) {
        do_close(&structure);
        return o_res;
    }
    int d_res = do_delete(argv[1], &structure);
    do_close(&structure);
    if (d_res != ERR_NONE) {
        return d_res;
    }
    return ERR_NONE;
}

// --- PROVIDED ---
int do_read_cmd(int argc, char **argv)
{
    M_REQUIRE_NON_NULL(argv);
    if (argc != 2 && argc != 3) return ERR_NOT_ENOUGH_ARGUMENTS;

    const char * const img_id = argv[1];

    const int resolution = (argc == 3) ? resolution_atoi(argv[2]) : ORIG_RES;
    if (resolution == -1) return ERR_RESOLUTIONS;

    struct imgfs_file myfile;
    zero_init_var(myfile);
    int error = do_open(argv[0], "rb+", &myfile);
    if (error != ERR_NONE) return error;

    char *image_buffer = NULL;
    uint32_t image_size = 0;
    error = do_read(img_id, resolution, &image_buffer, &image_size, &myfile);
    do_close(&myfile);
    if (error != ERR_NONE) {
        free(image_buffer);
        return error;
    }

    // Extracting to a separate image file.
    char* tmp_name = NULL;
    create_name(img_id, resolution, &tmp_name);
    if (tmp_name == NULL) return ERR_OUT_OF_MEMORY;
    error = write_disk_image(tmp_name, image_buffer, image_size);
    free(tmp_name);
    free(image_buffer);

    return error;
}

int do_insert_cmd(int argc, char **argv)
{
    M_REQUIRE_NON_NULL(argv);
    if (argc != 3) return ERR_NOT_ENOUGH_ARGUMENTS;

    struct imgfs_file myfile;
    zero_init_var(myfile);
    int error = do_open(argv[0], "rb+", &myfile);
    if (error != ERR_NONE) return error;

    char *image_buffer = NULL;
    uint32_t image_size;

    // Reads image from the disk.
    error = read_disk_image (argv[2], &image_buffer, &image_size);
    if (error != ERR_NONE) {
        do_close(&myfile);
        return error;
    }

    error = do_insert(image_buffer, image_size, argv[1], &myfile);
    free(image_buffer);
    do_close(&myfile);
    return error;
}
// ------------------

static void create_name(const char* img_id, int resolution, char** new_name)
{
    if(img_id == NULL || new_name == NULL) {
        return;
    }
    const char* suffix = NULL;
    switch (resolution) {
    case ORIG_RES:
        suffix = "_orig";
        break;
    case SMALL_RES:
        suffix = "_small";
        break;
    case THUMB_RES:
        suffix = "_thumb";
        break;

    default:
        return;
    }
    int n = strlen(img_id) + strlen(suffix) + strlen(".jpg") + 1;
    *new_name = calloc(1, n * sizeof(char));
    if(new_name==NULL) {
        return;
    }
    snprintf(*new_name, n, "%s%s.jpg", img_id, suffix);
}
static int write_disk_image(const char *filename, const char *image_buffer, uint32_t image_size)
{
    FILE* fp = fopen(filename, "wb");
    if(fp==NULL) {
        return ERR_IO;
    }
    int ret = fwrite(image_buffer, image_size, 1, fp);
    if(ret != 1) {
        return ERR_IO;
    }
    return ERR_NONE;
}
static int read_disk_image(const char *path, char **image_buffer, uint32_t *image_size)
{
    FILE* fp = fopen(path, "rb+");
    if(fp==NULL) {
        return ERR_IO;
    }

    // obtain image size
    fseek(fp, 0, SEEK_END);
    *image_size = (uint32_t)ftell(fp);
    fseek(fp, 0L, SEEK_SET);

    *image_buffer = malloc((size_t)*image_size);
    if(image_buffer==NULL) {
        fclose(fp);
        return ERR_OUT_OF_MEMORY;
    }
    size_t ret = fread(*image_buffer, *image_size, 1, fp);
    if(ret!=1) {
        free(*image_buffer);
        fclose(fp);
        return ERR_IO;
    }
    fclose(fp);
    return ERR_NONE;
}