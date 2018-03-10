#include "bbox.h"

#include "GL/glew.h"

#include <algorithm>
#include <iostream>

namespace CGL {

    bool BBox::intersect(const Ray &r, double &t0, double &t1) const {

        // TODO (Part 2.2):
        // Implement ray - bounding box intersection test
        // If the ray intersected the bounding box within the range given by
        // t0, t1, update t0 and t1 with the new intersection times.

        //bounding box min and max are infinity

        //printf("bleh 0");
        double tx1 = (min.x - r.o.x) / r.d.x;
        //printf("bleh 1");
        double tx2 = (max.x - r.o.x) / r.d.x;
        //printf("bleh 2");
        double ty1 = (min.y - r.o.y) / r.d.y;
        //printf("bleh 3");
        double ty2 = (max.y - r.o.y) / r.d.y;
        //printf("bleh 4");
        double tz1 = (min.z - r.o.z) / r.d.z;
        //printf("bleh 5");
        double tz2 = (max.z - r.o.z) / r.d.z;
        //printf("bleh 6");

        double t0x, t0y, t1x, t1y, t0z, t1z;
        if (tx1 > tx2) {
            t0x = tx2;
            t1x = tx1;
        }
        else {
            t0x = tx1;
            t1x = tx2;
        }

        if (ty1 > ty2) {
            t0y = ty2;
            t1y = ty1;
        }
        else {
            t0y = ty1;
            t1y = ty2;
        }

        if (tz1 > tz2) {
            t0z = tz2;
            t1z = tz1;
        }
        else {
            t0z = tz1;
            t1z = tz2;
        }

        double maxofmins;
        double minofmaxes;

        if (t0x >= t0y && t0x >= t0z) {
            //printf("x \n");
            maxofmins = t0x;
        }
        else if (t0y >= t0x && t0y >= t0z) {
            //printf("y \n");
            maxofmins = t0y;
        }
        else {
            //printf("z \n");
            maxofmins = t0z;
        }

        if (t1x <= t1y && t1x <= t1z) {
            //printf("x \n");
            minofmaxes = t1x;
        }
        else if (t1y <= t1x && t1y <= t1z) {
            //printf("y \n");
            minofmaxes = t1y;
        }
        else {
            //printf("z \n");
            minofmaxes = t1z;
        }

        /*printf("t0x %f \n", t0x);
        printf("t0y %f \n", t0y);
        printf("t0z %f \n", t0z);
        printf("maxofmin %f \n \n", maxofmins);
        printf("t1x %f \n", t1x);
        printf("t1y %f \n", t1y);
        printf("t1z %f \n", t1z);
        printf("minofmax %f \n \n", minofmaxes);*/

        /*printf(".....\n");
        printf("r.o.x %f \n", r.o.x);
        printf("r.o.y %f \n", r.o.y);
        printf("r.o.z %f \n", r.o.z);
        printf("r.d.x %f \n", r.d.x);
        printf("r.d.y %f \n", r.d.y);
        printf("r.d.z %f \n", r.d.z);
        printf("min.x %f \n", min.x);
        printf("min.y %f \n", min.y);
        printf("min.z %f \n", min.z);
        printf("max.x %f \n", max.x);
        printf("max.y %f \n", max.y);
        printf("max.z %f \n", max.z);*/
        if (minofmaxes < maxofmins) return false;
        if (t0 > minofmaxes || t1 < maxofmins) return false;
        t0 = minofmaxes;
        t1 = maxofmins;
        return true;

        if (t0 <= maxofmins && maxofmins <= minofmaxes && minofmaxes <= t1) {
            //printf("wasssup");
            t0 = maxofmins;
            t1 = minofmaxes;
            return true;
        }
        else {
            return false;
        }
    }

    void BBox::draw(Color c) const {

        glColor4f(c.r, c.g, c.b, c.a);

        // top
        glBegin(GL_LINE_STRIP);
        glVertex3d(max.x, max.y, max.z);
        glVertex3d(max.x, max.y, min.z);
        glVertex3d(min.x, max.y, min.z);
        glVertex3d(min.x, max.y, max.z);
        glVertex3d(max.x, max.y, max.z);
        glEnd();

        // bottom
        glBegin(GL_LINE_STRIP);
        glVertex3d(min.x, min.y, min.z);
        glVertex3d(min.x, min.y, max.z);
        glVertex3d(max.x, min.y, max.z);
        glVertex3d(max.x, min.y, min.z);
        glVertex3d(min.x, min.y, min.z);
        glEnd();

        // side
        glBegin(GL_LINES);
        glVertex3d(max.x, max.y, max.z);
        glVertex3d(max.x, min.y, max.z);
        glVertex3d(max.x, max.y, min.z);
        glVertex3d(max.x, min.y, min.z);
        glVertex3d(min.x, max.y, min.z);
        glVertex3d(min.x, min.y, min.z);
        glVertex3d(min.x, max.y, max.z);
        glVertex3d(min.x, min.y, max.z);
        glEnd();

    }

    std::ostream &operator<<(std::ostream &os, const BBox &b) {
        return os << "BBOX(" << b.min << ", " << b.max << ")";
    }

} // namespace CGL
