/*
 * Copyright 2016 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __UTIL_LEVELSETS_H__
#define __UTIL_LEVELSETS_H__

#include <stdio.h>

namespace openvdb {
OPENVDB_USE_VERSION_NAMESPACE
namespace OPENVDB_VERSION_NAME {
namespace tools {

//////////////////////////////////////// CyclesLinearSearchImpl ////////////////////////////////////////


/// @brief Implements linear iterative search for an iso-value of
/// the level set along along the direction of the ray. Provides customized functionality for
/// cycles renderer
///
/// @note Since this class is used internally in
/// LevelSetRayIntersector (define above) and LevelSetHDDA (defined below)
/// client code should never interact directly with its API. This also
/// explains why we are not concerned with the fact that several of
/// its methods are unsafe to call unless roots were already detected.
///
/// @details It is approximate due to the limited number of iterations
/// which can can be defined with a template parameter. However the default value
/// has proven surprisingly accurate and fast. In fact more iterations
/// are not guaranteed to give significantly better results.
///
/// @warning Since the root-searching algorithm is approximate
/// (first-order) it is possible to miss intersections if the
/// iso-value is too close to the inside or outside of the narrow
/// band (typically a distance less then a voxel unit).
///
/// @warning Since this class internally stores a ValueAccessor it is NOT thread-safe,
/// so make sure to give each thread its own instance.  This of course also means that
/// the cost of allocating an instance should (if possible) be amortized over
/// as many ray intersections as possible.
template<typename GridT, typename StencilT, int Iterations, typename RealT>
class CyclesLinearSearchImpl
{
public:
	typedef math::Ray<RealT>              RayT;
	typedef typename GridT::ValueType     ValueT;
	typedef typename GridT::ConstAccessor AccessorT;
	typedef math::Vec3<RealT>             Vec3T;

	/// @brief Constructor from a grid.
	/// @throw RunTimeError if the grid is empty.
	/// @throw ValueError if the isoValue is not inside the narrow-band.
	CyclesLinearSearchImpl(const GridT& grid, const ValueT& isoValue = zeroVal<ValueT>())
		: mStencil(grid),
		  mIsoValue(isoValue),
		  mMinValue(isoValue - ValueT(2 * grid.voxelSize()[0])),
		  mMaxValue(isoValue + ValueT(2 * grid.voxelSize()[0])),
		  mDX(grid.voxelSize()[0])
	{
		if ( grid.empty() ) {
			OPENVDB_THROW(RuntimeError, "LinearSearchImpl does not supports empty grids");
		}
		if (mIsoValue<= -grid.background() ||
			mIsoValue>=  grid.background() ){
			OPENVDB_THROW(ValueError, "The iso-value must be inside the narrow-band!");
		}
		grid.tree().root().evalActiveBoundingBox(mBBox, /*visit individual voxels*/false);
	}

	/// @brief Return the iso-value used for ray-intersections
	const ValueT& getIsoValue() const { return mIsoValue; }

	/// @brief Return @c false the ray misses the bbox of the grid.
	/// @param iRay Ray represented in index space.
	/// @warning Call this method before the ray traversal starts.
	inline bool setIndexRay(const RayT& iRay)
	{
		mRay = iRay;
		return mRay.clip(mBBox);//did it hit the bbox
	}

	/// @brief Return @c false the ray misses the bbox of the grid.
	/// @param wRay Ray represented in world space.
	/// @warning Call this method before the ray traversal starts.
	inline bool setWorldRay(const RayT& wRay)
	{
		mRay = wRay.worldToIndex(mStencil.grid());
		return mRay.clip(mBBox);//did it hit the bbox
	}

	/// @brief Get the intersection point in index space.
	/// @param xyz The position in index space of the intersection.
	inline void getIndexPos(Vec3T& xyz) const { xyz = mRay(mTime); }

	/// @brief Get the intersection point in world space.
	/// @param xyz The position in world space of the intersection.
	inline void getWorldPos(Vec3T& xyz) const { xyz = mStencil.grid().indexToWorld(mRay(mTime)); }

	/// @brief Get the intersection point and normal in world space
	/// @param xyz The position in world space of the intersection.
	/// @param nml The surface normal in world space of the intersection.
	inline void getWorldPosAndNml(Vec3T& xyz, Vec3T& nml)
	{
		this->getIndexPos(xyz);
		mStencil.moveTo(xyz);
		nml = mStencil.gradient(xyz);
		nml.normalize();
		xyz = mStencil.grid().indexToWorld(xyz);
	}

	/// @brief Return the time of intersection along the index ray.
	inline RealT getIndexTime() const { return mTime; }

	/// @brief Return the time of intersection along the world ray.
	inline RealT getWorldTime() const
	{
		return mTime*mStencil.grid().transform().baseMap()->applyJacobian(mRay.dir()).length();
	}

