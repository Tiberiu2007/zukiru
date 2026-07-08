# math

**Layer 0 — foundation.** Header-only linear algebra and geometry. Depends only
on [`core`](../core) (for the fixed-width scalar aliases).

Namespace: `zuki::math` (nested, per the standard convention — `core` is the only
root-namespace exception).

## Conventions

These are engine-wide and worth committing to memory:

- **Right-handed** coordinate system.
- **Column-vector** math: a transform is applied as `p' = M * p`, and `A * B`
  means "apply B first, then A". Quaternion composition matches (`a * b` applies
  `b` first).
- **Column-major** matrix storage (OpenGL/GLSL layout): `Mat4::e` can be uploaded
  straight to a graphics API. Element `(row, col)` is `e[col * N + row]`.
- **Projections target clip-space depth `[0, 1]`** (Vulkan/D3D style). Any
  clip-space Y flip is the render backend's responsibility, not the math's.
- **f32 everywhere.** `Vec4` and `Mat4` are 16-byte aligned so they can later be
  backed by SIMD without an ABI change.

## Headers

| Header | Contents |
|--------|----------|
| `<zuki/math/scalar.hpp>` | Constants (`kPi`, `kDegToRad`, `kEpsilon`, …), `radians`/`degrees`, `lerp`, `clamp`, `saturate`, `sign`, `approxEqual`/`approxZero`, generic `min`/`max`. |
| `<zuki/math/vec.hpp>` | `Vec2`/`Vec3`/`Vec4`: operators, `dot`, `cross`, `length`, `normalize`, `distance`, `reflect`, `lerp`, component-wise min/max. |
| `<zuki/math/mat.hpp>` | `Mat3`/`Mat4`: multiply, `transpose`, `determinant`, `inverse`, `translation`/`scale`, point/direction transforms. |
| `<zuki/math/quat.hpp>` | `Quat`: `fromAxisAngle`/`fromEuler`, compose, `rotate`, `conjugate`/`inverse`, `slerp`, `toMat3`/`toMat4`. |
| `<zuki/math/transform.hpp>` | `Transform` (TRS), `lookAt`, `perspective`, `orthographic`. |
| `<zuki/math/geometry.hpp>` | `Aabb`, `Ray`, `Plane`, `Sphere` + `raySphere`/`rayPlane`/`rayAabb`. |
| `<zuki/math/math.hpp>` | Umbrella header. |

## Example

```cpp
#include <zuki/math/math.hpp>
using namespace zuki::math;

Mat4 view = lookAt({0, 2, 5}, {0, 0, 0}, Vec3::unitY());
Mat4 proj = perspective(radians(60.0f), 16.0f / 9.0f, 0.1f, 1000.0f);

Transform t;
t.position = {1, 0, 0};
t.rotation = Quat::fromAxisAngle(Vec3::unitY(), radians(90.0f));
Mat4 mvp = proj * view * t.toMatrix();
```

## SIMD

Implementations are currently scalar but the value types are laid out to allow a
drop-in SIMD backend later (aligned `Vec4`/`Mat4`, free-function algebra). That
optimization is deliberately deferred until profiling justifies it.

## Tests

```bash
ctest --preset debug -R '^math\.'
```

## Dependencies

`core` (Layer 0). See the dependency table in
[`agents/PROJECT_STRUCTURE.md`](../../../agents/PROJECT_STRUCTURE.md).
