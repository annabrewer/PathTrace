#include "pathtracer.h"
#include "bsdf.h"
#include "ray.h"

#include <stack>
#include <random>
#include <algorithm>
#include <sstream>

#include "CGL/CGL.h"
#include "CGL/vector3D.h"
#include "CGL/matrix3x3.h"
#include "CGL/lodepng.h"

#include "GL/glew.h"

#include "static_scene/sphere.h"
#include "static_scene/triangle.h"
#include "static_scene/light.h"

using namespace CGL::StaticScene;

using std::min;
using std::max;

namespace CGL {

PathTracer::PathTracer(size_t ns_aa,
                       size_t max_ray_depth, size_t ns_area_light,
                       size_t ns_diff, size_t ns_glsy, size_t ns_refr,
                       size_t num_threads,
                       size_t samples_per_batch,
                       float max_tolerance,
                       HDRImageBuffer* envmap,
                       bool direct_hemisphere_sample,
                       string filename, int sample_type) {
  state = INIT,
  this->ns_aa = ns_aa;
  this->max_ray_depth = max_ray_depth;
  this->ns_area_light = ns_area_light;
  this->ns_diff = ns_diff;
  this->ns_glsy = ns_diff;
  this->ns_refr = ns_refr;
  this->samplesPerBatch = samples_per_batch;
  this->maxTolerance = max_tolerance;
  this->direct_hemisphere_sample = direct_hemisphere_sample;
  this->filename = filename;
  this->sampleType = sample_type;

  if (envmap) {
    this->envLight = new EnvironmentLight(envmap);
  } else {
    this->envLight = NULL;
  }

  bvh = NULL;
  scene = NULL;
  camera = NULL;

    if (sampleType == 0) {
        gridSampler = new UniformGridSampler2D();
    }
    else if (sampleType == 1) {
        gridSampler = new JitterGridSampler2D();
    }
    else if (sampleType == 2) {
        printf("howdy");
        gridSampler = new HammersleyGridSampler2D();
    }
    else {
        gridSampler = new HaltonGridSampler2D();
    }


  hemisphereSampler = new UniformHemisphereSampler3D();

  show_rays = true;

  imageTileSize = 32;
  numWorkerThreads = num_threads;
  workerThreads.resize(numWorkerThreads);

  tm_gamma = 2.2f;
  tm_level = 1.0f;
  tm_key = 0.18;
  tm_wht = 5.0f;

}

PathTracer::~PathTracer() {

  delete bvh;
  delete gridSampler;
  delete hemisphereSampler;

}

void PathTracer::set_scene(Scene *scene) {

  if (state != INIT) {
    return;
  }

  if (this->scene != nullptr) {
    delete scene;
    delete bvh;
    selectionHistory.pop();
  }

  if (this->envLight != nullptr) {
    scene->lights.push_back(this->envLight);
  }

  this->scene = scene;
  build_accel();

  if (has_valid_configuration()) {
      printf("pleaseee");
    state = READY;
  }
}

void PathTracer::set_camera(Camera *camera) {

  if (state != INIT) {
    return;
  }

  this->camera = camera;
  if (has_valid_configuration()) {
    state = READY;
  }

}

void PathTracer::set_frame_size(size_t width, size_t height) {
  if (state != INIT && state != READY) {
    stop();
  }
  sampleBuffer.resize(width, height);
  frameBuffer.resize(width, height);
  cell_tl = Vector2D(0,0);
  cell_br = Vector2D(width, height);
  render_cell = false;
  sampleCountBuffer.resize(width * height);
  if (has_valid_configuration()) {
    state = READY;
  }
}

bool PathTracer::has_valid_configuration() {
   // printf("hello");
   if (!scene ) {
        printf("nice");
    }
  return scene && camera && gridSampler && hemisphereSampler &&
         (!sampleBuffer.is_empty());
}

void PathTracer::update_screen() {
  switch (state) {
    case INIT:
    case READY:
      break;
    case VISUALIZE:
      visualize_accel();
      break;
    case RENDERING:
      glDrawPixels(frameBuffer.w, frameBuffer.h, GL_RGBA,
                   GL_UNSIGNED_BYTE, &frameBuffer.data[0]);
      if (render_cell)
        visualize_cell();
      break;
    case DONE:
        //sampleBuffer.tonemap(frameBuffer, tm_gamma, tm_level, tm_key, tm_wht);
      glDrawPixels(frameBuffer.w, frameBuffer.h, GL_RGBA,
                   GL_UNSIGNED_BYTE, &frameBuffer.data[0]);
      if (render_cell)
        visualize_cell();
      break;
  }
}

void PathTracer::stop() {
  switch (state) {
    case INIT:
    case READY:
      break;
    case VISUALIZE:
      while (selectionHistory.size() > 1) {
        selectionHistory.pop();
      }
      state = READY;
      break;
    case RENDERING:
      continueRaytracing = false;
    case DONE:
      for (int i=0; i<numWorkerThreads; i++) {
            workerThreads[i]->join();
            delete workerThreads[i];
        }
      state = READY;
      break;
  }
}

void PathTracer::clear() {
  if (state != READY) return;
  delete bvh;
  bvh = NULL;
  scene = NULL;
  camera = NULL;
  selectionHistory.pop();
  sampleBuffer.resize(0, 0);
  frameBuffer.resize(0, 0);
  state = INIT;
  render_cell = false;
}

void PathTracer::start_visualizing() {
  if (state != READY) {
    return;
  }
  state = VISUALIZE;
}

void PathTracer::start_raytracing() {
  if (state != READY) return;

  rayLog.clear();
  workQueue.clear();

  state = RENDERING;
  continueRaytracing = true;
  workerDoneCount = 0;

  sampleBuffer.clear();
  if (!render_cell) {
    frameBuffer.clear();
    num_tiles_w = sampleBuffer.w / imageTileSize + 1;
    num_tiles_h = sampleBuffer.h / imageTileSize + 1;
    tilesTotal = num_tiles_w * num_tiles_h;
    tilesDone = 0;
    tile_samples.resize(num_tiles_w * num_tiles_h);
    memset(&tile_samples[0], 0, num_tiles_w * num_tiles_h * sizeof(int));

    // populate the tile work queue
    for (size_t y = 0; y < sampleBuffer.h; y += imageTileSize) {
        for (size_t x = 0; x < sampleBuffer.w; x += imageTileSize) {
            workQueue.put_work(WorkItem(x, y, imageTileSize, imageTileSize));
        }
    }
  } else {
    int w = (cell_br-cell_tl).x;
    int h = (cell_br-cell_tl).y;
    int imTS = imageTileSize / 4;
    num_tiles_w = w / imTS + 1;
    num_tiles_h = h / imTS + 1;
    tilesTotal = num_tiles_w * num_tiles_h;
    tilesDone = 0;
    tile_samples.resize(num_tiles_w * num_tiles_h);
    memset(&tile_samples[0], 0, num_tiles_w * num_tiles_h * sizeof(int));

    // populate the tile work queue
    for (size_t y = cell_tl.y; y < cell_br.y; y += imTS) {
      for (size_t x = cell_tl.x; x < cell_br.x; x += imTS) {
        workQueue.put_work(WorkItem(x, y,
          min(imTS, (int)(cell_br.x-x)), min(imTS, (int)(cell_br.y-y)) ));
      }
    }
  }

  bvh->total_isects = 0; bvh->total_rays = 0;
  // launch threads
  fprintf(stdout, "[PathTracer] Rendering... "); fflush(stdout);
  for (int i=0; i<numWorkerThreads; i++) {
      workerThreads[i] = new std::thread(&PathTracer::worker_thread, this);
  }
}

void PathTracer::render_to_file(string filename, size_t x, size_t y, size_t dx, size_t dy) {
  if (x == -1) {
    unique_lock<std::mutex> lk(m_done);
    start_raytracing();
    cv_done.wait(lk, [this]{ return state == DONE; });
    lk.unlock();
    save_image(filename);
    fprintf(stdout, "[PathTracer] Job completed.\n");
  } else {
    render_cell = true;
    cell_tl = Vector2D(x,y);
    cell_br = Vector2D(x+dx,y+dy);
    ImageBuffer buffer;
    raytrace_cell(buffer);
    save_image(filename, &buffer);
    fprintf(stdout, "[PathTracer] Cell job completed.\n");
  }
}


void PathTracer::build_accel() {

  // collect primitives //
  fprintf(stdout, "[PathTracer] Collecting primitives... "); fflush(stdout);
  timer.start();
  vector<Primitive *> primitives;
  for (SceneObject *obj : scene->objects) {
    const vector<Primitive *> &obj_prims = obj->get_primitives();
    primitives.reserve(primitives.size() + obj_prims.size());
    primitives.insert(primitives.end(), obj_prims.begin(), obj_prims.end());
  }
  timer.stop();
  fprintf(stdout, "Done! (%.4f sec)\n", timer.duration());

  // build BVH //
  fprintf(stdout, "[PathTracer] Building BVH from %lu primitives... ", primitives.size());
  fflush(stdout);
  timer.start();
  bvh = new BVHAccel(primitives);
  timer.stop();
  fprintf(stdout, "Done! (%.4f sec)\n", timer.duration());

  // initial visualization //
  selectionHistory.push(bvh->get_root());
}

void PathTracer::visualize_accel() const {

  glPushAttrib(GL_ENABLE_BIT);
  glDisable(GL_LIGHTING);
  glLineWidth(1);
  glEnable(GL_DEPTH_TEST);

  // hardcoded color settings
  Color cnode = Color(.5, .5, .5, .25);
  Color cnode_hl = Color(1., .25, .0, .6);
  Color cnode_hl_child = Color(1., 1., 1., .6);

  Color cprim_hl_left = Color(.6, .6, 1., 1);
  Color cprim_hl_right = Color(.8, .8, 1., 1);
  Color cprim_hl_edges = Color(0., 0., 0., 0.5);

  BVHNode *selected = selectionHistory.top();

  // render solid geometry (with depth offset)
  glPolygonOffset(1.0, 1.0);
  glEnable(GL_POLYGON_OFFSET_FILL);

  if (selected->isLeaf()) {
    bvh->draw(selected, cprim_hl_left);
  } else {
    bvh->draw(selected->l, cprim_hl_left);
    bvh->draw(selected->r, cprim_hl_right);
  }

  glDisable(GL_POLYGON_OFFSET_FILL);

  // draw geometry outline
  bvh->drawOutline(selected, cprim_hl_edges);

  // keep depth buffer check enabled so that mesh occluded bboxes, but
  // disable depth write so that bboxes don't occlude each other.
  glDepthMask(GL_FALSE);

  // create traversal stack
  stack<BVHNode *> tstack;

  // push initial traversal data
  tstack.push(bvh->get_root());

  // draw all BVH bboxes with non-highlighted color
  while (!tstack.empty()) {

    BVHNode *current = tstack.top();
    tstack.pop();

    current->bb.draw(cnode);
    if (current->l) tstack.push(current->l);
    if (current->r) tstack.push(current->r);
  }

  // draw selected node bbox and primitives
  if (selected->l) selected->l->bb.draw(cnode_hl_child);
  if (selected->r) selected->r->bb.draw(cnode_hl_child);

  glLineWidth(3.f);
  selected->bb.draw(cnode_hl);

  // now perform visualization of the rays
  if (show_rays) {
      glLineWidth(1.f);
      glBegin(GL_LINES);

      for (size_t i=0; i<rayLog.size(); i+=500) {

          const static double VERY_LONG = 10e4;
          double ray_t = VERY_LONG;

          // color rays that are hits yellow
          // and rays this miss all geometry red
          if (rayLog[i].hit_t >= 0.0) {
              ray_t = rayLog[i].hit_t;
              glColor4f(1.f, 1.f, 0.f, 0.1f);
          } else {
              glColor4f(1.f, 0.f, 0.f, 0.1f);
          }

          Vector3D end = rayLog[i].o + ray_t * rayLog[i].d;

          glVertex3f(rayLog[i].o[0], rayLog[i].o[1], rayLog[i].o[2]);
          glVertex3f(end[0], end[1], end[2]);
      }
      glEnd();
  }

  glDepthMask(GL_TRUE);
  glPopAttrib();
}

void PathTracer::visualize_cell() const {
  glPushAttrib(GL_VIEWPORT_BIT);
  glViewport(0, 0, sampleBuffer.w, sampleBuffer.h);

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(0, sampleBuffer.w, sampleBuffer.h, 0, 0, 1);

  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();
  glTranslatef(0, 0, -1);

  glColor4f(1.0, 0.0, 0.0, 0.8);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_LIGHTING);

