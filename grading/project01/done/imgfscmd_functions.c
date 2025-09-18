
/**
 * @file imgfscmd_functions.c
 * @brief imgFS command line interpreter for imgFS core commands.
 *
 * @author Mia Primorac
 */

#include "imgfs.h"
#include "imgfscmd_functions.h"
#include "util.h"   // for _unused

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

// default values
static const uint32_t default_max_files = 128;
static const uint16_t default_thumb_res =  64;
static const uint16_t default_small_res = 256;

// max values
static const uint16_t MAX_THUMB_RES = 128;
static const uint16_t MAX_SMALL_RES = 512;

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
    printf("                                  default value is 128\n");
    printf("                                  maximum value is 4294967295\n");
    printf("          -thumb_res <X_RES> <Y_RES>: resolution for thumbnail images.\n");
    printf("                                  default value is 64x64\n");
    printf("                                  maximum value is 128x128\n");
    printf("          -small_res <X_RES> <Y_RES>: resolution for small images.\n");
    printf("                                  default value is 256x256\n");
    printf("                                  maximum value is 512x512\n");
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
    /* **********************************************************************
     * TODO WEEK 08: WRITE YOUR CODE HERE (and change the return if needed).
     * **********************************************************************
     */

    // called by for example:
    //      ./imgfscmd create my_new_fs -thumb_res 128 128
    // check result by:
    //      ./imgfscmd list my_new_fs

    M_REQUIRE_NON_NULL(argv);

    if(argc<1) {
        return ERR_NOT_ENOUGH_ARGUMENTS;
    }
    struct imgfs_file imgfs_file;
    // set default header values
    imgfs_file.header.version = 0;
    imgfs_file.header.nb_files = 0;
    // imgfs_file.header.unused_32 = 0; // usage unclear
    // imgfs_file.header.unused_64 = 0; // usage unclear
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
                // if (mf == 0)
                //     return ERR_MAX_FILES;
                // }
                imgfs_file.header.max_files = mf;
                i++;                                                               //skip next argument
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
                        i+=2;                                                       //skip next two arguments
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
                        i+=2;                                                       //skip next two arguments
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
    do_close(&imgfs_file);
    return ERR_NONE;
}



/**********************************************************************
 * Deletes an image from the imgFS.
 */
int do_delete_cmd(int argc, char** argv)
{
    /* **********************************************************************
     * TODO WEEK 08: WRITE YOUR CODE HERE (and change the return if needed).
     * **********************************************************************
     */

    // assuming argv[0] is path to imgfs_file
    // assuming argv[1] is img_id
    // ex:                      ./imgfscmd delete ../provided/tests/data/test02.imgfs pic1
    // at this point argv++:                      ../provided/tests/data/test02.imgfs pic1

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
