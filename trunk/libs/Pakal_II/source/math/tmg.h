#ifndef TMG_H
#define TMG_H

#include "tm.h"

namespace Pakal
{
	namespace tmg
	{
		/// Pi value
		const double pi = 3.1415926535897932384626433832795;
		const float PI = 3.14159265359f;
		
		//! Constant for 64bit PI.
		const double PI64 = 3.1415926535897932384626433832795028841971693993751;

		//! Constant for 64bit reciprocal of PI.
		const double RECIPROCAL_PI64 = 1.0/PI64;

		//! 32bit Constant for converting from degrees to radians
		const float DEGTORAD = PI / 180.0f;

		//! 32bit constant for converting from radians to degrees (formally known as GRAD_PI)
		const float RADTODEG   = 180.0f / PI;

		//! 64bit constant for converting from degrees to radians (formally known as GRAD_PI2)
		const double DEGTORAD64 = PI64 / 180.0;

		//! 64bit constant for converting from radians to degrees
		const double RADTODEG64 = 180.0 / PI64;

		/// Converts degres to radian
		template<typename T>
		inline const T d2r(const T a)
		{
			return (a * static_cast<T>(pi)) / static_cast<T>(180.0);
		}

		/// Converts radian to degres
		template<typename T>
		inline const T r2d(const T a)
		{
			return (a * static_cast<T>(180.0f)) / static_cast<T>(pi);
		}

		/// Calculate a quaternion that represents a rotation around an arbitrary axis
		template<class T>
		void rot(const T angle, const tmath::vectorn<T,3>& axis, tmath::quaternion<T>& q)
		{
			 T rad = d2r(angle * static_cast<T>(0.5));
			q.w = cos(rad);
			q.x = axis.x * sin(rad);
			q.y = axis.y * sin(rad);
			q.z = axis.z * sin(rad);
		}

		/// Calculate a quaternion that represents a rotation around the x axis
		template<class T>
		void rotx(const T angle, tmath::quaternion<T>& q)
		{
			 T rad = d2r(angle * static_cast<T>(0.5));
			q.w = cos(rad);
			q.x = sin(rad);
			q.y = static_cast<T>(0.0);
			q.z = static_cast<T>(0.0);
		}

		/// Calculate a quaternion that represents a rotation around the y axis
		template<class T>
		void roty(const T angle, tmath::quaternion<T>& q)
		{
			 T rad = d2r(angle * static_cast<T>(0.5));
			q.w = cos(rad);
			q.x = static_cast<T>(0.0);
			q.y = sin(rad);
			q.z = static_cast<T>(0.0);
		}

		/// Calculate a quaternion that represents a rotation around the z axis
		template<class T>
		void rotz(const T angle, tmath::quaternion<T>& q)
		{
			 T rad = d2r(angle * static_cast<T>(0.5));
			q.w = cos(rad);
			q.x = static_cast<T>(0.0);
			q.y = static_cast<T>(0.0);
			q.z = sin(rad);
		}

		//! Rotates outVector by a specified number of degrees around the Y axis and the specified center.		
		template<class T>
		void rotateXZBy(tmath::vectorn<T, 3>& outVector, double degrees, const tmath::vectorn<T, 3>& center = tmath::vectorn<T, 3>())
		{
			degrees *= DEGTORAD64;
			double cs = cos(degrees);
			double sn = sin(degrees);
			outVector.x -= center.x;
			outVector.z -= center.z;
			auto x = outVector.x;
			auto z = outVector.z;
			
			outVector.x = static_cast<T>(x*cs -z*sn);
			outVector.z = static_cast<T>(x*sn + z*cs);

			outVector.x += center.x;
			outVector.z += center.z;
		}

		//! Rotates outVector by a specified number of degrees around the Z axis and the specified center.		
		template<class T>
		void rotateXYBy(tmath::vectorn<T, 3>& outVector, double degrees, const tmath::vectorn<T, 3>& center = tmath::vectorn<T, 3>())
		{
			degrees *= DEGTORAD64;
			double cs = cos(degrees);
			double sn = sin(degrees);
			outVector.x -= center.x;
			outVector.y -= center.y;
			auto x = outVector.x;
			auto y = outVector.y;
			outVector.x = static_cast<T>(x*cs - y*sn);
			outVector.y = static_cast<T>(x*sn + y*cs);
				
			outVector.x += center.x;
			outVector.y += center.y;
		}