  // Draw the Red Rectangle.
  glBegin(GL_LINE_LOOP);
  glVertex2f(cell_tl.x, sampleBuffer.h-cell_br.y);
  glVertex2f(cell_br.x, sampleBuffer.h-cell_br.y);
  glVertex2f(cell_br.x, sampleBuffer.h-cell_tl.y);
  glVertex2f(cell_tl.x, sampleBuffer.h-cell_tl.y);
  glEnd();

  glMatrixMode(GL_PROJECTION);
  glPopMatrix();

  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();

  glPopAttrib();

  glEnable(GL_LIGHTING);
  glEnable(GL_DEPTH_TEST);
}

void PathTracer::key_press(int key) {
  BVHNode *current = selectionHistory.top();
  switch (key) {
  case ']':
      ns_aa *=2;
      fprintf(stdout, "[PathTracer] Samples per pixel changed to %lu\n", ns_aa);
      //tm_key = clamp(tm_key + 0.02f, 0.0f, 1.0f);
      break;
  case '[':
      //tm_key = clamp(tm_key - 0.02f, 0.0f, 1.0f);
      ns_aa /=2;
      if (ns_aa < 1) ns_aa = 1;
      fprintf(stdout, "[PathTracer] Samples per pixel changed to %lu\n", ns_aa);
      break;
  case '=': case '+':
      ns_area_light *= 2;
      fprintf(stdout, "[PathTracer] Area light sample count increased to %zu.\n", ns_area_light);
      break;
  case '-': case '_':
      if (ns_area_light > 1) ns_area_light /= 2;
      fprintf(stdout, "[PathTracer] Area light sample count decreased to %zu.\n", ns_area_light);
      break;
  case '.': case '>':
      max_ray_depth++;
      fprintf(stdout, "[PathTracer] Max ray depth increased to %zu.\n", max_ray_depth);
      break;
  case ',': case '<':
      if (max_ray_depth) max_ray_depth--;
      fprintf(stdout, "[PathTracer] Max ray depth decreased to %zu.\n", max_ray_depth);
      break;
  case 'h': case 'H':
      direct_hemisphere_sample = !direct_hemisphere_sample;
      fprintf(stdout, "[PathTracer] Toggled direct lighting to %s\n", (direct_hemisphere_sample ? "uniform hemisphere sampling" : "importance light sampling"));
      break;
  case KEYBOARD_UP:
      if (current != bvh->get_root()) {
          selectionHistory.pop();
      }
      break;
  case KEYBOARD_LEFT:
      if (current->l) {
          selectionHistory.push(current->l);
      }
      break;
  case KEYBOARD_RIGHT:
      if (current->l) {
          selectionHistory.push(current->r);
      }
      break;

  case 'C':
    render_cell = !render_cell;
    if (render_cell)
      fprintf(stdout, "[PathTracer] Now in cell render mode.\n");
    else
      fprintf(stdout, "[PathTracer] No longer in cell render mode.\n");
    break;

  case 'a': case 'A':
      show_rays = !show_rays;
  default:
      return;
  }
}


