#include "spring_solver.h"
#include "drag.h"
#include "nlopt.hpp"
#include <unordered_map>

typedef std::vector<std::vector<double>> std_matrix;
namespace origamiSurface {
    typedef std::unordered_map<std::string, int> MapStrInt; 
    typedef enum {
        POLYGON,
        FACEONEARC,
        FACETWOARC
    } faceType;
} 

class Crease; 

class Vertex {
    int index; 
    std::vector<double> coords;
    bool nv; 
public : 
    std::vector<double> getCoords() { return coords; }
    int getIdx() { return index; }
    bool judgeNonVirtual() { return nv; }
    Vertex(const std::vector<double>& cds, int idx, bool nonVir = false) : 
        coords(cds), index(idx), nv(nonVir) {};
};

class Nvvertex : public Vertex {
    std::vector<Crease*> crsFromVertex; 
public : 
    Nvvertex(const std::vector<double>& cds, int idx) : 
        Vertex(cds, idx, true) {}
    void insertCrease(Crease* crs) { crsFromVertex.push_back(crs); }
    std::vector<Crease*> getCreaseOnVertex() { return crsFromVertex; }
}; 

class Crease {
    Vertex* v1_; 
    Vertex* v2_; 
    // normalized vector v1v2
    std::vector<double> dir; 
    int index1; 
    int index2; 
    double rho_T;
    std_matrix rot_matrix; 
public : 
    Crease(Vertex* v1, Vertex* v2, int idx1, int idx2, double); 
    void updateRotMatrix(double); 
    std_matrix getRotMatrix() { return rot_matrix; }
    std::vector<double> getDir() { return dir; }
    double getRotAngle() { return rho_T; }
};

class OgmPoint; 

class faceTypeSingleton {
    origamiSurface::MapStrInt mymap; 
    faceTypeSingleton(); 
    void operator=(faceTypeSingleton&); 
    faceTypeSingleton(const faceTypeSingleton&); 
public : 
    static faceTypeSingleton& instance() {
        static faceTypeSingleton fts; 
        return fts; 
    }
    origamiSurface::MapStrInt& getMap() { return mymap; } 
};

class Face {
    typedef enum {
        POLYGON, 
        FACEONEARC, 
        FACETWOARC
    } faceType; 
    // vertices constructing the face
    std::vector<Vertex*> vertices_;
    // crease lines vertices on the face will go through
    // when do origami
    std::vector<Crease*> crsAlongPath;
    // folding matrix corresponds to crease lines l1, l2, ..., lk in 
    // crsAlongPath
    std_matrix fd_matrix; 
    origamiSurface::faceType type; 
protected: 
    void setType(origamiSurface::faceType ft) { type = ft; }
    std::vector<Vertex*> getVertices() { return vertices_; }
public :
    Face(const std::vector<Vertex*>&, const std::vector<Crease*>&); 
    virtual bool poInside(OgmPoint*) = 0;
    std::vector<Crease*> getCrsAlongPath() { return crsAlongPath; }
    void crsFoldCrds(std::vector<double>&); 
    std_matrix& getFoldingMatrix() { return fd_matrix; }
    void updateFoldingMatrix(); 
    virtual ~Face() = 0; 
};

class Polygon : public Face {
public : 
    Polygon(const std::vector<Vertex*>& vv, const std::vector<Crease*>& cv) : 
        Face(vv, cv) { setType(origamiSurface::POLYGON); }
    virtual bool poInside(OgmPoint*); 
}; 

// the face with only 1 arc and others are straight lines
class FaceOneArc : public Face {
    std::vector<double> center; 
public : 
    FaceOneArc(const std::vector<Vertex*>& vv, const std::vector<Crease*>& cv, 
        const std::vector<double>& cen) : Face(vv, cv) { 
            center.resize(3); std::copy(cen.begin(), cen.end(), 
            center.begin()); setType(origamiSurface::FACEONEARC); }
    virtual bool poInside(OgmPoint*); 
};

class FaceTwoArc : public Face {
    std::vector<double> center; 
public : 
    FaceTwoArc(std::vector<Vertex*>& vv, const std::vector<Crease*>& cv, 
        const std::vector<double>& cen) : Face(vv, cv) {
        center.resize(3); std::copy(cen.begin(), cen.end(), center.begin()); 
        setType(origamiSurface::FACETWOARC); }
    virtual bool poInside(OgmPoint*); 
};

