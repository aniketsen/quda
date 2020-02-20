#ifndef _REGISTER_TRAITS_H
#define _REGISTER_TRAITS_H

/**
 * @file  register_traits.h
 * @brief Provides precision abstractions and defines the register
 * precision given the storage precision using C++ traits.
 *
 */

#include <quda_internal.h>
#include <generics/ldg.h>
#include <complex_quda.h>
#include <inline_ptx.h>

namespace quda {

  /*
    Here we use traits to define the greater type used for mixing types of computation involving these types
  */
  template<class T, class U> struct PromoteTypeId { typedef T Type; };
  template<> struct PromoteTypeId<complex<float>, float> { typedef complex<float> Type; };
  template<> struct PromoteTypeId<float, complex<float> > { typedef complex<float> Type; };
  template<> struct PromoteTypeId<complex<double>, double> { typedef complex<double> Type; };
  template<> struct PromoteTypeId<double, complex<double> > { typedef complex<double> Type; };
  template<> struct PromoteTypeId<double,int> { typedef double Type; };
  template<> struct PromoteTypeId<int,double> { typedef double Type; };
  template<> struct PromoteTypeId<float,int> { typedef float Type; };
  template<> struct PromoteTypeId<int,float> { typedef float Type; };
  template<> struct PromoteTypeId<double,float> { typedef double Type; };
  template<> struct PromoteTypeId<float,double> { typedef double Type; };

  /*
    Here we use traits to define the mapping between storage type and
    register type:
    double -> double
    float -> float
    short -> float
    quarter -> float
    This allows us to wrap the encapsulate the register type into the storage template type
   */
  template<typename> struct mapper { };
  template<> struct mapper<double> { typedef double type; };
  template<> struct mapper<float> { typedef float type; };
  template<> struct mapper<short> { typedef float type; };
  template<> struct mapper<char> { typedef float type; };

  template<> struct mapper<double2> { typedef double2 type; };
  template<> struct mapper<float2> { typedef float2 type; };
  template<> struct mapper<short2> { typedef float2 type; };
  template<> struct mapper<char2> { typedef float2 type; };

  template<> struct mapper<double4> { typedef double4 type; };
  template<> struct mapper<float4> { typedef float4 type; };
  template<> struct mapper<short4> { typedef float4 type; };
  template<> struct mapper<char4> { typedef float4 type; };

  template<> struct mapper<double8> { typedef double8 type; };
  template<> struct mapper<float8> { typedef float8 type; };
  template<> struct mapper<short8> { typedef float8 type; };
  template<> struct mapper<char8> { typedef float8 type; };

  template<typename,typename> struct bridge_mapper { };
  template<> struct bridge_mapper<double2,double2> { typedef double2 type; };
  template<> struct bridge_mapper<double2,float2> { typedef double2 type; };
  template<> struct bridge_mapper<double2,short2> { typedef float2 type; };
  template<> struct bridge_mapper<double2,char2> { typedef float2 type; };
  template<> struct bridge_mapper<double2,float4> { typedef double4 type; };
  template<> struct bridge_mapper<double2,short4> { typedef float4 type; };
  template<> struct bridge_mapper<double2,char4> { typedef float4 type; };
  template<> struct bridge_mapper<float4,double2> { typedef float2 type; };
  template<> struct bridge_mapper<float4,float4> { typedef float4 type; };
  template<> struct bridge_mapper<float4,short4> { typedef float4 type; };
  template<> struct bridge_mapper<float4,char4> { typedef float4 type; };
  template<> struct bridge_mapper<float2,double2> { typedef float2 type; };
  template<> struct bridge_mapper<float2,float2> { typedef float2 type; };
  template<> struct bridge_mapper<float2,short2> { typedef float2 type; };
  template<> struct bridge_mapper<float2,char2> { typedef float2 type; };

  template<> struct bridge_mapper<double2,short8> { typedef double8 type; };
  template<> struct bridge_mapper<double2,char8> { typedef double8 type; };
  template<> struct bridge_mapper<float8,short8> { typedef float8 type; };
  template<> struct bridge_mapper<float8,char8> { typedef float8 type; };
  template<> struct bridge_mapper<float4,short8> { typedef float8 type; };
  template<> struct bridge_mapper<float4,char8> { typedef float8 type; };

  template<typename> struct vec_length { static const int value = 0; };
  template<> struct vec_length<double8> { static const int value = 8; };
  template<> struct vec_length<double4> { static const int value = 4; };
  template<> struct vec_length<double2> { static const int value = 2; };
  template<> struct vec_length<double> { static const int value = 1; };
  template<> struct vec_length<float8> { static const int value = 8; };
  template<> struct vec_length<float4> { static const int value = 4; };
  template<> struct vec_length<float2> { static const int value = 2; };
  template<> struct vec_length<float> { static const int value = 1; };
  template<> struct vec_length<short8> { static const int value = 8; };
  template<> struct vec_length<short4> { static const int value = 4; };
  template<> struct vec_length<short2> { static const int value = 2; };
  template<> struct vec_length<short> { static const int value = 1; };
  template<> struct vec_length<char8> { static const int value = 8; };
  template<> struct vec_length<char4> { static const int value = 4; };
  template<> struct vec_length<char2> { static const int value = 2; };
  template<> struct vec_length<char> { static const int value = 1; };

