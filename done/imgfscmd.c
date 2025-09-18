/**
 * @file imgfscmd.c
 * @brief imgFS command line interpreter for imgFS core commands.
 *
 * Image Filesystem Command Line Tool
 *
 * @author Mia Primorac
 */

#include "imgfs.h"
#include "imgfscmd_functions.h"
#include "util.h"   // for _unused

#include <stdlib.h>
#include <string.h>
#include <vips/vips.h>

#define NB_CMDS 6   // to be changed if command is added to "commands"

typedef int (*command)(int, char**);

struct command_mapping {
    const char * name;
    command com;
};

struct command_mapping commands[] = {{"list", do_list_cmd}, {"create", do_create_cmd}, {"help", help}, {"delete", do_delete_cmd}, {"insert", do_insert_cmd}, {"read", do_read_cmd}};

/*******************************************************************************
 * MAIN
 */
int main(int argc, char* argv[])
{
    VIPS_INIT(argv[0]);
    int ret = 0;
    if (argc < 2) {
        ret = ERR_NOT_ENOUGH_ARGUMENTS;
    } else {
        argc--; argv++; // skips command call name

        int found = 0;
        for (int i = 0; i < NB_CMDS; i++) {
            if (!strcmp(argv[0], commands[i].name)) {
                // as soon as cmd is found skips cmd name
                argc--; argv++;
                ret = commands[i].com(argc, argv);
                found = 1;
                break;
            }
        }
        if (found == 0) {
            ret = ERR_INVALID_COMMAND;
        }
    }

    if (ret) {
        fprintf(stderr, "ERROR: %s\n", ERR_MSG(ret));
        help(argc, argv);
    }
    vips_shutdown();
    return ret;
}