Spectrum PathTracer::estimate_direct_lighting_hemisphere(const Ray& r, const Intersection& isect) {
  // Estimate the lighting from this intersection coming directly from a light.
  // For this function, sample uniformly in a hemisphere.

  // make a coordinate system for a hit point
  // with N aligned with the Z direction.
  Matrix3x3 o2w;
  make_coord_space(o2w, isect.n);
  Matrix3x3 w2o = o2w.T();

  // w_out points towards the source of the ray (e.g.,
  // toward the camera if this is a primary ray)
  const Vector3D& hit_p = r.o + r.d * isect.t;
  const Vector3D& w_out = w2o * (-r.d);

  // This is the same number of total samples as estimate_direct_lighting_importance (outside of delta lights).
  // We keep the same number of samples for clarity of comparison.
  int num_samples = scene->lights.size() * ns_area_light; //# lights * number of samples per light
  Spectrum L_out;

  // TODO (Part 3): Write your sampling loop here
  // COMMENT OUT `normal_shading` IN `est_radiance_global_illumination` BEFORE YOU BEGIN

  for (int i = 0; i < num_samples; i++) {
    //get incoming vector
    //Spectrum s =  isect.bsdf->sample_f(w_out, wi, pdf);

      //direction in local space
    Vector3D w_in = hemisphereSampler->get_sample();

    Vector3D raydir = o2w * w_in; //raydir = wi_world
    Vector3D rayorig = hit_p + (EPS_D * raydir);

    Ray r = Ray(rayorig, raydir);
    Intersection ise;

    bool inter = bvh->intersect(r, &ise);

    if (inter) {

        //bsdf at point
      Spectrum b = isect.bsdf->f(w_out, w_in);

        //incoming radiance
      Spectrum e = ise.bsdf->get_emission();

        //w_in.z is cosine of angle between w_in and normal vector
      L_out += b * e * cos_theta(w_in);
      //printf("\n L %f %f %f \n", L_out.r, L_out.g, L_out.b);

    }
  }

  L_out = (L_out * 2.0 * PI) / (1.0 * num_samples);

  return L_out;
}