  template<typename, int N> struct vector { };

  template<> struct vector<double, 2> {
    typedef double2 type;
    type a;
    vector(const type &a) { this->a.x = a.x; this->a.y = a.y; }
    operator type() const { return a; }
  };

  template<> struct vector<float, 2> {
    typedef float2 type;
    float2 a;
    vector(const double2 &a) { this->a.x = a.x; this->a.y = a.y; }
    operator type() const { return a; }
  };

  template<> struct vector<int, 2> {
    typedef int2 type;
    int2 a;
    vector(const int2 &a) { this->a.x = a.x; this->a.y = a.y; }
    operator type() const { return a; }
  };

  template<typename> struct scalar { };
  template<> struct scalar<double8> { typedef double type; };
  template<> struct scalar<double4> { typedef double type; };
  template<> struct scalar<double3> { typedef double type; };
  template<> struct scalar<double2> { typedef double type; };
  template<> struct scalar<double> { typedef double type; };
  template<> struct scalar<float8> { typedef float type; };
  template<> struct scalar<float4> { typedef float type; };
  template<> struct scalar<float3> { typedef float type; };
  template<> struct scalar<float2> { typedef float type; };
  template<> struct scalar<float> { typedef float type; };
  template<> struct scalar<short8> { typedef short type; };
  template<> struct scalar<short4> { typedef short type; };
  template<> struct scalar<short3> { typedef short type; };
  template<> struct scalar<short2> { typedef short type; };
  template<> struct scalar<short> { typedef short type; };
  template<> struct scalar<char8> { typedef char type; };
  template<> struct scalar<char4> { typedef char type; };
  template<> struct scalar<char3> { typedef char type; };
  template<> struct scalar<char2> { typedef char type; };
  template<> struct scalar<char> { typedef char type; };

  /* Traits used to determine if a variable is half precision or not */
  template< typename T > struct isHalf{ static const bool value = false; };
  template<> struct isHalf<short>{ static const bool value = true; };
  template<> struct isHalf<short2>{ static const bool value = true; };
  template<> struct isHalf<short4>{ static const bool value = true; };
  template<> struct isHalf<short8>{ static const bool value = true; };

  /* Traits used to determine if a variable is quarter precision or not */
  template< typename T > struct isQuarter{ static const bool value = false; };
  template<> struct isQuarter<char>{ static const bool value = true; };
  template<> struct isQuarter<char2>{ static const bool value = true; };
  template<> struct isQuarter<char4>{ static const bool value = true; };
  template<> struct isQuarter<char8>{ static const bool value = true; };

  /* Traits used to determine if a variable is fixed precision or not */
  template< typename T > struct isFixed{ static const bool value = false; };
  template<> struct isFixed<short>{ static const bool value = true; };
  template<> struct isFixed<short2>{ static const bool value = true; };
  template<> struct isFixed<short4>{ static const bool value = true; };
  template<> struct isFixed<short8>{ static const bool value = true; };
  template<> struct isFixed<char>{ static const bool value = true; };
  template<> struct isFixed<char2>{ static const bool value = true; };
  template<> struct isFixed<char4>{ static const bool value = true; };
  template<> struct isFixed<char8>{ static const bool value = true; };

  /**
     Generic wrapper for Trig functions
  */
  template <bool isFixed, typename T>
    struct Trig {
      __device__ __host__ static T Atan2( const T &a, const T &b) { return atan2(a,b); }
      __device__ __host__ static T Sin( const T &a ) { return sin(a); }
      __device__ __host__ static T Cos( const T &a ) { return cos(a); }
      __device__ __host__ static void SinCos(const T &a, T *s, T *c) { sincos(a, s, c); }
    };
  
  /**
     Specialization of Trig functions using floats
   */
  template <>
    struct Trig<false,float> {
    __device__ __host__ static float Atan2( const float &a, const float &b) { return atan2f(a,b); }
    __device__ __host__ static float Sin(const float &a)
    {
#ifdef __CUDA_ARCH__
      return __sinf(a); 
#else
      return sinf(a);
#endif
    }
    __device__ __host__ static float Cos(const float &a)
    {
#ifdef __CUDA_ARCH__
      return __cosf(a); 
#else
      return cosf(a); 
#endif
    }