class OgmPoint : public SpringVertex
{
    std::vector<Face*> ogmFaces; 
    std::vector<double> x0;
public:
    OgmPoint(SpringVertex&);
    std::vector<Face*> getOgmFaces() { return ogmFaces; }
    void addOgmFace(Face* f) { ogmFaces.push_back(f); }
    std::vector<double> getInitialCoords() { return x0; }
};

class optAlgoSingleton {
    origamiSurface::MapStrInt mymap; 
    optAlgoSingleton(); 
    void operator=(optAlgoSingleton&); 
    optAlgoSingleton(const optAlgoSingleton&); 
public : 
    static optAlgoSingleton& instance() {
        static optAlgoSingleton oas; 
        return oas; 
    }
    origamiSurface::MapStrInt& getMap() { return mymap; }
};

class OrigamiFold : public Drag
{
public:
    void preprocess(std::vector<SpringVertex*>&);
    void postprocess(std::vector<SpringVertex*>&);
    void setVel(SpringVertex*);
    void updateVel(std::vector<SpringVertex*>& pts, double t) {}
    void setAccel(SpringVertex*);
    std::string id() {return "OrigamiDrag";}
    OrigamiFold(const std::vector<std::vector<double>>&,
                const std::vector<double>&, const std::vector<int>&,
		const std::vector<std::vector<int>>&,
                const std::vector<std::pair<int, int>>&,
                const std::vector<std::vector<int>>&, 
                const std::vector<double>&, int);
    OrigamiFold();
    ~OrigamiFold();
    size_t dataSize();
private:
    bool first = true;
    std::vector<Vertex*> vertices_;
    std::vector<Crease*> creases_; 
    std::vector<Face*> faces_; 
    std::vector<double> rho, rho_delta, rho_tau, rho_T;
    nlopt::opt *m_opt = NULL;
    void assignFacesOgmPoints(std::vector<SpringVertex*>&); 
    std::vector<double> findFoldable(std::vector<double>&, double&);
    static double staticTargetFunction(const std::vector<double>&, std::vector<double>&, void*);
    double targetFunction(const std::vector<double>&);
    void findNextFoldingAngle();
    void ogmComputeNewPosition(SpringVertex* sv, std::vector<double>&);
    bool isValid(const std::vector<double>&);
    Drag * clone(const Info&);
    size_t totalDataSize;
    int optAlgoType_; 
};

class Math
{
public:

    static void MatxMat(const std_matrix &,
                   const std_matrix &,
                   std_matrix &);

    static void MatxVec(const std_matrix &,
                   const std::vector<double>&,
                   std::vector<double>&);

    static void MatpMat(const std_matrix &,
                   const std_matrix &,
                   std_matrix &);

    static void MatmMat(const std_matrix &,
                   const std_matrix &,
                   std_matrix &);

    static void VecmVec(const std::vector<double>&,
                      const std::vector<double>&,
                      std::vector<double>&);

    static void VecpVec(const std::vector<double>&,
                      const std::vector<double>&,
                      std::vector<double>&);
    static void assignMatToMat(const std_matrix&, std_matrix&);
    static double Mag(const std::vector<double>&);
    static std_matrix Eye(int);
    static std_matrix Mat(int, int);
    static double Norm(const std_matrix &, int);
    static double Norm(const std::vector<double>&, int);
    static double dist3d(const std::vector<double>& p1,
                         const std::vector<double>& p2);
    static void Normalize(std::vector<double>& v);
    static double TriProd(const std::vector<double>& a,
                     const std::vector<double>& b,
                     const std::vector<double>& c);
    static double Mdot3d(const std::vector<double>& a,
                   const std::vector<double>& b);
    static void Mcross3d(const std::vector<double>& u,
                     const std::vector<double>& v,
                     std::vector<double>& ans);
    static bool leftOnStraightLine(const std::vector<double>&,
                const std::vector<double>&, const std::vector<double>&);
    static bool leftOnArc(const std::vector<double>&,
                const std::vector<double>&, 
                const std::vector<double>&, const std::vector<double>&); 
    static void getRotMatrix(const std::vector<double>&, 
        const std::vector<double>&, std_matrix& , double); 
    static void transpose(std_matrix&);
    static void transpose(const std_matrix&, std_matrix&);
private:
    Math();
};



