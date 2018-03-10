#include "triangle.h"

#include "CGL/CGL.h"
#include "GL/glew.h"

namespace CGL { namespace StaticScene {

Triangle::Triangle(const Mesh* mesh, size_t v1, size_t v2, size_t v3) :
    mesh(mesh), v1(v1), v2(v2), v3(v3) { }

BBox Triangle::get_bbox() const {

  Vector3D p1(mesh->positions[v1]), p2(mesh->positions[v2]), p3(mesh->positions[v3]);
  BBox bb(p1);
  bb.expand(p2);
  bb.expand(p3);
  return bb;

}

bool Triangle::intersect(const Ray& r) const {
  // Part 1, Task 3: implement ray-triangle intersection
  Vector3D p0(mesh->positions[v1]), p1(mesh->positions[v2]), p2(mesh->positions[v3]);

  //we're finding the ray's t-value at the hit point
  //moller trumbore ////////
  Vector3D e1 = p1 - p0;
  Vector3D e2 = p2 - p0;
  Vector3D s = r.o - p0;
  Vector3D s1 = cross(r.d, e2);
  Vector3D s2 = cross(s, e1);

  double step1x = dot(s2, e2);
  double step1y = dot(s1, s);
  double step1z = dot(s2, r.d);

  Vector3D step1a = Vector3D(step1x, step1y, step1z);

  Vector3D step1b = (1.0/dot(s1, e1)) * step1a;

  double t = step1b.x;
  double b = step1b.y;
  double g = step1b.z;

  double a = (1-b-g);

  Vector3D step2 = a*p0 + b*p1 + g*p2;
  //////////////////////////

  //printf("ABG lol %f %f %f \n", a, b, g);

  if (r.min_t <= t && t <= r.max_t && 0 <= a && a <= 1 && 0 <= b && b <= 1 && 0 <= g && g <= 1) {
    r.max_t = t;
    return true;
  }
  return false;
}

bool Triangle::intersect(const Ray& r, Intersection *isect) const {

  // Part 1, Task 3:
  // implement ray-triangle intersection. When an intersection takes
  // place, the Intersection data should be updated accordingly
  Vector3D p0(mesh->positions[v1]), p1(mesh->positions[v2]), p2(mesh->positions[v3]);
  Vector3D n0(mesh->normals[v1]), n1(mesh->normals[v2]), n2(mesh->normals[v3]);

  //moller trumbore ////////
  Vector3D e1 = p1 - p0;
  Vector3D e2 = p2 - p0;
  Vector3D s = r.o - p0;
  Vector3D s1 = cross(r.d, e2);
  Vector3D s2 = cross(s, e1);

  double step1x = dot(s2, e2);
  double step1y = dot(s1, s);
  double step1z = dot(s2, r.d);

  Vector3D step1a = Vector3D(step1x, step1y, step1z);

  Vector3D step1b = (1.0/dot(s1, e1)) * step1a;

  double t = step1b.x;
  double b = step1b.y;
  double g = step1b.z;

  double a = (1-b-g);

  Vector3D step2 = a*p0 + b*p1 + g*p2;
  //////////////////////////

  //printf("ABG %f %f %f \n", a, b, g);
    /*if (r.min_t <= t && t <= r.max_t) {
        printf("minmax");
    }
    if (0 <= a && a <= 1) {
        printf("a");
    }
    if (0 <= b && b <= 1) {
        printf("b");
    }
    if (0 <= g && g <= 1) {
        printf("c");
    }*/

  if (r.min_t <= t && t <= r.max_t && 0 <= a && a <= 1 && 0 <= b && b <= 1 && 0 <= g && g <= 1) {
    //printf("BARY %f %f %f \n", a, b, g);
    r.max_t = t;
    isect->t = t;
    isect->n = a*n0 + b*n1 + g*n2;
    isect->primitive = this;
    isect->bsdf = this->get_bsdf();
    //printf("intersected");
    return true;
  }
  return false;
}

void Triangle::draw(const Color& c) const {
  glColor4f(c.r, c.g, c.b, c.a);
  glBegin(GL_TRIANGLES);
  glVertex3d(mesh->positions[v1].x,
             mesh->positions[v1].y,
             mesh->positions[v1].z);
  glVertex3d(mesh->positions[v2].x,
             mesh->positions[v2].y,
             mesh->positions[v2].z);
  glVertex3d(mesh->positions[v3].x,
             mesh->positions[v3].y,
             mesh->positions[v3].z);
  glEnd();
}

void Triangle::drawOutline(const Color& c) const {
  glColor4f(c.r, c.g, c.b, c.a);
  glBegin(GL_LINE_LOOP);
  glVertex3d(mesh->positions[v1].x,
             mesh->positions[v1].y,
             mesh->positions[v1].z);
  glVertex3d(mesh->positions[v2].x,
             mesh->positions[v2].y,
             mesh->positions[v2].z);
  glVertex3d(mesh->positions[v3].x,
             mesh->positions[v3].y,
             mesh->positions[v3].z);
  glEnd();
}



} // namespace StaticScene
} // namespace CGL
