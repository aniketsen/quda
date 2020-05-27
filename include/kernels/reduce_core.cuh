#pragma once

#include <color_spinor_field_order.h>

#include <blas_helper.cuh>
#include <cub_helper.cuh>

namespace quda
{

  namespace blas
  {

#define BLAS_SPINOR // do not include ghost functions in Spinor class to reduce parameter space overhead
#include <texture.h>

    template <typename ReduceType, typename SpinorX, typename SpinorY, typename SpinorZ, typename SpinorW,
        typename SpinorV, typename Reducer>
    struct ReductionArg : public ReduceArg<ReduceType> {
      SpinorX X;
      SpinorY Y;
      SpinorZ Z;
      SpinorW W;
      SpinorV V;
      Reducer r;
      const int length;
      ReductionArg(SpinorX X, SpinorY Y, SpinorZ Z, SpinorW W, SpinorV V, Reducer r, int length) :
          X(X),
          Y(Y),
          Z(Z),
          W(W),
          V(V),
          r(r),
          length(length)
      {; }
    };

    /**
       Generic reduction kernel with up to four loads and three saves.
    */
    template <int block_size, typename ReduceType, typename FloatN, int M, typename Arg>
    __global__ void reduceKernel(Arg arg)
    {
      unsigned int i = blockIdx.x * blockDim.x + threadIdx.x;
      unsigned int parity = blockIdx.y;
      unsigned int gridSize = gridDim.x * blockDim.x;

      ReduceType sum;
      ::quda::zero(sum);

      while (i < arg.length) {
        FloatN x[M], y[M], z[M], w[M], v[M];
        arg.X.load(x, i, parity);
        arg.Y.load(y, i, parity);
        arg.Z.load(z, i, parity);
        arg.W.load(w, i, parity);
        arg.V.load(v, i, parity);

        arg.r.pre();

#pragma unroll
        for (int j = 0; j < M; j++) arg.r(sum, x[j], y[j], z[j], w[j], v[j]);

        arg.r.post(sum);

        arg.X.save(x, i, parity);
        arg.Y.save(y, i, parity);
        arg.Z.save(z, i, parity);
        arg.W.save(w, i, parity);
        arg.V.save(v, i, parity);

        i += gridSize;
      }

      ::quda::reduce<block_size, ReduceType>(arg, sum, parity);
    }

    /**
       Base class from which all reduction functors should derive.
    */
    template <typename ReduceType, typename Float2, typename FloatN> struct ReduceFunctor {

      //! pre-computation routine called before the "M-loop"
      virtual __device__ __host__ void pre() { ; }

      //! where the reduction is usually computed and any auxiliary operations
      virtual __device__ __host__ __host__ void operator()(
          ReduceType &sum, FloatN &x, FloatN &y, FloatN &z, FloatN &w, FloatN &v)
          = 0;

      //! post-computation routine called after the "M-loop"
      virtual __device__ __host__ void post(ReduceType &sum) { ; }
    };

    /**
       Return the L1 norm of x
    */
    template <typename ReduceType, typename T> __device__ __host__ ReduceType norm1_(const typename VectorType<T, 2>::type &a)
    {
      return (ReduceType)sqrt(a.x * a.x + a.y * a.y);
    }

    template <typename ReduceType, typename T> __device__ __host__ ReduceType norm1_(const typename VectorType<T, 4>::type &a)
    {
      return (ReduceType)sqrt(a.x * a.x + a.y * a.y) + (ReduceType)sqrt(a.z * a.z + a.w * a.w);
    }

    template <typename ReduceType, typename T> __device__ __host__ ReduceType norm1_(const typename VectorType<T, 8>::type &a)
    {
      return norm1_<ReduceType>(a.x) + norm1_<ReduceType>(a.y);
    }

    template <typename ReduceType, typename Float2, typename FloatN>
    struct Norm1 : public ReduceFunctor<ReduceType, Float2, FloatN> {
      using real = typename scalar<Float2>::type;
      Norm1(const Float2 &a, const Float2 &b) { ; }
      __device__ __host__ void operator()(ReduceType &sum, FloatN &x, FloatN &y, FloatN &z, FloatN &w, FloatN &v)
      {
        sum += norm1_<ReduceType, real>(x);
      }
      static int streams() { return 1; } //! total number of input and output streams
      static int flops() { return 2; }   //! flops per element
    };