Spectrum PathTracer::estimate_direct_lighting_importance(const Ray& r, const Intersection& isect) {
  // Estimate the lighting from this intersection coming directly from a light.
  // To implement importance sampling, sample only from lights, not uniformly in a hemisphere.

  // make a coordinate system for a hit point
  // with N aligned with the Z direction.
  Matrix3x3 o2w;
  make_coord_space(o2w, isect.n);
  Matrix3x3 w2o = o2w.T();

  // w_out points towards the source of the ray (e.g.,
  // toward the camera if this is a primary ray)
  const Vector3D& hit_p = r.o + r.d * isect.t;
  const Vector3D& w_out = w2o * (-r.d);
  Spectrum L_out;

  // TODO (Part 3): Here is where your code for looping over scene lights goes
  // COMMENT OUT `normal_shading` IN `est_radiance_global_illumination` BEFORE YOU BEGIN

    for (SceneLight* s : scene->lights) {
        Vector3D wi;
        float distToLight;
        float pdf;
        //check if delta light
        //printf("\n hello \n");

        /*if (s->is_delta_light()) {
          //take 1 sample
            Spectrum rad = s->sample_L(hit_p, &wi, &distToLight, &pdf);
            Vector3D w_in = w2o * wi;
            //Vector3D raydir = o2w * w_in; //raydir is wi
            Vector3D rayorig = hit_p + (EPS_D * wi);
            Ray r2 = Ray(rayorig, wi);
            r2.max_t = distToLight;
            Intersection ise;
            bool inter = bvh->intersect(r2, &ise);
            if (!inter) {
                Spectrum b = isect.bsdf->f(w_out, w_in);
                Spectrum add = (rad * b * cos_theta(w_in)) / pdf;
                L_out += add;
            }
        }*/
        int loops;
        if (s->is_delta_light()) {
            loops = 1;
        }
        else {
            loops = ns_area_light;
        }
            Spectrum L;
            for (int i = 0; i < loops; i++) {
                //get incoming vector
                //Spectrum s =  isect.bsdf->sample_f(w_out, wi, pdf);

                //need w2o or is it already in local space??
                Spectrum rad = s->sample_L(hit_p, &wi, &distToLight, &pdf);

                Vector3D w_in = w2o * wi;

                if (w_in.z >= 0) {
                    //printf("\n helloooo \n");
                    //Vector3D raydir = o2w * w_in;
                    Vector3D rayorig = hit_p + (EPS_D * wi);
                    Ray r2 = Ray(rayorig, wi);
                    r2.max_t = distToLight;

                    Intersection ise;

                    bool inter = bvh->intersect(r2, &ise);

                    if (!inter) {
                        //bsdf at point
                        Spectrum b = isect.bsdf->f(w_out, w_in);
                        Spectrum add = (rad * b * cos_theta(w_in)) / pdf;
                        L += add;
                    }
                }
            }
            L = L / (1.0 * loops);
            L_out += L;
        }
    //L_out = L_out / scene->lights.size();

  //printf("\n L final 1.5 %f %f %f \n", L_out.r, L_out.g, L_out.b);
  return L_out;
}