    __device__ __host__ static void SinCos(const float &a, float *s, float *c)
    {
#ifdef __CUDA_ARCH__
       __sincosf(a, s, c);
#else
       sincosf(a, s, c);
#endif
    }
  };

  /**
     Specialization of Trig functions using fixed b/c gauge reconstructs are -1 -> 1 instead of -Pi -> Pi
   */
  template <>
    struct Trig<true,float> {
    __device__ __host__ static float Atan2( const float &a, const float &b) { return atan2f(a,b)/M_PI; }
    __device__ __host__ static float Sin(const float &a)
    {
#ifdef __CUDA_ARCH__
      return __sinf(a * static_cast<float>(M_PI));
#else
      return sinf(a * static_cast<float>(M_PI));
#endif
    }
    __device__ __host__ static float Cos(const float &a)
    {
#ifdef __CUDA_ARCH__
      return __cosf(a * static_cast<float>(M_PI));
#else
      return cosf(a * static_cast<float>(M_PI));
#endif
    }

    __device__ __host__ static void SinCos(const float &a, float *s, float *c)
    {
#ifdef __CUDA_ARCH__
      __sincosf(a * static_cast<float>(M_PI), s, c);
#else
      sincosf(a * static_cast<float>(M_PI), s, c);
#endif
    }
  };

  
  template <typename Float, int number> struct VectorType;

  // double precision
  template <> struct VectorType<double, 1>{typedef double type; };
  template <> struct VectorType<double, 2>{typedef double2 type; };
  template <> struct VectorType<double, 4>{typedef double4 type; };
  template <> struct VectorType<double, 8>{typedef double8 type; };

  // single precision
  template <> struct VectorType<float, 1>{typedef float type; };
  template <> struct VectorType<float, 2>{typedef float2 type; };
  template <> struct VectorType<float, 4>{typedef float4 type; };
  template <> struct VectorType<float, 8>{typedef float8 type; };

  // half precision
  template <> struct VectorType<short, 1>{typedef short type; };
  template <> struct VectorType<short, 2>{typedef short2 type; };
  template <> struct VectorType<short, 4>{typedef short4 type; };
  template <> struct VectorType<short, 8>{typedef short8 type; };

  // quarter precision
  template <> struct VectorType<char, 1>{typedef char type; };
  template <> struct VectorType<char, 2>{typedef char2 type; };
  template <> struct VectorType<char, 4>{typedef char4 type; };
  template <> struct VectorType<char, 8>{typedef char8 type; };

  // This trait returns the matching texture type (needed for double precision)
  template <typename Float, int number> struct TexVectorType;

  // double precision
  template <> struct TexVectorType<double, 1>{typedef int2 type; };
  template <> struct TexVectorType<double, 2>{typedef int4 type; };

  // single precision
  template <> struct TexVectorType<float, 1>{typedef float type; };
  template <> struct TexVectorType<float, 2>{typedef float2 type; };
  template <> struct TexVectorType<float, 4>{typedef float4 type; };

  // half precision
  template <> struct TexVectorType<short, 1>{typedef short type; };
  template <> struct TexVectorType<short, 2>{typedef short2 type; };
  template <> struct TexVectorType<short, 4>{typedef short4 type; };

  // quarter precision
  template <> struct TexVectorType<char, 1>{typedef char type; };
  template <> struct TexVectorType<char, 2>{typedef char2 type; };
  template <> struct TexVectorType<char, 4>{typedef char4 type; };

  template <typename VectorType, typename Int> constexpr VectorType vector_loader(void *ptr, Int idx) {
#if defined(__CUDA_ARCH__)
      return __ldg(reinterpret_cast< VectorType* >(static_cast<char*>(ptr) + static_cast<Int>(sizeof(VectorType)) * idx));
#else
      return reinterpret_cast< VectorType* >(ptr)[idx];
#endif
  }

  template <typename VectorType, typename Int> struct vector_io {
    __device__ __host__ inline VectorType load(void *ptr, Int idx) { return vector_loader<VectorType>(ptr, idx); }
    __device__ __host__ inline void store(void *ptr, Int idx, const VectorType &value) {
      *reinterpret_cast< VectorType* >(static_cast<char*>(ptr) + static_cast<Int>(sizeof(VectorType)) * idx) = value;
    }
  };

  template <typename Int> struct vector_io<double2, Int> {
    __device__ __host__ inline double2 load(void *ptr, Int idx) { return vector_loader<double2>(ptr, idx); }
    __device__ __host__ inline void store(void *ptr, Int idx, const double2 &value) {
#if defined(__CUDA_ARCH__)
      store_streaming_double2(reinterpret_cast<double2*>(static_cast<char*>(ptr) + static_cast<Int>(sizeof(double2)) * idx), value.x, value.y);
#else
      reinterpret_cast<double2*>(ptr)[idx] = value;
#endif
    }
  };