    /**
       Return the L2 norm of x
    */
    template <typename ReduceType, typename T> __device__ __host__ void norm2_(ReduceType &sum, const typename VectorType<T, 2>::type &a)
    {
      sum += (ReduceType)a.x * (ReduceType)a.x;
      sum += (ReduceType)a.y * (ReduceType)a.y;
    }

    template <typename ReduceType, typename T> __device__ __host__ void norm2_(ReduceType &sum, const typename VectorType<T, 4>::type &a)
    {
      sum += (ReduceType)a.x * (ReduceType)a.x;
      sum += (ReduceType)a.y * (ReduceType)a.y;
      sum += (ReduceType)a.z * (ReduceType)a.z;
      sum += (ReduceType)a.w * (ReduceType)a.w;
    }

    template <typename ReduceType, typename T> __device__ __host__ void norm2_(ReduceType &sum, const typename VectorType<T, 8>::type &a)
    {
      norm2_(sum, a.x);
      norm2_(sum, a.y);
    }

    template <typename ReduceType, typename Float2, typename FloatN>
    struct Norm2 : public ReduceFunctor<ReduceType, Float2, FloatN> {
      Norm2(const Float2 &a, const Float2 &b) { ; }
      using real = typename scalar<Float2>::type;
      __device__ __host__ void operator()(ReduceType &sum, FloatN &x, FloatN &y, FloatN &z, FloatN &w, FloatN &v)
      {
        norm2_<ReduceType,real>(sum, x);
      }
      static int streams() { return 1; } //! total number of input and output streams
      static int flops() { return 2; }   //! flops per element
    };

    /**
       Return the real dot product of x and y
    */
    template <typename ReduceType, typename T> __device__ __host__ void dot_(ReduceType &sum, const typename VectorType<T, 2>::type &a,
                                                                             const typename VectorType<T, 2>::type &b)
    {
      sum += (ReduceType)a.x * (ReduceType)b.x;
      sum += (ReduceType)a.y * (ReduceType)b.y;
    }

    template <typename ReduceType, typename T> __device__ __host__ void dot_(ReduceType &sum, const typename VectorType<T, 4>::type &a,
                                                                             const typename VectorType<T, 4>::type &b)
    {
      sum += (ReduceType)a.x * (ReduceType)b.x;
      sum += (ReduceType)a.y * (ReduceType)b.y;
      sum += (ReduceType)a.z * (ReduceType)b.z;
      sum += (ReduceType)a.w * (ReduceType)b.w;
    }

    template <typename ReduceType, typename T> __device__ __host__ void dot_(ReduceType &sum, const typename VectorType<T, 8>::type &a,
                                                                             const typename VectorType<T, 8>::type &b)
    {
      dot_(sum, a.x, b.x);
      dot_(sum, a.y, b.y);
    }

    template <typename ReduceType, typename Float2, typename FloatN>
    struct Dot : public ReduceFunctor<ReduceType, Float2, FloatN> {
      using real = typename scalar<Float2>::type;
      Dot(const Float2 &a, const Float2 &b) { ; }
      __device__ __host__ void operator()(ReduceType &sum, FloatN &x, FloatN &y, FloatN &z, FloatN &w, FloatN &v)
      {
        dot_<ReduceType, real>(sum, x, y);
      }
      static int streams() { return 2; } //! total number of input and output streams
      static int flops() { return 2; }   //! flops per element
    };

    /**
       First performs the operation z[i] = a*x[i] + b*y[i]
       Return the norm of y
    */
    template <typename ReduceType, typename Float2, typename FloatN>
    struct axpbyzNorm2 : public ReduceFunctor<ReduceType, Float2, FloatN> {
      using real = typename scalar<Float2>::type;
      const real a;
      const real b;
      axpbyzNorm2(const Float2 &a, const Float2 &b) : a(a.x), b(b.x) { ; }
      __device__ __host__ void operator()(ReduceType &sum, FloatN &x, FloatN &y, FloatN &z, FloatN &w, FloatN &v)
      {
        z = a * x + b * y;
        norm2_<ReduceType, real>(sum, z);
      }
      static int streams() { return 3; } //! total number of input and output streams
      static int flops() { return 4; }   //! flops per element
    };