Spectrum PathTracer::zero_bounce_radiance(const Ray&r, const Intersection& isect) {
  // TODO: Part 4, Task 2
  // Returns the light that results from no bounces of light

  return isect.bsdf->get_emission();
}

Spectrum PathTracer::one_bounce_radiance(const Ray&r, const Intersection& isect) {
  // TODO: Part 4, Task 2
  // Returns either the direct illumination by hemisphere or importance sampling
  // depending on `direct_hemisphere_sample
  // (you implemented these functions in Part 3)
    Spectrum L_out;

    if (direct_hemisphere_sample) {
        L_out = estimate_direct_lighting_hemisphere(r, isect);
    }
    else {
        L_out = estimate_direct_lighting_importance(r, isect);
    }
    //printf("\n L final 2 %f %f %f \n", L_out.r, L_out.g, L_out.b);

  return L_out;

}

Spectrum PathTracer::at_least_one_bounce_radiance(const Ray&r, const Intersection& isect) {
  Matrix3x3 o2w;
  make_coord_space(o2w, isect.n);
  Matrix3x3 w2o = o2w.T();

  Vector3D hit_p = r.o + r.d * isect.t;
  Vector3D w_out = w2o * (-r.d);

    //for indirect ONLY
    //Spectrum L_out;
  Spectrum L_out = one_bounce_radiance(r, isect);


  // TODO (Part 4.2): Here is where your code for sampling the BSDF,
  // performing Russian roulette step, and returning a recursively
  // traced ray (when applicable) goes
    Vector3D w_in;
    float pdf;

    Spectrum s = isect.bsdf->sample_f(w_out, &w_in, &pdf);

    double cp;

    if (r.depth == max_ray_depth) {
        cp = 1;
        //for indirect ONLY
        //L_out = Spectrum(0, 0, 0);
    }
    else {
        cp = 0.65;
        //for indirect ONLY
        //L_out = one_bounce_radiance(r, isect);
    }

    if (r.depth > 1 && coin_flip(cp)) {
            Intersection ise;

            Vector3D raydir = o2w * w_in;
            Vector3D rayorig = hit_p + (EPS_D * raydir); //need to multiply eps_d by something?

            int d = r.depth - 1;
            //printf("\n %f \n", d);
            Ray r2 = Ray(rayorig, raydir, d);

            bool inter = bvh->intersect(r2, &ise);

            //Spectrum b = isect.bsdf->f(w_out, w_in);

            if(inter) {
                L_out += (at_least_one_bounce_radiance(r2, ise) * s * cos_theta(w_in)) / (pdf * cp);
            }
    }

  return L_out;

}