		//! Rotates outVector by a specified number of degrees around the X axis and the specified center.		
		template<class T>
		void rotateYZBy(tmath::vectorn<T, 3>& outVector, double degrees, const tmath::vectorn<T, 3>& center = tmath::vectorn<T, 3>())
		{
			degrees *= DEGTORAD64;
			double cs = cos(degrees);
			double sn = sin(degrees);
			outVector.z -= center.z;
			outVector.y -= center.y;
			auto z = outVector.z;
			auto y = outVector.y;
			outVector.y = static_cast<T>(y*cs - z*sn);
			outVector.z = static_cast<T>(y*sn + z*cs);

			outVector.z += center.z;
			outVector.y += center.y;
		}

		/// Converts a quaternion orientation into euler angles
		template<class T>
		void q2e(const tmath::quaternion<T>& q, T& anglex, T& angley, T& anglez)
		{
			anglex = atan(2 * (q.y * q.z + q.w * q.x) / (q.w * q.w - q.x * q.w - q.y * q.y + q.z * q.z));
			angley = asin(-2 * (q.x * q.z - q.w * q.y));
			anglez = atan(2 * (q.x * q.y + q.w * q.z) / (q.w * q.w + q.x * q.x - q.y * q.y - q.z * q.z));
		}

		/// Converts euler angles into a quaternion orienation
		template<typename T>
		tmath::quaternion<T> e2q(T yaw, T pitch, T roll)
		{
			T cosYaw = cos(yaw / static_cast<T>(2.0));
			T sinYaw = sin(yaw / static_cast<T>(2.0));
			T cosPitch = cos(pitch / static_cast<T>(2.0));
			T sinPitch = sin(pitch / static_cast<T>(2.0));
			T cosRoll = cos(roll / static_cast<T>(2.0));
			T sinRoll = sin(roll / static_cast<T>(2.0));
			return tmath::quaternion<T>(cosRoll * sinPitch * cosYaw + sinRoll * cosPitch * sinYaw,
			                            cosRoll * cosPitch * sinYaw - sinRoll * sinPitch * cosYaw,
			                            sinRoll * cosPitch * cosYaw - cosRoll * sinPitch * sinYaw,
			                            cosRoll * cosPitch * cosYaw + sinRoll * sinPitch * sinYaw);
		}

		/// Converts euler angles into a quaternion orienation
		template<typename T>
		void e2q(T yaw, T pitch, T roll, tmath::quaternion<T>& q)
		{
			T cosYaw = cos(yaw / static_cast<T>(2.0));
			T sinYaw = sin(yaw / static_cast<T>(2.0));
			T cosPitch = cos(pitch / static_cast<T>(2.0));
			T sinPitch = sin(pitch / static_cast<T>(2.0));
			T cosRoll = cos(roll / static_cast<T>(2.0));
			T sinRoll = sin(roll / static_cast<T>(2.0));
			q.x = cosRoll * sinPitch * cosYaw + sinRoll * cosPitch * sinYaw;
			q.y = cosRoll * cosPitch * sinYaw - sinRoll * sinPitch * cosYaw;
			q.z = sinRoll * cosPitch * cosYaw - cosRoll * sinPitch * sinYaw;
			q.w = cosRoll * cosPitch * cosYaw + sinRoll * sinPitch * sinYaw;
		}


		/// Create a quaternion that represents a spherical interpolation
		template<class T>
		tmath::quaternion<T> slerp(tmath::quaternion<T>& q1, tmath::quaternion<T>& q2, T t)
		{
			tmath::quaternion<T> qInterpolated;
			if (q1.x == q2.x && q1.y == q2.y && q1.z == q2.z && q1.w == q2.w)
				return q1;

			T result = (q1.x * q2.x) + (q1.y * q2.y) + (q1.z * q2.z) + (q1.w * q2.w);

			if (result < 0.0f)
			{
				q2 = quaternion(-q2.x,-q2.y,-q2.z,-q2.w);
				result = -result;
			}

			T scale0 = 1 - t, scale1 = t;
			if (1 - result > 0.1f)
			{
				T theta = (T)acos(result);
				T sinTheta = (T)sin(theta);

				scale0 = (T)sin((1 - t) * theta) / sinTheta;
				scale1 = (T)sin((t * theta)) / sinTheta;
			}

			qInterpolated.x = (scale0 * q1.x) + (scale1 * q2.x);
			qInterpolated.y = (scale0 * q1.y) + (scale1 * q2.y);
			qInterpolated.z = (scale0 * q1.z) + (scale1 * q2.z);
			qInterpolated.w = (scale0 * q1.w) + (scale1 * q2.w);

			return qInterpolated;
		}