    /**
       First performs the operation y[i] += a*x[i]
       Return real dot product (x,y)
    */
    template <typename ReduceType, typename Float2, typename FloatN>
    struct AxpyReDot : public ReduceFunctor<ReduceType, Float2, FloatN> {
      using real = typename scalar<Float2>::type;
      const real a;
      AxpyReDot(const Float2 &a, const Float2 &b) : a(a.x) { ; }
      __device__ __host__ void operator()(ReduceType &sum, FloatN &x, FloatN &y, FloatN &z, FloatN &w, FloatN &v)
      {
        y += a * x;
        dot_<ReduceType, real>(sum, x, y);
      }
      static int streams() { return 3; } //! total number of input and output streams
      static int flops() { return 4; }   //! flops per element
    };

    /**
       Functor to perform the operation y += a * x  (complex-valued)
    */
    template <typename T>
    __device__ __host__ void caxpy_(const complex<T> &a, const typename VectorType<T, 2>::type &x, typename VectorType<T, 2>::type &y)
    {
      y.x += a.x * x.x;
      y.x -= a.y * x.y;
      y.y += a.y * x.x;
      y.y += a.x * x.y;
    }

    template <typename T>
    __device__ __host__ void caxpy_(const complex<T> &a, const typename VectorType<T, 4>::type &x, typename VectorType<T, 4>::type &y)
    {
      y.x += a.x * x.x;
      y.x -= a.y * x.y;
      y.y += a.y * x.x;
      y.y += a.x * x.y;
      y.z += a.x * x.z;
      y.z -= a.y * x.w;
      y.w += a.y * x.z;
      y.w += a.x * x.w;
    }

    template <typename T>
    __device__ __host__ void caxpy_(const complex<T> &a, const typename VectorType<T, 8>::type &x, typename VectorType<T, 8>::type &y)
    {
      caxpy_(a, x.x, y.x);
      caxpy_(a, x.y, y.y);
    }

    /**
       First performs the operation y[i] = a*x[i] + y[i] (complex-valued)
       Second returns the norm of y
    */
    template <typename ReduceType, typename Float2, typename FloatN>
    struct caxpyNorm2 : public ReduceFunctor<ReduceType, Float2, FloatN> {
      using real = typename scalar<Float2>::type;
      const complex<real> a;
      caxpyNorm2(const Float2 &a, const Float2 &b) : a(a) { ; }
      __device__ __host__ void operator()(ReduceType &sum, FloatN &x, FloatN &y, FloatN &z, FloatN &w, FloatN &v)
      {
        caxpy_(a, x, y);
        norm2_<ReduceType, real>(sum, y);
      }
      static int streams() { return 3; } //! total number of input and output streams
      static int flops() { return 6; }   //! flops per element
    };

    /**
       double caxpyXmayNormCuda(float a, float *x, float *y, n){}
       First performs the operation y[i] = a*x[i] + y[i]
       Second performs the operator x[i] -= a*z[i]
       Third returns the norm of x
    */
    template <typename ReduceType, typename Float2, typename FloatN>
    struct caxpyxmaznormx : public ReduceFunctor<ReduceType, Float2, FloatN> {
      using real = typename scalar<Float2>::type;
      const complex<real> a;
      caxpyxmaznormx(const Float2 &a, const Float2 &b) : a(a) { ; }
      __device__ __host__ void operator()(ReduceType &sum, FloatN &x, FloatN &y, FloatN &z, FloatN &w, FloatN &v)
      {
        caxpy_(a, x, y);
        caxpy_(-a, z, x);
        norm2_<ReduceType, real>(sum, x);
      }
      static int streams() { return 5; } //! total number of input and output streams
      static int flops() { return 10; }  //! flops per element
    };

