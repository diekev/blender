#include "openvdb_primitive.h"

OpenVDBPrimitive::OpenVDBPrimitive()
{}

OpenVDBPrimitive::~OpenVDBPrimitive()
{}

openvdb::GridBase &OpenVDBPrimitive::getGrid()
{
	return *m_grid;
}

const openvdb::GridBase &OpenVDBPrimitive::getConstGrid() const
{
	return *m_grid;
}

openvdb::GridBase::Ptr OpenVDBPrimitive::getGridPtr()
{
	return m_grid;
}

openvdb::GridBase::ConstPtr OpenVDBPrimitive::getConstGridPtr() const
{
	return m_grid;
}

void OpenVDBPrimitive::setGrid(openvdb::GridBase::Ptr grid)
{
	m_grid = grid->copyGrid();
}

static openvdb::Mat4R convertMatrix(const float mat[4][4])
{
	return openvdb::Mat4R(
	            mat[0][0], mat[0][1], mat[0][2], mat[0][3],
	            mat[1][0], mat[1][1], mat[1][2], mat[1][3],
	            mat[2][0], mat[2][1], mat[2][2], mat[2][3],
	            mat[3][0], mat[3][1], mat[3][2], mat[3][3]);
}

/* A simple protection to avoid crashes for cases when something goes wrong for
 * some reason in the matrix creation. */
static openvdb::math::MapBase::Ptr createAffineMap(const float mat[4][4])
{
	using namespace openvdb::math;
	MapBase::Ptr transform;

	try {
		transform.reset(new AffineMap(convertMatrix(mat)));
	}
	catch (const openvdb::ArithmeticError &e) {
		std::cerr << e.what() << "\n";
		transform.reset(new AffineMap());
	}

	return transform;
}

void OpenVDBPrimitive::setTransform(const float mat[4][4])
{
	using namespace openvdb::math;

	Transform::Ptr transform = Transform::Ptr(new Transform(createAffineMap(mat)));
	m_grid->setTransform(transform);
}

OpenVDBGeom::OpenVDBGeom(const size_t num_points, const size_t num_faces)
{
	m_points.reserve(num_points);
	m_polys.reserve(num_faces);
}

OpenVDBGeom::~OpenVDBGeom()
{
	m_points.clear();
	m_polys.clear();
	m_tris.clear();
}

void OpenVDBGeom::addPoint(const openvdb::Vec3s point)
{
	m_points.push_back(point);
}

void OpenVDBGeom::addQuad(const openvdb::Vec4I quad)
{
	m_polys.push_back(quad);
}

void OpenVDBGeom::addTriangle(const openvdb::v3_0_0::Vec3I tri)
{
	m_tris.push_back(tri);
}

openvdb::Vec3s OpenVDBGeom::point(const size_t idx) const
{
	return m_points[idx];
}

openvdb::Vec4I OpenVDBGeom::quad(const size_t idx) const
{
	return m_polys[idx];
}

openvdb::Vec3I OpenVDBGeom::triangle(const size_t idx) const
{
	return m_tris[idx];
}

size_t OpenVDBGeom::numPoints() const
{
	return m_points.size();
}

size_t OpenVDBGeom::numQuads() const
{
	return m_polys.size();
}

size_t OpenVDBGeom::numTriangles() const
{
	return m_tris.size();
}

std::vector<openvdb::Vec4I> OpenVDBGeom::getPolys() const
{
	return m_polys;
}
