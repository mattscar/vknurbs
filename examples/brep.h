#ifndef BREP_H
#define BREP_H

#include <cmath>
#include <memory>
#include <vector>

enum class SurfaceType {
    None, Plane, Cylinder, Cone, Sphere, BSpline
};

struct StepEntity {
    int stepId = 0;
    virtual ~StepEntity() = default;
};

// Geometry
struct Point3D {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct Vector3D {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct Axis2Placement3D {
    Point3D location;
    Vector3D axis;
    Vector3D refDirection;
};

struct Point : StepEntity {};

struct CartesianPoint : Point {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct Curve : StepEntity {
    virtual Point3D evaluate(double u) const = 0;
};

struct Line : Curve {
    Point3D origin;
    Vector3D direction;
    Point3D evaluate(double u) const override {
        return {
            origin.x + u * direction.x,
            origin.y + u * direction.y,
            origin.z + u * direction.z
        };
    }
};

struct Circle : Curve {
    Axis2Placement3D position;
    double radius = 1.0;

    Point3D evaluate(double u) const override {
        // Local Z-axis (Normal)
        double zx = position.axis.x;
        double zy = position.axis.y;
        double zz = position.axis.z;

        // Local X-axis (Reference Direction)
        double xx = position.refDirection.x;
        double xy = position.refDirection.y;
        double xz = position.refDirection.z;

        // Local Y-axis (Cross product: Z x X)
        double yx = zy * xz - zz * xy;
        double yy = zz * xx - zx * xz;
        double yz = zx * xy - zy * xx;

        double cosU = std::cos(u);
        double sinU = std::sin(u);

        // P(u) = Origin + (R * cos(u)) * X + (R * sin(u)) * Y
        return {
            position.location.x + (radius * cosU * xx) + (radius * sinU * yx),
            position.location.y + (radius * cosU * xy) + (radius * sinU * yy),
            position.location.z + (radius * cosU * xz) + (radius * sinU * yz)
        };
    }
};

struct Ellipse : Curve {
    Axis2Placement3D position;
    double semiAxis1 = 1.0;
    double semiAxis2 = 1.0;

    Point3D evaluate(double u) const override {
        // Local Z-axis
        double zx = position.axis.x;
        double zy = position.axis.y;
        double zz = position.axis.z;

        // Local X-axis
        double xx = position.refDirection.x;
        double xy = position.refDirection.y;
        double xz = position.refDirection.z;

        // Local Y-axis (Cross product: Z x X)
        double yx = zy * xz - zz * xy;
        double yy = zz * xx - zx * xz;
        double yz = zx * xy - zy * xx;

        double cosU = std::cos(u);
        double sinU = std::sin(u);

        // P(u) = Origin + (a * cos(u)) * X + (b * sin(u)) * Y
        return {
            position.location.x +
                (semiAxis1 * cosU * xx) + (semiAxis2 * sinU * yx),
            position.location.y +
                (semiAxis1 * cosU * xy) + (semiAxis2 * sinU * yy),
            position.location.z +
                (semiAxis1 * cosU * xz) + (semiAxis2 * sinU * yz)
        };
    }
};

struct BSplineCurveWithKnots : Curve {
    int degree = 0;
    std::vector<Point3D> controlPoints;
    std::vector<double> knots;
    std::vector<double> weights;
    std::vector<int> multiplicities;
    bool closed = false;
    bool selfIntersect = false;
    Point3D evaluate(double u) const override {
        return {};
    }
};

struct Surface : StepEntity {
    virtual Point3D evaluate(double u, double v) const = 0;
};

struct Plane : Surface {
    Axis2Placement3D position;
    Point3D evaluate(double u, double v) const override {
        return {
            position.location.x + u * position.refDirection.x
                                  + v * position.axis.x,
            position.location.y + u * position.refDirection.y
                                  + v * position.axis.y,
            position.location.z + u * position.refDirection.z
                                  + v * position.axis.z
        };
    }
};

struct CylindricalSurface : Surface {
    Axis2Placement3D position;
    double radius = 1.0;
    Point3D evaluate(double u, double v) const override {
        return {};
    }
};

struct ConicalSurface : Surface {
    Axis2Placement3D position;
    double radius = 1.0;
    double semiAngle = 0.0;
    Point3D evaluate(double u, double v) const override {
        return {};
    }
};

struct SphericalSurface : Surface {
    Axis2Placement3D position;
    double radius = 1.0;
    Point3D evaluate(double u, double v) const override {
        return {};
    }
};

struct ToroidalSurface : Surface {
    Axis2Placement3D position;
    double majorRadius = 1.0;
    double minorRadius = 0.25;
    Point3D evaluate(double u, double v) const override {
        return {};
    }
};

struct BSplineSurfaceWithKnots : Surface {
    int uDegree = 0;
    int vDegree = 0;
    std::vector<std::vector<Point3D>> controlPoints;
    std::vector<int> uKnotMultiplicities;
    std::vector<int> vKnotMultiplicities;
    std::vector<double> uKnots;
    std::vector<double> vKnots;
    std::vector<double> weights;
    bool uClosed = false;
    bool vClosed = false;
    bool selfIntersect = false;
    Point3D evaluate(double u, double v) const override {
        return {};
    }
};

// Topology entities
struct Vertex : StepEntity {
    std::shared_ptr<Point> point = nullptr;
};

struct Edge : StepEntity {
    std::shared_ptr<Vertex> start = nullptr;
    std::shared_ptr<Vertex> end = nullptr;
    std::shared_ptr<Curve> curve = nullptr;
    double uStart;
    double uEnd;
};

struct OrientedEdge : StepEntity {
    std::shared_ptr<Edge> edge = nullptr;
    bool orientation = false;
};

struct Loop : StepEntity {
    std::vector<std::shared_ptr<OrientedEdge>> edges;
};

struct FaceBound : StepEntity {
    std::shared_ptr<Loop> bound = nullptr;
    bool orientation = true;
};

struct FaceOuterBound : FaceBound {};

struct Face : StepEntity {
    std::vector<std::shared_ptr<FaceBound>> bounds;
    bool sameSense;
};

struct Shell : StepEntity {    
    std::vector<std::shared_ptr<Face>> faces;
    std::vector<Plane> planes;
    std::vector<CylindricalSurface> cylinders;
    std::vector<BSplineSurfaceWithKnots> bsplineSurfaces;
    bool isClosed;
    bool containsMaterial;
};

#endif // BREP_H