    /**
       double cabxpyzAxNorm(float a, complex b, float *x, float *y, float *z){}
       First performs the operation z[i] = y[i] + a*b*x[i]
       Second performs x[i] *= a
       Third returns the norm of x
    */
    template <typename ReduceType, typename Float2, typename FloatN>
    struct cabxpyzaxnorm : public ReduceFunctor<ReduceType, Float2, FloatN> {
      using real = typename scalar<Float2>::type;
      const real a;
      const complex<real> b;
      cabxpyzaxnorm(const Float2 &a, const Float2 &b) : a(a.x), b(b) { ; }
      __device__ __host__ void operator()(ReduceType &sum, FloatN &x, FloatN &y, FloatN &z, FloatN &w, FloatN &v)
      {
        x *= a;
        caxpy_(b, x, y);
        z = y;
        norm2_<ReduceType, real>(sum, z);
      }
      static int streams() { return 4; } //! total number of input and output streams
      static int flops() { return 10; }  //! flops per element
    };

    /**
       Returns complex-valued dot product of x and y
    */
    template <typename ReduceType, typename T>
    __device__ __host__ void cdot_(ReduceType &sum, const typename VectorType<T, 2>::type &a, const typename VectorType<T, 2>::type &b)
    {
      typedef typename scalar<ReduceType>::type scalar;
      sum.x += (scalar)a.x * (scalar)b.x;
      sum.x += (scalar)a.y * (scalar)b.y;
      sum.y += (scalar)a.x * (scalar)b.y;
      sum.y -= (scalar)a.y * (scalar)b.x;
    }

    template <typename ReduceType, typename T>
    __device__ __host__ void cdot_(ReduceType &sum, const typename VectorType<T, 4>::type &a, const typename VectorType<T, 4>::type &b)
    {
      typedef typename scalar<ReduceType>::type scalar;
      sum.x += (scalar)a.x * (scalar)b.x;
      sum.x += (scalar)a.y * (scalar)b.y;
      sum.x += (scalar)a.z * (scalar)b.z;
      sum.x += (scalar)a.w * (scalar)b.w;
      sum.y += (scalar)a.x * (scalar)b.y;
      sum.y -= (scalar)a.y * (scalar)b.x;
      sum.y += (scalar)a.z * (scalar)b.w;
      sum.y -= (scalar)a.w * (scalar)b.z;
    }

    template <typename ReduceType, typename T>
    __device__ __host__ void cdot_(ReduceType &sum, const typename VectorType<T, 8>::type &a, const typename VectorType<T, 8>::type &b)
    {
      cdot_(sum, a.x, b.x);
      cdot_(sum, a.y, b.y);
    }

    template <typename ReduceType, typename Float2, typename FloatN>
    struct Cdot : public ReduceFunctor<ReduceType, Float2, FloatN> {
      using real = typename scalar<Float2>::type;
      Cdot(const Float2 &a, const Float2 &b) { ; }
      __device__ __host__ void operator()(ReduceType &sum, FloatN &x, FloatN &y, FloatN &z, FloatN &w, FloatN &v)
      {
        cdot_<ReduceType, real>(sum, x, y);
      }
      static int streams() { return 2; } //! total number of input and output streams
      static int flops() { return 4; }   //! flops per element
    };

    /**
       double caxpyDotzyCuda(float a, float *x, float *y, float *z, n){}
       First performs the operation y[i] = a*x[i] + y[i]
       Second returns the dot product (z,y)
    */
    template <typename ReduceType, typename Float2, typename FloatN>
    struct caxpydotzy : public ReduceFunctor<ReduceType, Float2, FloatN> {
      using real = typename scalar<Float2>::type;
      const complex<real> a;
      caxpydotzy(const Float2 &a, const Float2 &b) : a(a) { ; }
      __device__ __host__ void operator()(ReduceType &sum, FloatN &x, FloatN &y, FloatN &z, FloatN &w, FloatN &v)
      {
        caxpy_(a, x, y);
        cdot_<ReduceType, real>(sum, z, y);
      }
      static int streams() { return 4; } //! total number of input and output streams
      static int flops() { return 8; }   //! flops per element
    };

    /**
       First returns the dot product (x,y)
       Returns the norm of x
    */
    template <typename ReduceType, typename InputType>
    __device__ __host__ void cdotNormA_(ReduceType &sum, const InputType &a, const InputType &b)
    {
      using real = typename scalar<InputType>::type;
      using scalar = typename scalar<ReduceType>::type;
      cdot_<ReduceType, real>(sum, a, b);
      norm2_<scalar, real>(sum.z, a);
    }