		// ----------------------------------------------------------------------------
		// matrix3 functions
		//
		/// Creates a matrix<T,3,3> that represents a rotation around the x axis
		template<typename T>
		void rotx(const T& angle, tmath::matrix<T,3,3>& m)
		{
			 T rad = d2r(angle);
			 T c = cos(rad);
			 T s = sin(rad);
			m.xx = 1.0;
			m.xy = 0.0;
			m.xz = 0.0;
			m.yx = 0.0;
			m.yy = c;
			m.yz = -s;
			m.zx = 0.0;
			m.zy = s;
			m.zz = c;
		}

		/// Creates a matrix<T,3,3> that represents a rotation around the y axis
		template<typename T>
		void roty(const T& angle, tmath::matrix<T,3,3>& m)
		{
			 T rad = d2r(angle);
			 T c = cos(rad);
			 T s = sin(rad);
			m.xx = c;
			m.xy = 0.0;
			m.xz = s;
			m.yx = 0.0;
			m.yy = 1;
			m.yz = 0;
			m.zx = -s;
			m.zy = 0;
			m.zz = c;
		}

		/// Creates a matrix<T,3,3> that represents a rotation around the z axis
		template<typename T>
		void rotz(const T& angle, tmath::matrix<T,3,3>& m)
		{
			 T rad = d2r(angle);
			 T c = cos(rad);
			 T s = sin(rad);
			m.xx = c;
			m.xy = -s;
			m.xz = 0.0;
			m.yx = s;
			m.yy = c;
			m.yz = 0.0;
			m.zx = 0.0;
			m.zy = 0.0;
			m.zz = 1.0;
		}

		/// Creates a matrix<T,3,3> that represents a rotation around an arbitrary axis
		template<typename T>
		void rot(const T& angle, const tmath::vectorn<T,3>& axis, tmath::matrix<T,3,3>& pM)
		{
			T rad = d2r(angle);
			T c = cos(rad);
			T s = sin(rad);
			tmath::vectorn<T,3> v = axis;
			tmath::normalize(v);
			T xy = v.x * v.y;
			T yz = v.y * v.z;
			T zx = v.z * v.x;
			T xs = v.x * s;
			T ys = v.y * s;
			T zs = v.z * s;
			T tt = static_cast<T>(1.0) - c;

			pM.xx = tt * v.x * v.x + c;
			pM.xy = tt * xy - zs;
			pM.xz = tt * zx + ys;

			pM.yx = tt * xy + zs;
			pM.yy = tt * v.y * v.y + c;
			pM.yz = tt * yz - xs;

			pM.zx = tt * zx - ys;
			pM.zy = tt * yz + xs;
			pM.zz = tt * v.z * v.z + c;
		}

		// ----------------------------------------------------------------------------
		// matrix3 functions
		//

		/// Creates a matrix<T,4,4> that represents a rotation around the x axis
		template<typename T>
		void rotx(const T& angle, tmath::matrix<T,4,4>& m)
		{
			 T rad = d2r(angle);
			 T c = static_cast<T>(cos(rad));
			 T s = static_cast<T>(sin(rad));
			m.xx = 1.0;
			m.xy = 0.0;
			m.xz = 0.0;
			m.xw = 0.0;
			m.yx = 0.0;
			m.yy = c;
			m.yz = -s;
			m.yw = 0.0;
			m.zx = 0.0;
			m.zy = s;
			m.zz = c;
			m.zw = 0.0;
			m.wx = 0.0;
			m.wy = 0.0;
			m.wz = 0.0;
			m.ww = 1.0;
		}

		/// Creates a matrix<T,4,4> that represents a rotation around the y axis
		template<typename T>
		void roty(const T& angle, tmath::matrix<T,4,4>& m)
		{
			 T rad = d2r(angle);
			 T c = static_cast<T>(cos(rad));
			 T s = static_cast<T>(sin(rad));
			m.xx = c;
			m.xy = 0.0;
			m.xz = s;
			m.xw = 0.0;
			m.yx = 0.0;
			m.yy = 1;
			m.yz = 0;
			m.yw = 0.0;
			m.zx = -s;
			m.zy = 0;
			m.zz = c;
			m.zw = 0.0;
			m.wx = 0.0;
			m.wy = 0.0;
			m.wz = 0.0;
			m.ww = 1.0;
		}

