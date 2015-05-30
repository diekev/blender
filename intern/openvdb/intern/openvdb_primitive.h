#ifndef __OPENVDB_PRIMITIVE_H__
#define __OPENVDB_PRIMITIVE_H__

#include <openvdb/openvdb.h>

class OpenVDBPrimitive {
	openvdb::GridBase::Ptr m_grid;

public:
	OpenVDBPrimitive();
	~OpenVDBPrimitive();

	openvdb::GridBase &getGrid();
	const openvdb::GridBase &getConstGrid() const;
	openvdb::GridBase::Ptr getGridPtr();
	openvdb::GridBase::ConstPtr getConstGridPtr() const;

	void setGrid(openvdb::GridBase::Ptr grid);
	void setTransform(const float mat[4][4]);
};

class OpenVDBGeom {
	std::vector<openvdb::Vec3s> m_points;
	std::vector<openvdb::Vec4I> m_polys;
	std::vector<openvdb::Vec3I> m_tris;

public:
	OpenVDBGeom(const size_t num_points, const size_t num_faces);
	~OpenVDBGeom();

	void addPoint(const openvdb::Vec3s point);
	void addQuad(const openvdb::Vec4I quad);
	void addTriangle(const openvdb::Vec3I tri);

	openvdb::Vec3s point(const size_t idx) const;
	openvdb::Vec4I quad(const size_t idx) const;
	openvdb::Vec3I triangle(const size_t idx) const;

	size_t numPoints() const;
	size_t numQuads() const;
	size_t numTriangles() const;

	std::vector<openvdb::Vec4I> getPolys() const;
};

#endif /* __OPENVDB_PRIMITIVE_H__ */
