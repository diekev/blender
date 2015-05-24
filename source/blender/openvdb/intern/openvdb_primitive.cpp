#include "openvdb_primitive.h"

OpenVDBPrimitive::OpenVDBPrimitive()
{

}

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
