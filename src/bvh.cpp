#include "bvh.h"

#include "CGL/CGL.h"
#include "static_scene/triangle.h"

#include <iostream>
#include <stack>

using namespace std;

namespace CGL { namespace StaticScene {

BVHAccel::BVHAccel(const std::vector<Primitive *> &_primitives,
                   size_t max_leaf_size) {

  root = construct_bvh(_primitives, max_leaf_size);

}

BVHAccel::~BVHAccel() {
  if (root) delete root;
}

BBox BVHAccel::get_bbox() const {
  return root->bb;
}

void BVHAccel::draw(BVHNode *node, const Color& c) const {
  if (node->isLeaf()) {
    for (Primitive *p : *(node->prims))
      p->draw(c);
  } else {
    draw(node->l, c);
    draw(node->r, c);
  }
}

void BVHAccel::drawOutline(BVHNode *node, const Color& c) const {
  if (node->isLeaf()) {
    for (Primitive *p : *(node->prims))
      p->drawOutline(c);
  } else {
    drawOutline(node->l, c);
    drawOutline(node->r, c);
  }
}

BVHNode *BVHAccel::construct_bvh(const std::vector<Primitive*>& prims, size_t max_leaf_size) {

  // TODO (Part 2.1):
  // Construct a BVH from the given vector of primitives and maximum leaf
  // size configuration. The starter code build a BVH aggregate with a
  // single leaf node (which is also the root) that encloses all the
  // primitives.

  //printf("\n is this the issue \n");

  BBox bbox, centroid_box;

  BVHNode *node = new BVHNode(bbox);
    //printf("\n start \n");

  for (Primitive *p : prims) {
    BBox bb = p->get_bbox();
    bbox.expand(bb);
    Vector3D c = bbox.centroid();
    centroid_box.expand(c);
  }
    //printf("\n sup \n");
    node->bb = bbox;
    //printf("bbox %f \n", bbox.max.x);
  /*printf("\n hey \n");
    //reached leaf
  printf("\n %lu \n", prims.size());
  printf("\n %zu \n", max_leaf_size);*/
    if(prims.size() <= max_leaf_size) {
      node->prims = new vector<Primitive *>(prims);
        //printf("\n leaf \n");
      return node;
    }

    int axis;
    if (centroid_box.extent.x >= centroid_box.extent.y && centroid_box.extent.x >= centroid_box.extent.z) {
      axis = 0;
    }
    else if (centroid_box.extent.y >= centroid_box.extent.x && centroid_box.extent.y >= centroid_box.extent.z) {
      axis = 1;
    }
    else {
      axis = 2;
    }

    double splitCoord = centroid_box.centroid()[axis];
    double sctest = splitCoord;

    vector<Primitive *> left;
    vector<Primitive *> right;

    for (Primitive *p : prims){
      if (p->get_bbox().centroid()[axis] < splitCoord) {
        left.push_back(p);
      }
      //greater or equal
      else {
        right.push_back(p);
      }
    }

    int counter = 0;
    double newRight = bbox.max[axis];
    double newLeft = bbox.min[axis];

    while ((left.empty() || right.empty())) {
        //printf("\nsplit 2 %f \n", sctest);
      counter++;
        /*if (left.empty() && right.empty()) {
            printf("\n yayayay \n");
        }*/
      //printf("\n %lu \n", left.size());
      //printf("\n %lu \n", right.size());

        //printf("split \n %f \n", splitCoord);
        //printf("max \n %f \n", bbox.max[axis]);
        //printf("min \n %f \n", bbox.min[axis]);
      if (left.empty() & !right.empty()) {
        newLeft = splitCoord;
        splitCoord = (newRight + splitCoord)/2.0;

        //printf("\nsplit left %f \n", splitCoord);
      }
      else if (right.empty() & !left.empty()) {
        newRight = splitCoord;
        splitCoord = (newLeft + splitCoord)/2.0;
        //splitCoord = splitCoord + (splitCoord/ 2.0);
        //printf("\nsplit right %f \n", splitCoord);
      }
      left.clear();
      right.clear();
      for (Primitive *p : prims){
        if (p->get_bbox().centroid()[axis] < splitCoord) {
          left.push_back(p);
        }
        //greater or equal
        else {
          right.push_back(p);
        }
      }
    }
    //printf("\n left \n");
    node->l = construct_bvh(left, max_leaf_size);
    //printf("\n right\n");
    node->r = construct_bvh(right, max_leaf_size);
    //printf("\n done \n");
  return node;
}


bool BVHAccel::intersect(const Ray& ray, BVHNode *node) const {
  // TODO (Part 2.3):
  // Fill in the intersect function.
  // Take note that this function has a short-circuit that the
  // Intersection version cannot, since it returns as soon as it finds
  // a hit, it doesn't actually have to find the closest hit.

  //printf("\n intersect 1 \n");

    double tmin = ray.min_t; //0??
    double tmax = ray.max_t;
  if (!node->bb.intersect(ray, tmin, tmax)) {
    return false;
  }

  if (node->isLeaf()) {
    for (Primitive *p : *node->prims) {
      if (p->intersect(ray)) {
        return true;
      }
    }
      return false;
  }
  bool hit1 = intersect(ray, node->r);
  bool hit2 = intersect(ray, node->l);

  return hit1 || hit2;
}

bool BVHAccel::intersect(const Ray& ray, Intersection* i, BVHNode *node) const {
  // TODO (Part 2.3):
  // Fill in the intersect function.

  double tmin = ray.min_t; //0??
  double tmax = ray.max_t;
  /*printf("node bbox minx %f \n", node->bb.min.x);
  printf("node bbox miny %f \n", node->bb.min.y);
  printf("node bbox maxx %f \n", node->bb.max.x);
  printf("node bbox maxy %f \n", node->bb.max.y);*/

  //test
  /*bool didIntersect = false;
  for (Primitive *p : *node->prims) {
    if (p->intersect(ray, i)) {
      didIntersect = true;
    }
  }*/



  if (!(node->bb.intersect(ray, tmin, tmax))) {
      //printf("oi");
    return false;
  }
  //Primitive closest;
  //printf("\n lolol \n");
  if (node->isLeaf()) {

    bool didIntersect = false;

    for (Primitive *p : *node->prims) {

      //double oldmin = ray.min_t;
        if (p->intersect(ray, i)) {
            didIntersect = true;
        }
      //didIntersect = p->intersect(ray, i) || didIntersect;
        //printf("\n oi \n");

        /*if (ray.min_t < oldmin) {
          i->primitive = p;
          i->t = ray.min_t;
          //Vector3D intersection = ray.o + ray.min_t * ray.d;
          i->n = Vector3D(0, 0, 0);
          i->bsdf = p->get_bsdf();
        }*/
    }
    return didIntersect;
  }
    /*if (node->isLeaf()) {
    printf("what heck");
    }
    printf("\n h \n");*/
  bool hit1 = intersect(ray, i, node->r);
    //printf("\n i \n");
  bool hit2 = intersect(ray, i, node->l);

    //printf("\n intersect after \n");

  return hit1 || hit2;

  /*bool hit = false;
  for (Primitive *p : *(root->prims)) {
    total_isects++;
    if (p->intersect(ray, i))
      hit = true;
  }
  return hit;*/
}

}  // namespace StaticScene
}  // namespace CGL