		/// Creates a matrix<T,4,4> that represents a rotation around the z axis
		template<typename T>
		void rotz(const T& angle, tmath::matrix<T,4,4>& m)
		{
			 T rad = d2r(angle);
			 T c = static_cast<T>(cos(rad));
			 T s = static_cast<T>(sin(rad));
			m.xx = c;
			m.xy = -s;
			m.xz = 0.0;
			m.xw = 0.0;
			m.yx = s;
			m.yy = c;
			m.yz = 0.0;
			m.yw = 0.0;
			m.zx = 0.0;
			m.zy = 0.0;
			m.zz = 1.0;
			m.zw = 0.0;
			m.wx = 0.0;
			m.wy = 0.0;
			m.wz = 0.0;
			m.ww = 1.0;
		}

		/// Creates a matrix<T,4,4> that represents a rotation around an arbitrary axis
		template<typename T>
		void rot(const T& angle, const tmath::vectorn<T,3>& axis, tmath::matrix<T,4,4>& pM)
		{
			T rad = d2r(angle);
			T c = static_cast<T>(cos(rad));
			T s = static_cast<T>(sin(rad));
			tmath::vectorn<T,3> v = axis;
			tmath::normalize(v);
			T xy = v.x * v.y;
			T yz = v.y * v.z;
			T zx = v.z * v.x;
			T xs = v.x * s;
			T ys = v.y * s;
			T zs = v.z * s;
			T tt = static_cast<T>(1.0) - c;

			pM.xx = tt * v.x * v.x + c;
			pM.xy = tt * xy - zs;
			pM.xz = tt * zx + ys;
			pM.xw = 0.0;

			pM.yx = tt * xy + zs;
			pM.yy = tt * v.y * v.y + c;
			pM.yz = tt * yz - xs;
			pM.yw = 0.0;

			pM.zx = tt * zx - ys;
			pM.zy = tt * yz + xs;
			pM.zz = tt * v.z * v.z + c;
			pM.zw = 0.0;

			pM.wx = 0.0;
			pM.wy = 0.0;
			pM.wz = 0.0;
			pM.ww = 1.0;
		}

		/// Creates a matrix<T,4,4> that represents a translation
		template<typename T>
		void transl(const T x, const T y, const T z, tmath::matrix<T,4,4>& m)
		{
			m.xx = 1.0;
			m.xy = 0.0;
			m.xz = 0.0;
			m.xw = x;
			m.yx = 0.0;
			m.yy = 1.0;
			m.yz = 0.0;
			m.yw = y;
			m.zx = 0.0;
			m.zy = 0.0;
			m.zz = 1.0;
			m.zw = z;
			m.wx = 0.0;
			m.wy = 0.0;
			m.wz = 0.0;
			m.ww = 1.0;
		}

		/// Creates a matrix<T,4,4> that represents a translation
		template<typename T>
		void transl(const tmath::vectorn<T,3>& v, tmath::matrix<T,4,4>& m)
		{
			m.xx = 1.0;
			m.xy = 0.0;
			m.xz = 0.0;
			m.xw = v.x;
			m.yx = 0.0;
			m.yy = 1.0;
			m.yz = 0.0;
			m.yw = v.y;
			m.zx = 0.0;
			m.zy = 0.0;
			m.zz = 1.0;
			m.zw = v.z;
			m.wx = 0.0;
			m.wy = 0.0;
			m.wz = 0.0;
			m.ww = 1.0;
		}

		/// Creates a matrix<T,4,4> that represents a translation
		template<typename T>
		tmath::matrix<T,4,4> transl(const tmath::vectorn<T,3>& v)
		{
			return tmath::matrix<T,4,4>(1.0,0.0,0.0,v.x,
			                            0.0,1.0,0.0,v.y,
			                            0.0,0.0,1.0,v.z,
			                            0.0,0.0,0.0,1.0);
		}


		/// Creates a matrix<T,4,4> that represents a scaling
		template<typename T>
		void scale(const tmath::vectorn<T,3>& v, tmath::matrix<T,4,4>& m)
		{
			m.xx = v.x;
			m.xy = 0.0;
			m.xz = 0.0;
			m.xw = 0.0;
			m.yx = 0.0;
			m.yy = v.y;
			m.yz = 0.0;
			m.yw = 0.0;
			m.zx = 0.0;
			m.zy = 0.0;
			m.zz = v.z;
			m.zw = 0.0;
			m.wx = 0.0;
			m.wy = 0.0;
			m.wz = 0.0;
			m.ww = 1.0;
		}