    /**
       First returns the dot product (x,y)
       Returns the norm of y
    */
    template <typename ReduceType, typename InputType>
    __device__ __host__ void cdotNormB_(ReduceType &sum, const InputType &a, const InputType &b)
    {
      using real = typename scalar<InputType>::type;
      using scalar = typename scalar<ReduceType>::type;
      cdot_<ReduceType, real>(sum, a, b);
      norm2_<scalar, real>(sum.z, b);
    }

    template <typename ReduceType, typename Float2, typename FloatN>
    struct CdotNormA : public ReduceFunctor<ReduceType, Float2, FloatN> {
      CdotNormA(const Float2 &a, const Float2 &b) { ; }
      __device__ __host__ void operator()(ReduceType &sum, FloatN &x, FloatN &y, FloatN &z, FloatN &w, FloatN &v)
      {
        cdotNormA_<ReduceType>(sum, x, y);
      }
      static int streams() { return 2; } //! total number of input and output streams
      static int flops() { return 6; }   //! flops per element
    };

    /**
       This convoluted kernel does the following:
       z += a*x + b*y, y -= b*w, norm = (y,y), dot = (u, y)
    */
    template <typename ReduceType, typename Float2, typename FloatN>
    struct caxpbypzYmbwcDotProductUYNormY_ : public ReduceFunctor<ReduceType, Float2, FloatN> {
      using real = typename scalar<Float2>::type;
      const complex<real> a;
      const complex<real> b;
      caxpbypzYmbwcDotProductUYNormY_(const Float2 &a, const Float2 &b) : a(a), b(b) { ; }
      __device__ __host__ void operator()(ReduceType &sum, FloatN &x, FloatN &y, FloatN &z, FloatN &w, FloatN &v)
      {
        caxpy_(a, x, z);
        caxpy_(b, y, z);
        caxpy_(-b, w, y);
        cdotNormB_<ReduceType>(sum, v, y);
      }
      static int streams() { return 7; } //! total number of input and output streams
      static int flops() { return 18; }  //! flops per element
    };

    /**
       Specialized kernel for the modified CG norm computation for
       computing beta.  Computes y = y + a*x and returns norm(y) and
       dot(y, delta(y)) where delta(y) is the difference between the
       input and out y vector.
    */
    template <typename ReduceType, typename Float2, typename FloatN>
    struct axpyCGNorm2 : public ReduceFunctor<ReduceType, Float2, FloatN> {
      using real = typename scalar<Float2>::type;
      using scalar = typename scalar<ReduceType>::type;
      const real a;
      axpyCGNorm2(const Float2 &a, const Float2 &b) : a(a.x) { ; }
      __device__ __host__ void operator()(ReduceType &sum, FloatN &x, FloatN &y, FloatN &z, FloatN &w, FloatN &v)
      {
        FloatN z_new = z + a * x;
        norm2_<scalar, real>(sum.x, z_new);
        dot_<scalar, real>(sum.y, z_new, z_new - z);
        z = z_new;
      }
      static int streams() { return 3; } //! total number of input and output streams
      static int flops() { return 6; }   //! flops per real element
    };

    /**
       This kernel returns (x, x) and (r,r) and also returns the so-called
       heavy quark norm as used by MILC: 1 / N * \sum_i (r, r)_i / (x, x)_i, where
       i is site index and N is the number of sites.
       When this kernel is launched, we must enforce that the parameter M
       in the launcher corresponds to the number of FloatN fields used to
       represent the spinor, e.g., M=6 for Wilson and M=3 for staggered.
       This is only the case for half-precision kernels by default.  To
       enable this, the siteUnroll template parameter must be set true
       when reduceCuda is instantiated.
    */
    template <typename ReduceType, typename Float2, typename FloatN>
    struct HeavyQuarkResidualNorm_ : public ReduceFunctor<ReduceType, Float2, FloatN> {
      using real = typename scalar<Float2>::type;
      using scalar = typename scalar<ReduceType>::type;
      ReduceType aux;
      HeavyQuarkResidualNorm_(const Float2 &a, const Float2 &b) : aux {} { ; }

      __device__ __host__ void pre()
      {
        aux.x = 0;
        aux.y = 0;
      }