Spectrum PathTracer::est_radiance_global_illumination(const Ray &r) {
  Intersection isect;
  Spectrum L_out;

  // You will extend this in assignment 3-2.
  // If no intersection occurs, we simply return black.
  // This changes if you implement hemispherical lighting for extra credit.

  if (!bvh->intersect(r, &isect))
    return L_out;

  // This line returns a color depending only on the normal vector
  // to the surface at the intersection point.
  // REMOVE IT when you are ready to begin Part 3.
  //printf("thank god");
  //return normal_shading(isect.n);

  // TODO (Part 3): Return the direct illumination.

    if (max_ray_depth == 0) {
        L_out = zero_bounce_radiance(r, isect);
    }
    else if (max_ray_depth == 1) {
        //L_out = one_bounce_radiance(r, isect);
        L_out = zero_bounce_radiance(r, isect) + one_bounce_radiance(r, isect); //
    }
    else {
        L_out = zero_bounce_radiance(r, isect) + at_least_one_bounce_radiance(r, isect);
    }

  return L_out;
}

Spectrum PathTracer::raytrace_pixel(size_t x, size_t y) {

  // TODO (Part 1.1):
  // Make a loop that generates num_samples camera rays and traces them
  // through the scene. Return the average Spectrum.
  // You should call est_radiance_global_illumination in this function.

  // TODO (Part 5):
  // Modify your implementation to include adaptive sampling.
  // Use the command line parameters "samplesPerBatch" and "maxTolerance

  //printf("come onn where am i segfaulting");

    //printf("whyyyy");

  double num_samples = ns_aa;            // total samples to evaluate
  Vector2D origin = Vector2D(x,y);    // bottom left corner of the pixel

    int rowLen = sqrt(num_samples);



  Spectrum sum = Spectrum(0, 0, 0);

  double x_small = (x+0.5) / sampleBuffer.w;
  double y_small = (y+0.5) / sampleBuffer.h;
  Ray r = camera->generate_ray(x_small, y_small);
    r.depth = max_ray_depth;
  sum += est_radiance_global_illumination(r);
  //printf("XY %f %f \n", x_small, y_small);
  //printf("SUM %f %f %f \n", sum.r, sum.g, sum.b);

    int counter = 0;
    double s1 = 0;
    double s2 = 0;
    double u = 0;
    double var = 0;
    double conv = INFINITY;

    for(int i = 1; i <= num_samples; i++) {

        //printf("oii");

        Vector2D v = gridSampler->get_sample(i,rowLen);

        //printf("%f %f \n", v.x, v.y);

        //printf("hello");
        double d = v.x;
        double e = v.y;
        double x_small = (x + d) / sampleBuffer.w;
        double y_small = (y + e) / sampleBuffer.h;
        Ray r = camera->generate_ray(x_small, y_small);
        r.depth = max_ray_depth;
        Spectrum rad = est_radiance_global_illumination(r);
        sum += rad;

      counter++;
      double xk = rad.illum();
      s1 += xk;
      s2 += xk*xk;

      if (counter == samplesPerBatch) {

          double n = i*1.0;

          u = s1 / n;
          var = (1.0 / (n - 1)) * (s2 - (s1*s1) / n);
          conv = 1.96 * (sqrt(var)/sqrt(n));
          counter = 0;

          if (conv <= maxTolerance * u) {
              int ns = n;
              //printf("\n %f %d \n", n, ns);
              sum = sum / n;
              sampleCountBuffer[x + sampleBuffer.w * y] = ns;
              //printf("%d \n", ns);
              return sum;
          }
      }

  }
    int ns = num_samples;
    //printf("\n %f %d \n", num_samples, ns);
    sampleCountBuffer[x + frameBuffer.w * y] = ns;
  sum = sum / num_samples;


  //printf("SUM%f %f %f \n", sum.r, sum.g, sum.b);
  return sum;

}

