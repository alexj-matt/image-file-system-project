#include "imgfs.h"  // for print_header and print_metadata
#include "util.h"   // for "TO_BE_IMPLEMENTED"
#include <unistd.h> // for fcntl
#include <fcntl.h>  // for fcntl


int do_list(const struct imgfs_file* structure, enum do_list_mode output_mode, char** json)
{
    M_REQUIRE_NON_NULL(structure);

    // check if imgfs_file->file is readable
    int fd = fileno(structure->file);
    // get flags
    int mode = fcntl(fd, F_GETFL) & O_ACCMODE;
    if (!(mode == O_RDONLY || mode == O_RDWR)) {
        return ERR_IO;
    }

    if (output_mode == STDOUT) {
        print_header(&(structure->header));

        if (structure->header.nb_files == 0) {
            printf("<< empty imgFS >>");
        }

        // assumption that metadata of invalid images should not be printed (correct)
        else {
            for (int i = 0; i < structure->header.max_files; i++) {
                if (structure->metadata[i].is_valid != 0) {
                    print_metadata(&(structure->metadata[i]));
                }
            }
        }
    }
    if (output_mode == JSON) {
        TO_BE_IMPLEMENTED();
    }
    return ERR_NONE;
}