      __device__ __host__ void operator()(ReduceType &sum, FloatN &x, FloatN &y, FloatN &z, FloatN &w, FloatN &v)
      {
        norm2_<scalar, real>(aux.x, x);
        norm2_<scalar, real>(aux.y, y);
      }

      //! sum the solution and residual norms, and compute the heavy-quark norm
      __device__ __host__ void post(ReduceType &sum)
      {
        sum.x += aux.x;
        sum.y += aux.y;
        sum.z += (aux.x > 0.0) ? (aux.y / aux.x) : static_cast<real>(1.0);
      }

      static int streams() { return 2; } //! total number of input and output streams
      static int flops() { return 4; }   //! undercounts since it excludes the per-site division
    };

    /**
      Variant of the HeavyQuarkResidualNorm kernel: this takes three
      arguments, the first two are summed together to form the
      solution, with the third being the residual vector.  This removes
      the need an additional xpy call in the solvers, impriving
      performance.
    */
    template <typename ReduceType, typename Float2, typename FloatN>
    struct xpyHeavyQuarkResidualNorm_ : public ReduceFunctor<ReduceType, Float2, FloatN> {
      using real = typename scalar<Float2>::type;
      using scalar = typename scalar<ReduceType>::type;
      ReduceType aux;
      xpyHeavyQuarkResidualNorm_(const Float2 &a, const Float2 &b) : aux {} { ; }

      __device__ __host__ void pre()
      {
        aux.x = 0;
        aux.y = 0;
      }

      __device__ __host__ void operator()(ReduceType &sum, FloatN &x, FloatN &y, FloatN &z, FloatN &w, FloatN &v)
      {
        norm2_<scalar, real>(aux.x, x + y);
        norm2_<scalar, real>(aux.y, z);
      }

      //! sum the solution and residual norms, and compute the heavy-quark norm
      __device__ __host__ void post(ReduceType &sum)
      {
        sum.x += aux.x;
        sum.y += aux.y;
        sum.z += (aux.x > 0.0) ? (aux.y / aux.x) : static_cast<real>(1.0);
      }

      static int streams() { return 3; } //! total number of input and output streams
      static int flops() { return 5; }
    };

    /**
       double3 tripleCGReduction(V x, V y, V z){}
       First performs the operation norm2(x)
       Second performs the operatio norm2(y)
       Third performs the operation dotPropduct(y,z)
    */
    template <typename ReduceType, typename Float2, typename FloatN>
    struct tripleCGReduction_ : public ReduceFunctor<ReduceType, Float2, FloatN> {
      using real = typename scalar<Float2>::type;
      using scalar = typename scalar<ReduceType>::type;
      tripleCGReduction_(const Float2 &a, const Float2 &b) { ; }
      __device__ __host__ void operator()(ReduceType &sum, FloatN &x, FloatN &y, FloatN &z, FloatN &w, FloatN &v)
      {
        norm2_<scalar, real>(sum.x, x);
        norm2_<scalar, real>(sum.y, y);
        dot_<scalar, real>(sum.z, y, z);
      }
      static int streams() { return 3; } //! total number of input and output streams
      static int flops() { return 6; }   //! flops per element
    };

    /**
       double4 quadrupleCGReduction(V x, V y, V z){}
       First performs the operation norm2(x)
       Second performs the operatio norm2(y)
       Third performs the operation dotPropduct(y,z)
       Fourth performs the operation norm(z)
    */
    template <typename ReduceType, typename Float2, typename FloatN>
    struct quadrupleCGReduction_ : public ReduceFunctor<ReduceType, Float2, FloatN> {
      using real = typename scalar<Float2>::type;
      using scalar = typename scalar<ReduceType>::type;
      quadrupleCGReduction_(const Float2 &a, const Float2 &b) { ; }
      __device__ __host__ void operator()(ReduceType &sum, FloatN &x, FloatN &y, FloatN &z, FloatN &w, FloatN &v)
      {
        norm2_<scalar, real>(sum.x, x);
        norm2_<scalar, real>(sum.y, y);
        dot_<scalar, real>(sum.z, y, z);
        norm2_<scalar, real>(sum.w, w);
      }
      static int streams() { return 3; } //! total number of input and output streams
      static int flops() { return 8; }   //! flops per element
    };

