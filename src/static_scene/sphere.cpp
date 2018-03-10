#include "sphere.h"

#include <cmath>

#include  "../bsdf.h"
#include "../misc/sphere_drawing.h"

namespace CGL { namespace StaticScene {

bool Sphere::test(const Ray& r, double& t1, double& t2) const {

  // TODO (Part 1.4):
  // Implement ray - sphere intersection test.
  // Return true if there are intersections and writing the
  // smaller of the two intersection times in t1 and the larger in t2.


  //printf("r squared %f \n", r2);
  double a = dot(r.d, r.d);
  double b = dot((2.0 * (r.o - this->o)), r.d);
  double c = dot((r.o - this->o), (r.o - this->o)) - (this->r2);
  //printf("ABC %f %f %f \n", a, b, c);

  //quadratic formula

  //discriminant: test whether there are real solutions
  double disc = (b*b)-(4*a*c);

  if (disc > 0) {
    double i1 = ((0 - b) + sqrt(disc)) / (2.0*a);
    double i2 = ((0 - b) - sqrt(disc)) / (2.0*a);
    //printf("I1I2 %f %f \n", i1, i2);
    t1 = min(i1, i2);
    t2 = max(i1, i2);
    return true;
  }
  else if (disc == 0) {
    double i1 = ((0 - b) / (2.0*a));
    t1 = i1;
    t2 = i1;
    return true;
  }
  else {
    return false;
  }


  /*if (r.min_t < i1 < r.max_t) {
    if (r.min_t < i2 < r.max_t) {
      t1 = min(i1, i2);
      t2 = max(i1, i2);
      r.max_t = t2;
    }
    else {
      t1 = i1;
      r.max_t = t1;
    }
    return true;
  }
  else if (r.min_t < i2 < r.max_t) {
    t1 = i2;
    r.max_t = t1;
    return true;
  }
  return false;*/
}

bool Sphere::intersect(const Ray& r) const {

  // TODO (Part 1.4):
  // Implement ray - sphere intersection.
  // Note that you might want to use the the Sphere::test helper here.
  double t1, t2;
  //values of t1, t2 should be overwritten here

  if (test(r, t1, t2)) {
    //printf("T1T2 %f %f \n", t1, t2);
    if (r.min_t <= t1 && t1 <= r.max_t) {
      r.max_t = t1;
    }
    else if (r.min_t <= t2 && t2 <= r.max_t) {
      r.max_t = t2;
    }
    return true;
  }
  return false;
}

bool Sphere::intersect(const Ray& r, Intersection *i) const {

  // TODO (Part 1.4):
  // Implement ray - sphere intersection.
  // Note again that you might want to use the the Sphere::test helper here.
  // When an intersection takes place, the Intersection data should be updated
  // correspondingly.

  double t1, t2;
  //values of t1, t2 should be overwritten here
  if (test(r, t1, t2)) {
    //printf("T1T2 %f %f \n", t1, t2);
    //leq or strictly less??

    if (t1 < 0) {
      t1 = t2;
    }
    if (t1 < 0) {
      return false;
    }
    //know t1 is first valid time
    if (r.min_t <= t1 && t1 <= r.max_t) {
      r.max_t = t1;
    }

    i->t = t1;
    Vector3D p = r.o + t1 * r.d;
    i->n = (p - this->o).unit();
    //printf("normal %f \n", i->n.x);
    i->primitive = this;
    i->bsdf = this->get_bsdf();
    return true;
  }
  return false;


  /*double a = r.d * r.d;
  double b = 2 * (r.o - this->o) * r.d;
  double c = (r.o - this->o) * (r.o - this->o) - (this->r2);

  //quadratic formula
  double t1 = ((0 - b) + sqrt((b*b)-(4*a*c))) / (2*a);
  double t2 = ((0 - b) - sqrt((b*b)-(4*a*c))) / (2*a);

  if (!(r.min_t < t1 < r.max_t || r.min_t < t2 < r.max_t)) {
    return false;
  }
  else if (r.min_t < t1 < r.max_t) {
    if (r.min_t < t2 < r.max_t) {
      isect->t = min(t1, t2);
    }
    else {
      isect->t = t1;
    }
  }
  else if (r.min_t < t2 < r.max_t) {
    isect->t = t2;
  }
  isect->primitive = this;
  isect->bsdf = this->get_bsdf();
  return true;*/
}

void Sphere::draw(const Color& c) const {
  Misc::draw_sphere_opengl(o, r, c);
}

void Sphere::drawOutline(const Color& c) const {
    //Misc::draw_sphere_opengl(o, r, c);
}


} // namespace StaticScene
} // namespace CGL
