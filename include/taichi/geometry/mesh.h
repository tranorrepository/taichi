/*******************************************************************************
    Taichi - Physically based Computer Graphics Library

    Copyright (c) 2017 Yuanming Hu <yuanmhu@gmail.com>

    All rights reserved. Use of this source code is governed by
    the MIT license as written in the LICENSE file.
*******************************************************************************/

#pragma once

#include <memory>
#include <vector>
#include <taichi/common/config.h>
#include <taichi/math.h>
#include <taichi/visual/scene.h>

TC_NAMESPACE_BEGIN

template <int dim>
struct Element {
  using Vector = VectorND<dim, real>;
  using Vectori = VectorND<dim, int>;
  using Matrix = MatrixND<dim, real>;
  using MatrixP = MatrixND<dim + 1, real>;

  Vector v[dim];
  bool open_end[dim];

  Element() {
    for (int i = 0; i < dim; i++) {
      v[i] = Vector(0.0_f);
      open_end[i] = false;
    }
  }

  Element get_transformed(const MatrixP &m) const {
    Element ret;
    for (int i = 0; i < dim; i++) {
      ret.v[i] = transform(m, v[i], 1);
      ret.open_end[i] = open_end[i];
    }
    return ret;
  }

  Vector get_center() const {
    Vector sum(0);
    for (int i = 0; i < dim; i++) {
      sum += v[i];
    }
    return 1.0_f / dim * sum;
  }

  Vector get_normal() const {
    Vector ret;
    TC_STATIC_IF(dim == 2) {
      Vector d = v[1] - v[0];
      ret = normalized(Vector(d[1], -d[0]));
    }
    TC_STATIC_ELSE {
      Vector n = cross(v[1] - v[0], v[2] - v[1]);
      ret = normalized(n);
    }
    TC_STATIC_END_IF
    return ret;
  }

  TC_IO_DECL {
    TC_IO(v);
    TC_IO(open_end);
  }
};

template <int dim>
struct ElementMesh {
  using Elem = Element<dim>;
  std::vector<Elem> elements;
  using Vector = VectorND<dim, real>;
  using Vectori = VectorND<dim, int>;
  using MatrixP = MatrixND<dim + 1, real>;

  void initialize(const Config &config) {
    TC_STATIC_IF(dim == 2) {
      std::string s = config.get<std::string>("segment_mesh");
      std::stringstream ss(s);
      int n;
      ss >> n;
      for (int i = 0; i < n; i++) {
        Elem elem;
        ss >> elem.v[0][0];
        ss >> elem.v[0][1];
        ss >> elem.v[1][0];
        ss >> elem.v[1][1];
        elements.push_back(elem);
      }
    }
    TC_STATIC_ELSE {
      TC_INFO("Adding mesh, fn={}", config.get<std::string>("mesh_fn"));
      std::string mesh_fn = config.get<std::string>("mesh_fn");
      std::string full_fn = std::getenv("TAICHI_ROOT_DIR") +
                            std::string("/taichi/projects/mpm/data/") + mesh_fn;
      std::shared_ptr<Mesh> mesh = std::make_shared<Mesh>();
      Config mesh_config;
      mesh_config.set("filename", full_fn);
      mesh_config.set("reverse_vertices",
                      config.get<bool>("reverse_vertices", false));
      mesh->initialize(mesh_config);
      for (auto tri : mesh->get_triangles()) {
        Elem elem;
        for (int i = 0; i < 3; i++) {
          elem.v[i] = tri.v[i];
        }
        elements.push_back(elem);
      }
    }
    TC_STATIC_END_IF
  }
};

TC_FORCE_INLINE real distance_to_segment(const Vector2 &pos,
                                         const Vector2 &a,
                                         const Vector2 &b) {
  real t = dot(pos - a, b - a) / length2(b - a);
  // t = clamp(t, 0.0_f, 1.0_f);
  if (0.0_f <= t && t <= 1.0_f) {
    return length(pos - lerp(t, a, b));
  } else {
    return 1e20_f;
  }
}

TC_FORCE_INLINE real distance_to_triangle(const Vector3 &pos,
                                          const Element<3> &tri) {
  Vector3 normal = tri.get_normal();
  real height = dot(normal, (pos - tri.v[0]));

  const Vector3 inter_local = pos - normal * height - tri.v[0];
  const Vector3 u = tri.v[1] - tri.v[0], v = tri.v[2] - tri.v[0];
  real uv = dot(u, v), vv = dot(v, v), wu = dot(inter_local, u), uu = dot(u, u),
       wv = dot(inter_local, v);
  real dom = uv * uv - uu * vv;
  real coord_u = (uv * wv - vv * wu) / dom;
  real coord_v = (uv * wu - uu * wv) / dom;

  if (coord_u >= 0 && coord_v >= 0 && coord_u + coord_v <= 1)
    return std::abs(height);
  return 1e20_f;
}

// Note: assuming world origin aligns with elem.v[0]
TC_FORCE_INLINE Matrix2 world_to_element(const Element<2> &elem) {
  Vector2 v = elem.v[1] - elem.v[0];
  Vector2 n = normalized(Vector2(v.y, -v.x));
  return inversed(Matrix2(v, n));
}

// Note: assuming world origin aligns with elem.v[0]
TC_FORCE_INLINE Matrix3 world_to_element(const Element<3> &elem) {
  Vector3 u = elem.v[1] - elem.v[0];
  Vector3 v = elem.v[2] - elem.v[0];
  Vector3 n = normalized(cross(u, v));
  return inversed(Matrix3(u, v, n));
}

TC_NAMESPACE_END