void PathTracer::raytrace_tile(int tile_x, int tile_y,
                               int tile_w, int tile_h) {

  //printf("come onn where am i segfaulting 2");
    printf("wassup");

  size_t w = sampleBuffer.w;
  size_t h = sampleBuffer.h;

  size_t tile_start_x = tile_x;
  size_t tile_start_y = tile_y;

  size_t tile_end_x = std::min(tile_start_x + tile_w, w);
  size_t tile_end_y = std::min(tile_start_y + tile_h, h);

  size_t tile_idx_x = tile_x / imageTileSize;
  size_t tile_idx_y = tile_y / imageTileSize;
  size_t num_samples_tile = tile_samples[tile_idx_x + tile_idx_y * num_tiles_w];

  for (size_t y = tile_start_y; y < tile_end_y; y++) {
    if (!continueRaytracing) return;
    for (size_t x = tile_start_x; x < tile_end_x; x++) {
        Spectrum s = raytrace_pixel(x, y);
        sampleBuffer.update_pixel(s, x, y);
    }
  }

  tile_samples[tile_idx_x + tile_idx_y * num_tiles_w] += 1;
  sampleBuffer.toColor(frameBuffer, tile_start_x, tile_start_y, tile_end_x, tile_end_y);
}

void PathTracer::raytrace_cell(ImageBuffer& buffer) {
  size_t tile_start_x = cell_tl.x;
  size_t tile_start_y = cell_tl.y;

  size_t tile_end_x = cell_br.x;
  size_t tile_end_y = cell_br.y;

  size_t w = tile_end_x - tile_start_x;
  size_t h = tile_end_y - tile_start_y;
  HDRImageBuffer sb(w, h);
  buffer.resize(w,h);

  stop();
  render_cell = true;
  {
    unique_lock<std::mutex> lk(m_done);
    start_raytracing();
    cv_done.wait(lk, [this]{ return state == DONE; });
    lk.unlock();
  }

  for (size_t y = tile_start_y; y < tile_end_y; y++) {
    for (size_t x = tile_start_x; x < tile_end_x; x++) {
        buffer.data[w*(y-tile_start_y)+(x-tile_start_x)] = frameBuffer.data[x+y*sampleBuffer.w];
    }
  }
}

