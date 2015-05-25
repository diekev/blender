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
};

#endif /* __OPENVDB_PRIMITIVE_H__ */