  template <typename Int> struct vector_io<float4, Int> {
    __device__ __host__ inline float4 load(void *ptr, Int idx) { return vector_loader<float4>(ptr, idx); }
    __device__ __host__ inline void store(void *ptr, Int idx, const float4 &value) {
#if defined(__CUDA_ARCH__)
      store_streaming_float4(reinterpret_cast<float4*>(static_cast<char*>(ptr) + static_cast<Int>(sizeof(float4)) * idx), value.x, value.y, value.z, value.w);
#else
      reinterpret_cast<float4*>(ptr)[idx] = value;
#endif
    }
  };

  template <typename Int> struct vector_io<float2, Int> {
    __device__ __host__ inline float2 load(void *ptr, Int idx) { return vector_loader<float2>(ptr, idx); }
    __device__ __host__ inline void store(void *ptr, Int idx, const float2 &value) {
#if defined(__CUDA_ARCH__)
      store_streaming_float2(reinterpret_cast<float2*>(static_cast<char*>(ptr) + static_cast<Int>(sizeof(float2)) * idx), value.x, value.y);
#else
      reinterpret_cast<float2*>(ptr)[idx] = value;
#endif
    }
  };

  template <typename Int> struct vector_io<short4, Int> {
    __device__ __host__ inline short4 load(void *ptr, Int idx) { return vector_loader<short4>(ptr, idx); }
    __device__ __host__ inline void store(void *ptr, Int idx, const short4 &value) {
#if defined(__CUDA_ARCH__)
      store_streaming_short4(reinterpret_cast<short4*>(static_cast<char*>(ptr) + static_cast<Int>(sizeof(short4)) * idx), value.x, value.y, value.z, value.w);
#else
      reinterpret_cast<short4*>(ptr)[idx] = value;
#endif
    }
  };

  template <typename Int> struct vector_io<short2, Int> {
    __device__ __host__ inline short2 load(void *ptr, Int idx) { return vector_loader<short2>(ptr, idx); }
    __device__ __host__ inline void store(void *ptr, Int idx, const short2 &value) {
#if defined(__CUDA_ARCH__)
      store_streaming_short2(reinterpret_cast<short2*>(static_cast<char*>(ptr) + static_cast<Int>(sizeof(short2)) * idx), value.x, value.y);
#else
      reinterpret_cast<short2*>(ptr)[idx] = value;
#endif
    }
  };

  template <typename Int> struct vector_io<short8, Int> {
    __device__ __host__ inline short8 load(void *ptr, Int idx) {
      float4 tmp = vector_io<float4, Int>().load(ptr, idx);
      short8 recast;
      memcpy(&recast, &tmp, sizeof(float4));
      return recast;
    }
    __device__ __host__ inline void store(void *ptr, Int idx, const short8 &value) {
      vector_io<float4, Int>().store(ptr, idx, *reinterpret_cast<const float4*>(&value));
    }
  };

  template <typename Int> struct vector_io<char2, Int> {
    __device__ __host__ inline char2 load(void *ptr, Int idx) { return vector_loader<char2>(ptr, idx); }
    __device__ __host__ inline void store(void *ptr, Int idx, const char2 &value) {
      vector_io<short, Int>().store(ptr, idx, *reinterpret_cast<const short*>(&value));
    }
  };

  template <typename Int> struct vector_io<char4, Int> {
    __device__ __host__ inline char4 load(void *ptr, Int idx) { return vector_loader<char4>(ptr, idx); }
    __device__ __host__ inline void store(void *ptr, Int idx, const char4 &value) {
      vector_io<short2, Int>().store(ptr, idx, *reinterpret_cast<const short2*>(&value));
    }
  };

  template <typename Int> struct vector_io<char8, Int> {
    __device__ __host__ inline char8 load(void *ptr, Int idx) {
      float2 tmp = vector_io<float2, Int>().load(ptr, idx);
      char8 recast;
      memcpy(&recast, &tmp, sizeof(float2));
      return recast;
    }
    __device__ __host__ inline void store(void *ptr, Int idx, const char8 &value) {
      vector_io<float2, Int>().store(ptr, idx, *reinterpret_cast<const float2*>(&value));
    }
  };

  template <typename VectorType, typename Int>
  __device__ __host__ inline VectorType vector_load(void *ptr, Int idx) {
    return vector_io<VectorType, Int>().load(ptr, idx);
  }

  template <typename VectorType, typename Int>
  __device__ __host__ inline void vector_store(void *ptr, Int idx, const VectorType &value) {
    vector_io<VectorType, Int>().store(ptr, idx, value);
  }

  template<bool large_alloc> struct AllocType { };
  template<> struct AllocType<true> { typedef size_t type; };
  template<> struct AllocType<false> { typedef unsigned int type; };

} // namespace quda

#endif // _REGISTER_TRAITS_H
