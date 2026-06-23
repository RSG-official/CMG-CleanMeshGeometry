/*  cmg_example.c  —  minimal CMG loading example
 *
 *  Build (Windows MSVC):
 *      cl cmg_example.c /Fe:cmg_example.exe
 *
 *  Build (GCC / Clang):
 *      gcc -o cmg_example cmg_example.c
 *      clang -o cmg_example cmg_example.c
 *
 *  Usage:
 *      cmg_example model.cmg
 */

#define CMG_IMPLEMENTATION
#include "cmg.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file.cmg>\n", argv[0]);
        return 1;
    }

    char err[512];
    CMGMesh *mesh = cmg_load(argv[1], err, sizeof(err));
    if (!mesh) {
        fprintf(stderr, "CMG load failed: %s\n", err);
        return 1;
    }

    printf("CMG format version : %u\n", cmg_format_version());
    printf("Vertices           : %u\n", mesh->vertex_count);
    printf("Triangles          : %u\n", mesh->triangle_count);

    if (mesh->has_bbox) {
        printf("Bounding box min   : (%.4f, %.4f, %.4f)\n",
               mesh->bbox_min[0], mesh->bbox_min[1], mesh->bbox_min[2]);
        printf("Bounding box max   : (%.4f, %.4f, %.4f)\n",
               mesh->bbox_max[0], mesh->bbox_max[1], mesh->bbox_max[2]);
    } else {
        printf("Bounding box       : not present\n");
    }

    if (mesh->sidecar_name[0] != '\0') {
        printf("Sidecar (.cmgex)   : %s\n", mesh->sidecar_name);
    }

    /* Print the first 3 vertices and first triangle as a sanity check */
    uint32_t preview_verts = mesh->vertex_count < 3 ? mesh->vertex_count : 3;
    printf("\nFirst %u vertex/vertices:\n", preview_verts);
    for (uint32_t i = 0; i < preview_verts; i++) {
        CMGVertex v = mesh->vertices[i];
        printf("  [%u] pos=(%.4f, %.4f, %.4f)  normal=(%.4f, %.4f, %.4f)\n",
               i, v.px, v.py, v.pz, v.nx, v.ny, v.nz);
    }

    if (mesh->triangle_count > 0) {
        printf("\nFirst triangle indices: %u, %u, %u\n",
               mesh->indices[0], mesh->indices[1], mesh->indices[2]);
    }

    cmg_free(mesh);
    return 0;
}