private:

	/// @brief Initiate the local voxel intersection test.
	/// @warning Make sure to call this method before the local voxel intersection test.
	inline void init(RealT t0)
	{
		mT[0] = t0;
		mV[0] = static_cast<ValueT>(this->interpValue(t0));
	}

	inline void setRange(RealT t0, RealT t1) { mRay.setTimes(t0, t1); }

	/// @brief Return a const reference to the ray.
	inline const RayT& ray() const { return mRay; }

	/// @brief Return true if a node of the the specified type exists at ijk.
	template <typename NodeT>
	inline bool hasNode(const Coord& ijk)
	{
		return mStencil.accessor().template probeConstNode<NodeT>(ijk) != NULL;
	}

	/// @brief Return @c true if an intersection is detected.
	/// @param ijk Grid coordinate of the node origin or voxel being tested.
	/// @param time Time along the index ray being tested.
	/// @warning Only if and intersection is detected is it safe to
	/// call getIndexPos, getWorldPos and getWorldPosAndNml!
	inline bool operator()(const Coord& ijk, RealT time)
	{
		// Implementation is derived from the PhysBAM Bisection_Secant_Root code
		ValueT V;
		if (mStencil.accessor().probeValue(ijk, V) && V > mMinValue && V < mMaxValue) {
			mT[1] = time;
			mV[1] = static_cast<ValueT>(this->interpValue(time));

			if (math::ZeroCrossing(mV[0], mV[1])) {
				const RealT tolerance = 1e-6;
				const int max_iterations = 100;

				RealT a=mT[0];
				RealT b=mT[1];
				int iterations=0;
				ValueT Fa=mV[0];
				ValueT Fb=mV[1];
				RealT x_old=a;
				RealT x=b;
				ValueT Fx_old=Fa;
				ValueT Fx=Fb;

				while(b - a > tolerance && iterations++ < max_iterations ) {
					/* bisection method */
					if(abs(Fx - Fx_old) < tolerance) {
						RealT m = 0.5 * (a + b);
						ValueT Fm = static_cast<ValueT>(this->interpValue(m));

						if(Fa * Fm <= 0) {
							b = m;
							Fb = Fm;
						}
						else {
							a = m;
							Fa = Fm;
						}

						/* Update secant points */
						x_old = a;
						x = b;
						Fx_old = Fa;
						Fx = Fb;
					}
					/* secant method */
					else {
						RealT x_temp = x;
						x -= Fx * (x - x_old) / (Fx - Fx_old);

						/* Update bisection points */
						if(a < x && x < b) {
							x_old=x_temp;
							Fx_old=Fx;
							Fx=static_cast<ValueT>(this->interpValue(x));
							RealT m=x;
							ValueT Fm=Fx;

							if(Fa * Fm <= 0) {
								b = m;
								Fb = Fm;
							}
							else {
								a = m;
								Fa = Fm;
							}
						}
						/* throw out secant root - do bisection instead */
						else {
							RealT m = 0.5 * (a + b);
							ValueT Fm = static_cast<ValueT>(this->interpValue(m));

							if(Fa * Fm <= 0) {
								b = m;
								Fb = Fm;
							}
							else {
								a = m;
								Fa = Fm;
							}

							x_old = a;
							x = b;
							Fx_old = Fa;
							Fx = Fb;
						}
					}
				}

				if(iterations == max_iterations) {
					printf("max iterations reached, tolerance not reached.");
				}

				mTime = x;
				return true;
			}

			mT[0] = mT[1];
			mV[0] = mV[1];
		}

		return false;
	}

	inline RealT interpTime()
	{
		assert(math::isApproxLarger(mT[1], mT[0], RealT(1e-6)));
		return mT[0]+(mT[1]-mT[0])*mV[0]/(mV[0]-mV[1]);
	}

	inline RealT interpValue(RealT time)
	{
		const Vec3T pos = mRay(time);
		mStencil.moveTo(pos);
		return mStencil.interpolation(pos) - mIsoValue;
	}

	template<typename, int> friend struct math::LevelSetHDDA;

	RayT            mRay;
	StencilT        mStencil;
	RealT           mTime;//time of intersection
	ValueT          mV[2];
	RealT           mT[2];
	const ValueT    mIsoValue, mMinValue, mMaxValue, mDX;
	math::CoordBBox mBBox;
};// CyclesLinearSearchImpl
} // namespace math
} // namespace OPENVDB_VERSION_NAME
} // namespace openvdb


#endif // __UTIL_LEVELSETS_H__
