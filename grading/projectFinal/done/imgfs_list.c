#include "imgfs.h"  // for print_header and print_metadata
#include "util.h"   // for "TO_BE_IMPLEMENTED"
#include <unistd.h> // for fcntl
#include <fcntl.h>  // for fcntl
#include <json-c/json.h>
#include <stdio.h>
#include <string.h>


int do_list(const struct imgfs_file* structure, enum do_list_mode output_mode, char** json)
{
    M_REQUIRE_NON_NULL(structure);


    // check if imgfs_file->file is readable
    int fd = fileno(structure->file);
    // get flags-
    int mode = fcntl(fd, F_GETFL) & O_ACCMODE;
    if (!(mode == O_RDONLY || mode == O_RDWR)) {
        return ERR_IO;
    }

    if (output_mode == STDOUT) {
        print_header(&(structure->header));

        if (structure->header.nb_files == 0) {
            printf("<< empty imgFS >>");
        }

        // metadata of invalid images should not be printed
        else {
            for (int i = 0; i < structure->header.max_files; i++) {
                if (structure->metadata[i].is_valid != 0) {
                    print_metadata(&(structure->metadata[i]));
                }
            }
        }
    }
    if (output_mode == JSON) {
        M_REQUIRE_NON_NULL(json);
        struct json_object* json_object  = json_object_new_object();
        if (json_object == NULL) {
            return ERR_RUNTIME;
        }
        struct json_object* json_array = json_object_new_array();
        if (json_array == NULL) {
            json_object_put(json_object);
            return ERR_RUNTIME;
        }
        struct json_object* json_string;
        for (int i = 0; i < structure->header.max_files; i++) {
            if (structure->metadata[i].is_valid != 0) {
                json_object_array_add(json_array, json_object_new_string(structure->metadata[i].img_id));
            }
        }
        json_object_object_add(json_object,"Images",  json_array);
        *json = calloc(structure->header.nb_files * MAX_IMG_ID + 18, sizeof(char));
        if(*json==NULL) {
            return ERR_OUT_OF_MEMORY;
        }
        const char *result = json_object_to_json_string(json_object);
        strncpy(*json, result, strlen(result)*sizeof(char));
        json_object_put(json_object);
    }
    return ERR_NONE;
}