		template<typename T>
		tmath::matrix<T,4,4>& frustum(T fleft, T fright, T fbottom, T ftop, T fnear, T ffar, tmath::matrix<T,4,4>& pSrc)
		{
			pSrc.xx = 2.0f * fnear / (fright - fleft);
			pSrc.xy = 0.0f;
			pSrc.xz = (fright + fleft) / (fright - fleft);
			pSrc.xw = 0.0f;
			pSrc.yx = 0.0f;
			pSrc.yy = 2.0f * fnear / (ftop - fbottom);
			pSrc.yz = (ftop + fbottom) / (ftop - fbottom);
			pSrc.yw = 0.0f;
			pSrc.zx = 0.0f;
			pSrc.zy = 0.0f;
			pSrc.zz = -(ffar + fnear) / (ffar - fnear);
			pSrc.zw = -2.0f * ffar * fnear / (ffar - fnear);
			pSrc.wx = 0.0f;
			pSrc.wy = 0.0f;
			pSrc.wz = -1.0f;
			pSrc.ww = 0.0f;
			return pSrc;
		}

		template<typename T>
		tmath::matrix<T,4,4> frustum(T fleft, T fright, T fbottom, T ftop, T fnear, T ffar)
		{
			return tmath::matrix<T,4,4>(2.0f * fnear / (fright - fleft),0.0f,(fright + fleft) / (fright - fleft),0.0f,
			                            0.0f,2.0f * fnear / (ftop - fbottom),(ftop + fbottom) / (ftop - fbottom),0.0f,
			                            0.0f,0.0f,-(ffar + fnear) / (ffar - fnear),-2.0f * ffar * fnear / (ffar - fnear),
			                            0.0f,0.0f,-1.0f,0.0f);
		}

		template<typename T>
		tmath::matrix<T,4,4>& ortho(T fleft, T fright, T fbottom, T ftop, T fnear, T ffar, tmath::matrix<T,4,4>& pSrc)
		{
			pSrc.xx = 2.0f / (fright - fleft);
			pSrc.xy = 0.0f;
			pSrc.xz = 0.0f;
			pSrc.xw = -(fright + fleft) / (fright - fleft);
			pSrc.yx = 0.0f;
			pSrc.yy = 2.0f / (ftop - fbottom);
			pSrc.yz = 0.0f;
			pSrc.yw = -(ftop + fbottom) / (ftop - fbottom);
			pSrc.zx = 0.0f;
			pSrc.zy = 0.0f;
			pSrc.zz = -2.0f / (ffar - fnear);
			pSrc.zw = -(ffar + fnear) / (ffar - fnear);
			pSrc.wx = 0.0f;
			pSrc.wy = 0.0f;
			pSrc.wz = 0.0f;
			pSrc.ww = 1.0f;
			return pSrc;
		}

		template<typename T>
		tmath::matrix<T,4,4> ortho(T fleft, T fright, T fbottom, T ftop, T fnear, T ffar)
		{
			return tmath::matrix<T,4,4>(2.0f / (fright - fleft),0.0f,0.0f,-(fright + fleft) / (fright - fleft),
			                            0.0f,2.0f / (ftop - fbottom),0.0f,-(ftop + fbottom) / (ftop - fbottom),
			                            0.0f,0.0f,-2.0f / (ffar - fnear),-(ffar + fnear) / (ffar - fnear),
			                            0.0f,0.0f,0.0,1.0f);
		}

		template<typename T>
		tmath::matrix<T,4,4> lookat(tmath::vectorn<T,3>& eye, tmath::vectorn<T,3>& center, tmath::vectorn<T,3>& up)
		{
			tmath::vectorn<float,3> forward, side;
			tmath::matrix<float,4,4> m, trans;
			// Calculate the direction of the view.
			forward = center - eye;
			tmath::normalize(forward);

			// Calculate the positive right coordinate
			tmath::cross(forward,up,side);
			tmath::normalize(side);

			// Now we must recompute the up vector to ajust
			// it to the new right vector.
			tmath::cross(side,forward,up);

			// Fill the right information into the matrix
			m.xx = side.x;
			m.xy = side.y;
			m.xz = side.z;

			m.yx = up.x;
			m.yy = up.y;
			m.yz = up.z;

			m.zx = -forward.x;
			m.zy = -forward.y;
			m.zz = -forward.z;

			m.ww = 1.0f;
			transl(eye,trans);
			return trans;
		}
	} // End of namespace
}
#endif

