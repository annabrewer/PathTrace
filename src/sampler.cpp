#include "sampler.h"

namespace CGL {

// Uniform Sampler2D Implementation //

    Vector2D UniformGridSampler2D::get_sample() const {
        //printf("WTF");

        return Vector2D(random_uniform(), random_uniform());

    }

Vector2D UniformGridSampler2D::get_sample(int index, int sideLength) const {
    //printf("wtf");

  return Vector2D(random_uniform(), random_uniform());

}

    Vector2D JitterGridSampler2D::get_sample() const {

        return Vector2D(random_uniform(), random_uniform());

    }
    Vector2D HammersleyGridSampler2D::get_sample() const {

        return Vector2D(random_uniform(), random_uniform());

    }
    Vector2D HaltonGridSampler2D::get_sample() const {

        return Vector2D(random_uniform(), random_uniform());

    }

    Vector2D JitterGridSampler2D::get_sample(int index, int sideLength) const {

        //index from 0 to N-1 where N is number of samples
        //sideLength is sqrt(N);


      int ind = index - 1;
      double rx = random_uniform();
      double ry = random_uniform();

        //double indexD = (1.0*index);
        //double sideLengthD = 1.0*sideLength;
        double unit = (1/(1.0*sideLength));

      double x = unit * ((ind % sideLength) + rx);
      double y = unit * ((ind / sideLength) + ry);
        if (ind == 1) {
            //should be less than 0.125
            /*printf("unit %f \n", unit);
            printf("xy %f %f \n", x, y);
            printf("ROW %d \n", (ind % sideLength));
            printf("COL %d \n", (ind / sideLength));*/
        }

      return Vector2D(x, y);
    }

    Vector2D HaltonGridSampler2D::get_sample(int index, int sideLength) const {

        /*int bx = 2;
        int by = 3;*/

        /*std::string r;
        int n = index;

        double unit = (1/(1.0*sideLength));

        double x = unit * ((index % sideLength));
        double y = unit * ((index / sideLength));

        while(x!=0) {
            r = (n%2==0 ?"0":"1") + r;
            n /= 2;
        }

        while(y!=0) {
            if (n%3 == 0) {
                r += "0";
            }
            else if (n%3 == 1) {
                r += "1";
            }
            else {
                r += "2";
            }
        }

        int place = 1;

        double rvx = 0;
        for (char c : r) {

            place = place * 2;
        }

        double rvy = 0;
        for (char c : r) {
            rvy += (1/stoi(c)) * 0.5;
            place = place * 3;
        }*/


      return Vector2D(random_uniform(), random_uniform());

    }

    Vector2D HammersleyGridSampler2D::get_sample(int index, int sideLength) const {

       /* printf("IND %d \n", index);
        printf("sidelength %d \n", sideLength);*/

        int ind = index - 1;
        int indcopy = index - 1;

        double unit = (1/(1.0*sideLength));

        int xi = (ind % sideLength);
        //int yi = (ind / sideLength);

        //double x = unit * xi;
        //double y = unit * yi;

        //std::string rx;
        std::string ry;

        //printf("X %d \n", xi);

        //printf("Y %d \n", yi);

        /*while(xi!=0) {
            rx = (xi%2==0 ?"0":"1") + rx;
            xi /= 2;
        }*/

        while(ind!=0) {
            ry = (ind%2==0 ?"0":"1") + ry;
            ind /= 2;
        }

        /*printf("RX %s \n", rx.c_str());
        printf("RY %s \n", ry.c_str());*/

        //int place = pow(2, rx.length() - 1);
        //printf("place %d", place);

        /*double rvx = 0;
        for (char c : rx) {
            std::string s(1, c);
            double si = stoi(s)*1.0;
            rvx += (1.0/place) * 0.5 * si;
            //printf("x add %f \n", (1.0/place) * 0.5 * si);
            place = place / 2;

        }*/

        int place = pow(2, ry.length() - 1);

        double rvy = 0;
        for (char c : ry) {
            std::string s(1, c);
            double si = stoi(s)*1.0;
            rvy += (1.0/place) * 0.5 * si;
            //printf("y add %f \n", (1.0/place) * 0.5 * si);
            place = place / 2;
        }

        double rvx;

        if (xi == 0) {
            rvx = 0;
        }
        else {
            //printf("please");
            rvx = (1.0*indcopy)/(1.0*pow(sideLength,2));
            //printf("please");
        }

        return Vector2D(rvx, rvy);

    };


// Uniform Hemisphere Sampler3D Implementation //

Vector3D UniformHemisphereSampler3D::get_sample() const {

  double Xi1 = random_uniform();
  double Xi2 = random_uniform();

  double theta = acos(Xi1);
  double phi = 2.0 * PI * Xi2;

  double xs = sinf(theta) * cosf(phi);
  double ys = sinf(theta) * sinf(phi);
  double zs = cosf(theta);

  return Vector3D(xs, ys, zs);

}

Vector3D CosineWeightedHemisphereSampler3D::get_sample() const {
  float f;
  return get_sample(&f);
}

Vector3D CosineWeightedHemisphereSampler3D::get_sample(float *pdf) const {

  double Xi1 = random_uniform();
  double Xi2 = random_uniform();

  double r = sqrt(Xi1);
  double theta = 2. * PI * Xi2;
  *pdf = sqrt(1-Xi1) / PI;
  return Vector3D(r*cos(theta), r*sin(theta), sqrt(1-Xi1));
}


} // namespace CGL
