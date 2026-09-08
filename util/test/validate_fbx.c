// Validate RenderDoc's synthetic FBX fixture using the independent, pinned ufbx reader.
#include <math.h>
#include <stdio.h>
#include "ufbx.h"

#define CHECK(x)                                          \
  do                                                      \
  {                                                       \
    if(!(x))                                              \
    {                                                     \
      fprintf(stderr, "FBX validation failed: %s\n", #x); \
      ufbx_free_scene(scene);                             \
      return 1;                                           \
    }                                                     \
  } while(0)

int main(int argc, char **argv)
{
  if(argc != 2)
    return 2;
  ufbx_error error;
  ufbx_scene *scene = ufbx_load_file(argv[1], NULL, &error);
  if(!scene)
  {
    fprintf(stderr, "Cannot read FBX: %s\n", error.description.data);
    return 1;
  }
  CHECK(scene->meshes.count == 1);
  ufbx_mesh *mesh = scene->meshes.data[0];
  CHECK(mesh->num_vertices == 3 && mesh->num_faces == 1 && mesh->num_indices == 3);
  CHECK(mesh->vertex_normal.exists && mesh->vertex_tangent.exists && mesh->vertex_color.exists);
  CHECK(mesh->uv_sets.count == 3);
  const double expected[3][3] = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
  for(size_t i = 0; i < 3; i++)
  {
    ufbx_vec3 p = ufbx_get_vertex_vec3(&mesh->vertex_position, i);
    CHECK(fabs(p.x - expected[i][0]) < 1e-9 && fabs(p.y - expected[i][1]) < 1e-9 && fabs(p.z) < 1e-9);
    ufbx_vec3 n = ufbx_get_vertex_vec3(&mesh->vertex_normal, i);
    CHECK(fabs(n.x) < 1e-9 && fabs(n.y) < 1e-9 && fabs(n.z - 1.0) < 1e-9);
    ufbx_vec3 t = ufbx_get_vertex_vec3(&mesh->vertex_tangent, i);
    CHECK(fabs(t.x - 1.0) < 1e-9 && fabs(t.y) < 1e-9 && fabs(t.z) < 1e-9);
    ufbx_vec4 c = ufbx_get_vertex_vec4(&mesh->vertex_color, i);
    CHECK(fabs(c.w - 1.0) < 1e-9);
    CHECK(fabs(c.x - (i == 0)) < 1e-9 && fabs(c.y - (i == 1)) < 1e-9 && fabs(c.z - (i == 2)) < 1e-9);
    for(size_t uv = 0; uv < 3; uv++)
    {
      ufbx_vec2 v = ufbx_get_vertex_vec2(&mesh->uv_sets.data[uv].vertex_uv, i);
      CHECK(fabs(v.x - expected[i][0]) < 1e-9 && fabs(v.y - expected[i][1]) < 1e-9);
    }
  }
  ufbx_free_scene(scene);
  puts(
      "Independent FBX validation passed: topology, positions, normals, tangents, color and 3 UV "
      "sets.");
  return 0;
}