    /**
       double quadrupleCG3InitNorm(d a, d b, V x, V y, V z, V w, V v){}
        z = x;
        w = y;
        x += a*y;
        y -= a*v;
        norm2(y);
    */
    template <typename ReduceType, typename Float2, typename FloatN>
    struct quadrupleCG3InitNorm_ : public ReduceFunctor<ReduceType, Float2, FloatN> {
      using real = typename scalar<Float2>::type;
      const real a;
      quadrupleCG3InitNorm_(const Float2 &a, const Float2 &b) : a(a.x) { ; }
      __device__ __host__ void operator()(ReduceType &sum, FloatN &x, FloatN &y, FloatN &z, FloatN &w, FloatN &v)
      {
        z = x;
        w = y;
        x += a * y;
        y -= a * v;
        norm2_<ReduceType, real>(sum, y);
      }
      static int streams() { return 6; } //! total number of input and output streams
      static int flops() { return 6; }   //! flops per element check if it's right
    };

    /**
       double quadrupleCG3UpdateNorm(d gamma, d rho, V x, V y, V z, V w, V v){}
        tmpx = x;
        tmpy = y;
        x = b*(x + a*y) + (1-b)*z;
        y = b*(y + a*v) + (1-b)*w;
        z = tmpx;
        w = tmpy;
        norm2(y);
    */
    template <typename ReduceType, typename Float2, typename FloatN>
    struct quadrupleCG3UpdateNorm_ : public ReduceFunctor<ReduceType, Float2, FloatN> {
      using real = typename scalar<Float2>::type;
      const real a;
      const real b;
      quadrupleCG3UpdateNorm_(const Float2 &a, const Float2 &b) : a(a.x), b(b.x) { ; }
      __device__ __host__ void operator()(ReduceType &sum, FloatN &x, FloatN &y, FloatN &z, FloatN &w, FloatN &v)
      {
        auto tmpx = x;
        auto tmpy = y;
        x = b * (x + a * y) + b * z;
        y = b * (y - a * v) + b * w;
        z = tmpx;
        w = tmpy;
        norm2_<ReduceType, real>(sum, y);
      }
      static int streams() { return 7; } //! total number of input and output streams
      static int flops() { return 16; }  //! flops per element check if it's right
    };

    /**
       void doubleCG3InitNorm(d a, V x, V y, V z){}
        y = x;
        x -= a*z;
        norm2(x);
    */
    template <typename ReduceType, typename Float2, typename FloatN>
    struct doubleCG3InitNorm_ : public ReduceFunctor<ReduceType, Float2, FloatN> {
      using real = typename scalar<Float2>::type;
      const real a;
      doubleCG3InitNorm_(const Float2 &a, const Float2 &b) : a(a.x) { ; }
      __device__ __host__ void operator()(ReduceType &sum, FloatN &x, FloatN &y, FloatN &z, FloatN &w, FloatN &v)
      {
        y = x;
        x -= a * z;
        norm2_<ReduceType, real>(sum, x);
      }
      static int streams() { return 3; } //! total number of input and output streams
      static int flops() { return 5; }   //! flops per element
    };

    /**
       void doubleCG3UpdateNorm(d a, d b, V x, V y, V z){}
        tmp = x;
        x = b*(x-a*z) + (1-b)*y;
        y = tmp;
        norm2(x);
    */
    template <typename ReduceType, typename Float2, typename FloatN>
    struct doubleCG3UpdateNorm_ : public ReduceFunctor<ReduceType, Float2, FloatN> {
      using real = typename scalar<Float2>::type;
      const real a;
      const real b;
      doubleCG3UpdateNorm_(const Float2 &a, const Float2 &b) : a(a.x), b(b.x) { ; }
      __device__ __host__ void operator()(ReduceType &sum, FloatN &x, FloatN &y, FloatN &z, FloatN &w, FloatN &v)
      {
        auto tmp = x;
        x = b * (x - a * z) + b * y;
        y = tmp;
        norm2_<ReduceType, real>(sum, x);
      }
      static int streams() { return 4; } //! total number of input and output streams
      static int flops() { return 9; }   //! flops per element
    };

  } // namespace blas

} // namespace quda