void PathTracer::worker_thread() {

  Timer timer;
  timer.start();

  WorkItem work;
  while (continueRaytracing && workQueue.try_get_work(&work)) {
    raytrace_tile(work.tile_x, work.tile_y, work.tile_w, work.tile_h);
    {
      lock_guard<std::mutex> lk(m_done);
      ++tilesDone;
      cout << "\r[PathTracer] Rendering... " << int((double)tilesDone/tilesTotal * 100) << '%';
      cout.flush();
    }
  }

  workerDoneCount++;
  if (!continueRaytracing && workerDoneCount == numWorkerThreads) {
    timer.stop();
    fprintf(stdout, "\n[PathTracer] Rendering canceled!\n");
    state = READY;
  }

  if (continueRaytracing && workerDoneCount == numWorkerThreads) {
    timer.stop();
    fprintf(stdout, "\r[PathTracer] Rendering... 100%%! (%.4fs)\n", timer.duration());
    fprintf(stdout, "[PathTracer] BVH traced %llu rays.\n", bvh->total_rays);
    fprintf(stdout, "[PathTracer] Averaged %f intersection tests per ray.\n", (((double)bvh->total_isects)/bvh->total_rays));

    lock_guard<std::mutex> lk(m_done);
    state = DONE;
    cv_done.notify_one();
  }
}

void PathTracer::save_image(string filename, ImageBuffer* buffer) {

  if (state != DONE) return;

  if (!buffer)
    buffer = &frameBuffer;

  if (filename == "") {
    time_t rawtime;
    time (&rawtime);

    time_t t = time(nullptr);
    tm *lt = localtime(&t);
    stringstream ss;
    ss << this->filename << "_screenshot_" << lt->tm_mon+1 << "-" << lt->tm_mday << "_"
      << lt->tm_hour << "-" << lt->tm_min << "-" << lt->tm_sec << ".png";
    filename = ss.str();
  }

  uint32_t* frame = &buffer->data[0];
  size_t w = buffer->w;
  size_t h = buffer->h;
  uint32_t* frame_out = new uint32_t[w * h];
  for(size_t i = 0; i < h; ++i) {
    memcpy(frame_out + i * w, frame + (h - i - 1) * w, 4 * w);
  }

  fprintf(stderr, "[PathTracer] Saving to file: %s... ", filename.c_str());
  lodepng::encode(filename, (unsigned char*) frame_out, w, h);
  fprintf(stderr, "Done!\n");

  save_sampling_rate_image(filename);
}

void PathTracer::save_sampling_rate_image(string filename) {
  size_t w = frameBuffer.w;
  size_t h = frameBuffer.h;
  ImageBuffer outputBuffer(w, h);

  for (int x = 0; x < w; x++) {
      for (int y = 0; y < h; y++) {
          float samplingRate = sampleCountBuffer[y * w + x] * 1.0f / ns_aa;

          Color c;
          if (samplingRate <= 0.5) {
              //printf("nice");
              float r = (0.5 - samplingRate) / 0.5;
              c = Color(0.0f, 0.0f, 1.0f) * r + Color(0.0f, 1.0f, 0.0f) * (1.0 - r);
          } else {
              //printf("noice");
              float r = (1.0 - samplingRate) / 0.5;
              c = Color(0.0f, 1.0f, 0.0f) * r + Color(1.0f, 0.0f, 0.0f) * (1.0 - r);
          }
          outputBuffer.update_pixel(c, x, h - 1 - y);
      }
  }

  lodepng::encode(filename.substr(0,filename.size()-4) + "_rate.png", (unsigned char*) (outputBuffer.data.data()), w, h);
}

}  // namespace CGL
