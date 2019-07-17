#include <iostream>
#include <dirac_quda.h>
#include <blas_quda.h>
#include <multigrid.h>

#define NEW_DSLASH

namespace quda {


  DiracClover::DiracClover(const DiracParam &param) : DiracWilson(param), clover(*(param.clover)) {}

  DiracClover::DiracClover(const DiracClover &dirac) : DiracWilson(dirac), clover(dirac.clover) {}

  DiracClover::~DiracClover() { }

  DiracClover& DiracClover::operator=(const DiracClover &dirac)
  {
    if (&dirac != this) {
      DiracWilson::operator=(dirac);
      clover = dirac.clover;
    }
    return *this;
  }

  void DiracClover::checkParitySpinor(const ColorSpinorField &out, const ColorSpinorField &in) const
  {
    Dirac::checkParitySpinor(out, in);

    if (out.Volume() != clover.VolumeCB()) {
      errorQuda("Parity spinor volume %d doesn't match clover checkboard volume %d",
		out.Volume(), clover.VolumeCB());
    }
  }

  /** Applies the operator (A + k D) */
  void DiracClover::DslashXpay(ColorSpinorField &out, const ColorSpinorField &in, 
			       const QudaParity parity, const ColorSpinorField &x,
			       const double &k) const
  {
    checkParitySpinor(in, out);
    checkSpinorAlias(in, out);

    ApplyWilsonClover(out, in, *gauge, clover, k, x, parity, dagger, commDim, profile);
    flops += 1872ll*in.Volume();
  }

  /** Applies the operator (A + k D) */
  void DiracClover::DslashXpay(ColorSpinorField &out, const ColorSpinorField &in,
  			       const QudaParity parity, const ColorSpinorField &x,
  			       const double &k, const double &b) const
    {
      checkParitySpinor(in, out);
      checkSpinorAlias(in, out);

  #ifndef USE_LEGACY_DSLASH
      ApplyWilsonClover(out, in, *gauge, clover, k, b ,x, parity, dagger, commDim, profile);
  #else
      if (checkLocation(out, in, x) == QUDA_CUDA_FIELD_LOCATION) {
        FullClover cs(clover);
        asymCloverDslashCuda(&static_cast<cudaColorSpinorField&>(out), *gauge, cs,
  			   &static_cast<const cudaColorSpinorField&>(in), parity, dagger,
  			   &static_cast<const cudaColorSpinorField&>(x), k, commDim, profile);
      } else {
        errorQuda("Not implemented");
      }
  #endif

      flops += 1872ll*in.Volume();
    }

  // Public method to apply the clover term only
  void DiracClover::Clover(ColorSpinorField &out, const ColorSpinorField &in, const QudaParity parity) const
  {
    checkParitySpinor(in, out);

    ApplyClover(out, in, clover, false, parity);
    flops += 504ll*in.Volume();
  }

  void DiracClover::M(ColorSpinorField &out, const ColorSpinorField &in) const
  {
    ApplyWilsonClover(out, in, *gauge, clover, -kappa, in, QUDA_INVALID_PARITY, dagger, commDim, profile);
    flops += 1872ll * in.Volume();
  }

  void DiracClover::MdagM(ColorSpinorField &out, const ColorSpinorField &in) const
  {
    checkFullSpinor(out, in);

    bool reset = newTmp(&tmp1, in);
    checkFullSpinor(*tmp1, in);

    M(*tmp1, in);
    Mdag(out, *tmp1);

    deleteTmp(&tmp1, reset);
  }

  void DiracClover::prepare(ColorSpinorField* &src, ColorSpinorField* &sol,
			    ColorSpinorField &x, ColorSpinorField &b, 
			    const QudaSolutionType solType) const
  {
    if (solType == QUDA_MATPC_SOLUTION || solType == QUDA_MATPCDAG_MATPC_SOLUTION) {
      errorQuda("Preconditioned solution requires a preconditioned solve_type");
    }

    src = &b;
    sol = &x;
  }

  void DiracClover::reconstruct(ColorSpinorField &x, const ColorSpinorField &b,
				const QudaSolutionType solType) const
  {
    // do nothing
  }

  void DiracClover::createCoarseOp(GaugeField &Y, GaugeField &X, const Transfer &T,
				   double kappa, double mass, double mu, double mu_factor) const {
    double a = 2.0 * kappa * mu * T.Vectors().TwistFlavor();
    CoarseOp(Y, X, T, *gauge, &clover, kappa, a, mu_factor, QUDA_CLOVER_DIRAC, QUDA_MATPC_INVALID);
  }

  /*******
   * DiracCloverPC Starts here 
   *******/
  DiracCloverPC::DiracCloverPC(const DiracParam &param) : 
    DiracClover(param)
  {
    // For the preconditioned operator, we need to check that the inverse of the clover term is present
    if (!clover.cloverInv) errorQuda("Clover inverse required for DiracCloverPC");
  }

  DiracCloverPC::DiracCloverPC(const DiracCloverPC &dirac) : DiracClover(dirac) { }

  DiracCloverPC::~DiracCloverPC() { }

  DiracCloverPC& DiracCloverPC::operator=(const DiracCloverPC &dirac)
  {
    if (&dirac != this) {
      DiracClover::operator=(dirac);
    }
    return *this;
  }

  // Public method
  void DiracCloverPC::CloverInv(ColorSpinorField &out, const ColorSpinorField &in, 
				const QudaParity parity) const
  {
    checkParitySpinor(in, out);
    ApplyClover(out, in, clover, true, parity);
    flops += 504ll*in.Volume();
  }

  // apply hopping term, then clover: (A_ee^-1 D_eo) or (A_oo^-1 D_oe),
  // and likewise for dagger: (A_ee^-1 D^dagger_eo) or (A_oo^-1 D^dagger_oe)
  // NOTE - this isn't Dslash dagger since order should be reversed!
  void DiracCloverPC::Dslash(ColorSpinorField &out, const ColorSpinorField &in, 
			     const QudaParity parity) const
  {
    checkParitySpinor(in, out);
    checkSpinorAlias(in, out);

    ApplyWilsonCloverPreconditioned(out, in, *gauge, clover, 0.0, in, parity, dagger, commDim, profile);
    flops += 1824ll*in.Volume();
  }

  // xpay version of the above
  void DiracCloverPC::DslashXpay(ColorSpinorField &out, const ColorSpinorField &in, 
				 const QudaParity parity, const ColorSpinorField &x,
				 const double &k) const
  {
    checkParitySpinor(in, out);
    checkSpinorAlias(in, out);

    ApplyWilsonCloverPreconditioned(out, in, *gauge, clover, k, x, parity, dagger, commDim, profile);
    flops += 1872ll*in.Volume();
  }



  // Apply the even-odd preconditioned clover-improved Dirac operator
  void DiracCloverPC::M(ColorSpinorField &out, const ColorSpinorField &in) const
  {
    double kappa2 = -kappa*kappa;
    bool reset1 = newTmp(&tmp1, in);

    bool symmetric =(matpcType == QUDA_MATPC_EVEN_EVEN || matpcType == QUDA_MATPC_ODD_ODD) ? true : false;
    int odd_bit = (matpcType == QUDA_MATPC_ODD_ODD || matpcType == QUDA_MATPC_ODD_ODD_ASYMMETRIC) ? 1 : 0;
    QudaParity parity[2] = {static_cast<QudaParity>((1 + odd_bit) % 2), static_cast<QudaParity>((0 + odd_bit) % 2)};


   
    if (!symmetric) {

      // No need to change order of calls for dagger
      // because the asymmetric operator is actually symmetric
      // A_oo -D_oe A^{-1}_ee D_eo -> A_oo -D^\dag_oe A^{-1}_ee D^\dag_eo
      // the pieces in Dslash and DslashXPay respect the dagger


      // DiracCloverPC::Dslash applies A^{-1}Dslash
      Dslash(*tmp1, in, parity[0]);
      // DiracClover::DslashXpay applies (A - kappa^2 D)
      DiracClover::DslashXpay(out, *tmp1, parity[1], in, kappa2);
    } else if (!dagger) { // symmetric preconditioning
      // We need two cases because M = 1-ADAD and M^\dag = 1-D^\dag A D^dag A
      // where A is actually a clover inverse.

      // This is the non-dag case: AD
      Dslash(*tmp1, in, parity[0]);
      
      // Then x + AD (AD)
      DslashXpay(out, *tmp1, parity[1], in, kappa2);
    } else { // symmetric preconditioning, dagger

      // This is the dagger: 1 - DADA
      //  i) Apply A
      CloverInv(out, in, parity[1]);
      // ii) Apply A D => ADA
      Dslash(*tmp1, out, parity[0]);
      // iii) Apply  x + D(ADA)
      DiracWilson::DslashXpay(out, *tmp1, parity[1], in, kappa2);
    }

    deleteTmp(&tmp1, reset1);
  }

  void DiracCloverPC::MdagM(ColorSpinorField &out, const ColorSpinorField &in) const
  {
    // need extra temporary because of symmetric preconditioning dagger
    // and for multi-gpu the input and output fields cannot alias
    bool reset = newTmp(&tmp2, in);
    M(*tmp2, in);
    Mdag(out, *tmp2);
    deleteTmp(&tmp2, reset);
  }

  void DiracCloverPC::prepare(ColorSpinorField* &src, ColorSpinorField* &sol, 
			      ColorSpinorField &x, ColorSpinorField &b, 
			      const QudaSolutionType solType) const
  {
    // we desire solution to preconditioned system
    if (solType == QUDA_MATPC_SOLUTION || solType == QUDA_MATPCDAG_MATPC_SOLUTION) {
      src = &b;
      sol = &x;
      return;
    }

    bool reset = newTmp(&tmp1, b.Even());
  
    // we desire solution to full system
    if (matpcType == QUDA_MATPC_EVEN_EVEN) {
      // src = A_ee^-1 (b_e + k D_eo A_oo^-1 b_o)
      src = &(x.Odd());
      CloverInv(*src, b.Odd(), QUDA_ODD_PARITY);
      DiracWilson::DslashXpay(*tmp1, *src, QUDA_EVEN_PARITY, b.Even(), kappa);
      CloverInv(*src, *tmp1, QUDA_EVEN_PARITY);
      sol = &(x.Even());
    } else if (matpcType == QUDA_MATPC_ODD_ODD) {
      // src = A_oo^-1 (b_o + k D_oe A_ee^-1 b_e)
      src = &(x.Even());
      CloverInv(*src, b.Even(), QUDA_EVEN_PARITY);
      DiracWilson::DslashXpay(*tmp1, *src, QUDA_ODD_PARITY, b.Odd(), kappa);
      CloverInv(*src, *tmp1, QUDA_ODD_PARITY);
      sol = &(x.Odd());
    } else if (matpcType == QUDA_MATPC_EVEN_EVEN_ASYMMETRIC) {
      // src = b_e + k D_eo A_oo^-1 b_o
      src = &(x.Odd());
      CloverInv(*tmp1, b.Odd(), QUDA_ODD_PARITY); // safe even when *tmp1 = b.odd
      DiracWilson::DslashXpay(*src, *tmp1, QUDA_EVEN_PARITY, b.Even(), kappa);
      sol = &(x.Even());
    } else if (matpcType == QUDA_MATPC_ODD_ODD_ASYMMETRIC) {
      // src = b_o + k D_oe A_ee^-1 b_e
      src = &(x.Even());
      CloverInv(*tmp1, b.Even(), QUDA_EVEN_PARITY); // safe even when *tmp1 = b.even
      DiracWilson::DslashXpay(*src, *tmp1, QUDA_ODD_PARITY, b.Odd(), kappa);
      sol = &(x.Odd());
    } else {
      errorQuda("MatPCType %d not valid for DiracCloverPC", matpcType);
    }

    // here we use final solution to store parity solution and parity source
    // b is now up for grabs if we want

    deleteTmp(&tmp1, reset);

  }

  void DiracCloverPC::reconstruct(ColorSpinorField &x, const ColorSpinorField &b,
				  const QudaSolutionType solType) const
  {
    if (solType == QUDA_MATPC_SOLUTION || solType == QUDA_MATPCDAG_MATPC_SOLUTION) {
      return;
    }

    checkFullSpinor(x, b);

    bool reset = newTmp(&tmp1, b.Even());

    // create full solution

    if (matpcType == QUDA_MATPC_EVEN_EVEN ||
	matpcType == QUDA_MATPC_EVEN_EVEN_ASYMMETRIC) {
      // x_o = A_oo^-1 (b_o + k D_oe x_e)
      DiracWilson::DslashXpay(*tmp1, x.Even(), QUDA_ODD_PARITY, b.Odd(), kappa);
      CloverInv(x.Odd(), *tmp1, QUDA_ODD_PARITY);
    } else if (matpcType == QUDA_MATPC_ODD_ODD ||
	       matpcType == QUDA_MATPC_ODD_ODD_ASYMMETRIC) {
      // x_e = A_ee^-1 (b_e + k D_eo x_o)
      DiracWilson::DslashXpay(*tmp1, x.Odd(), QUDA_EVEN_PARITY, b.Even(), kappa);
      CloverInv(x.Even(), *tmp1, QUDA_EVEN_PARITY);
    } else {
      errorQuda("MatPCType %d not valid for DiracCloverPC", matpcType);
    }

    deleteTmp(&tmp1, reset);

  }

  void DiracCloverPC::createCoarseOp(GaugeField &Y, GaugeField &X, const Transfer &T,
				     double kappa, double mass, double mu, double mu_factor) const {
    double a = - 2.0 * kappa * mu * T.Vectors().TwistFlavor();
    CoarseOp(Y, X, T, *gauge, &clover, kappa, a, -mu_factor, QUDA_CLOVERPC_DIRAC, matpcType);
  }


  /********
   * DiracCloverHasenbuschTwist start here
   ********/

  DiracCloverHasenbuschTwist::DiracCloverHasenbuschTwist(const DiracParam &param)
    : DiracClover(param), mu(param.mu)
  { }

  DiracCloverHasenbuschTwist::DiracCloverHasenbuschTwist(const DiracCloverHasenbuschTwist &dirac) 
    : DiracClover(dirac), mu(dirac.mu)
  { }

  DiracCloverHasenbuschTwist::~DiracCloverHasenbuschTwist() { }

  DiracCloverHasenbuschTwist& DiracCloverHasenbuschTwist::operator=(const DiracCloverHasenbuschTwist &dirac)
  {
    if (&dirac != this) {
      DiracWilson::operator=(dirac);
      clover = dirac.clover;
      mu = dirac.mu;
    }
    return *this;
  }


  void DiracCloverHasenbuschTwist::M(ColorSpinorField &out, const ColorSpinorField &in) const
  {
	  bool asymmetric = ( matpcType == QUDA_MATPC_ODD_ODD_ASYMMETRIC ) || (matpcType == QUDA_MATPC_EVEN_EVEN_ASYMMETRIC );

	  if( ! asymmetric ) {
#ifndef USE_LEGACY_DSLASH
		  if ( matpcType == QUDA_MATPC_EVEN_EVEN ) {
			  // printfQuda("Applying EVEN EVEN\n");
			  ApplyWilsonCloverHasenbuschTwist(out.Even(),in.Odd(),*gauge,clover, -kappa, mu, in.Even(), QUDA_EVEN_PARITY, dagger, commDim,profile);
			  ApplyWilsonClover(out.Odd(), in.Even(), *gauge, clover, -kappa, in.Odd(), QUDA_ODD_PARITY, dagger, commDim, profile);
		  }
		  else {
			  // printfQuda("Applying Odd Odd\n");
			  ApplyWilsonClover(out.Even(),in.Odd(),*gauge,clover,-kappa,in.Even(),QUDA_EVEN_PARITY,dagger,commDim,profile);
			  ApplyWilsonCloverHasenbuschTwist(out.Odd(),in.Even(),*gauge,clover, -kappa, mu, in.Odd(), QUDA_ODD_PARITY, dagger, commDim, profile);

		  }

		  // 2 Applies of DiracClover + (1-imu gamma_5 A)psi_{!p}
		  flops += 2*1872ll*in.Volume()+(48ll + 504ll)*in.VolumeCB();
	  }
	  else {
		  if ( matpcType == QUDA_MATPC_ODD_ODD_ASYMMETRIC ) {

			  // printfQuda("Applying Odd Odd\n");
			  ApplyWilsonClover(out.Even(),in.Odd(),*gauge,clover,-kappa,in.Even(),QUDA_EVEN_PARITY,dagger,commDim,profile);
			  ApplyWilsonClover(out.Odd(),in.Even(),*gauge,clover, -kappa, mu, in.Odd(), QUDA_ODD_PARITY, dagger, commDim, profile);
		  }
	     else {
		        // printfQuda("Applying EVEN EVEN\n");
	    	 ApplyWilsonClover(out.Even(),in.Odd(),*gauge,clover, -kappa, mu, in.Even(), QUDA_EVEN_PARITY, dagger, commDim,profile);
	    	 ApplyWilsonClover(out.Odd(), in.Even(), *gauge, clover, -kappa, in.Odd(), QUDA_ODD_PARITY, dagger, commDim, profile);
	     }
		  // 2 Applies of DiracClover + (1-imu gamma_5)psi_{!p}
		  flops += 2*1872ll*in.Volume()+48ll*in.VolumeCB();
	  }
#else
    errorQuda("DiracCloverHasenbuschTwist is not implemented for USE_LEGACY_DSLASH");
#endif
  }

  void DiracCloverHasenbuschTwist::MdagM(ColorSpinorField &out, const ColorSpinorField &in) const
  {
    checkFullSpinor(out, in);

    bool reset = newTmp(&tmp1, in);
    checkFullSpinor(*tmp1, in);

    M(*tmp1, in);
    Mdag(out, *tmp1);

    deleteTmp(&tmp1, reset);
  }



  void DiracCloverHasenbuschTwist::createCoarseOp(GaugeField &Y, GaugeField &X, const Transfer &T,
				   double kappa, double mass, double mu, double mu_factor) const {
    //double a = 2.0 * kappa * mu * T.Vectors().TwistFlavor();
    //CoarseOp(Y, X, T, *gauge, &clover, kappa, a, mu_factor, QUDA_CLOVER_DIRAC, QUDA_MATPC_INVALID);
    errorQuda("Not Yet Implemented");
  }


  /* **********************************************
   * DiracCloverHasenbuschTwistPC Starts Here
   * ********************************************* */

  DiracCloverHasenbuschTwistPC::DiracCloverHasenbuschTwistPC(const DiracParam &param) :
      DiracCloverPC(param), mu(param.mu) {}

    DiracCloverHasenbuschTwistPC::DiracCloverHasenbuschTwistPC(const DiracCloverHasenbuschTwistPC &dirac)
    	: DiracCloverPC(dirac), mu(dirac.mu) { }

    DiracCloverHasenbuschTwistPC::~DiracCloverHasenbuschTwistPC() { }

    DiracCloverHasenbuschTwistPC& DiracCloverHasenbuschTwistPC::operator=(const DiracCloverHasenbuschTwistPC &dirac)
    {
	  if (&dirac != this) {
        DiracCloverPC::operator=(dirac);
        mu = dirac.mu;
      }
      return *this;
    }

    // xpay version of the above
    void DiracCloverHasenbuschTwistPC::DslashXpayTwistClovInv(ColorSpinorField &out, const ColorSpinorField &in,
    				 const QudaParity parity, const ColorSpinorField &x,
    				 const double &k, const double& b) const
      {
        checkParitySpinor(in, out);
        checkSpinorAlias(in, out);

        ApplyWilsonCloverHasenbuschTwistPCClovInv(out, in, *gauge, clover, k, b, x, parity, dagger, commDim, profile);

        //    DiracCloverPC.DslashXPay -/+ mu ( i gamma_5 ) A
        flops += (1872ll + 48ll + 504ll)*in.Volume();
      }

    // xpay version of the above
     void DiracCloverHasenbuschTwistPC::DslashXpayTwistNoClovInv(ColorSpinorField &out, const ColorSpinorField &in,
     				 const QudaParity parity, const ColorSpinorField &x,
     				 const double &k, const double &b) const
       {
         checkParitySpinor(in, out);
         checkSpinorAlias(in, out);

         ApplyWilsonCloverHasenbuschTwistPCNoClovInv(out, in, *gauge, clover, k, b, x, parity, dagger, commDim, profile);

         //    DiracCloverPC.DslashXPay -/+ mu ( i gamma_5 )
         flops += (1872ll+48)*in.Volume();
       }
  
	void DiracCloverHasenbuschTwistPC::DslashXpay(ColorSpinorField &out, const ColorSpinorField &in, 
				 const QudaParity parity, const ColorSpinorField &x,
				 const double &k) const
	{
		checkParitySpinor(in, out);
		checkSpinorAlias(in, out);

		if(parity == QUDA_EVEN_PARITY){
			DiracCloverPC::DslashXpay(out, in, parity, x, k); 
		}else if(parity == QUDA_ODD_PARITY){
			DslashXpayTwistClovInv(out, in, parity, x, k, mu);
		}
		flops += 1872ll*in.Volume();
	}

    // Apply the even-odd preconditioned clover-improved Dirac operator
    void DiracCloverHasenbuschTwistPC::M(ColorSpinorField &out, const ColorSpinorField &in) const
    {
      double kappa2 = -kappa*kappa;
      bool reset1 = newTmp(&tmp1, in);

      bool symmetric =(matpcType == QUDA_MATPC_EVEN_EVEN || matpcType == QUDA_MATPC_ODD_ODD) ? true : false;
      int odd_bit = (matpcType == QUDA_MATPC_ODD_ODD || matpcType == QUDA_MATPC_ODD_ODD_ASYMMETRIC) ? 1 : 0;
      QudaParity parity[2] = {static_cast<QudaParity>((1 + odd_bit) % 2), static_cast<QudaParity>((0 + odd_bit) % 2)};



      if (!symmetric) {

        // No need to change order of calls for dagger
        // because the asymmetric operator is actually symmetric
        // A_oo -D_oe A^{-1}_ee D_eo -> A_oo -D^\dag_oe A^{-1}_ee D^\dag_eo
        // the pieces in Dslash and DslashXPay respect the dagger


        // DiracCloverHasenbuschTwistPC::Dslash applies A^{-1}Dslash
        Dslash(*tmp1, in, parity[0]);
        // DiracClover::DslashXpay applies (A - kappa^2 D)-
        DiracClover::DslashXpay(out, *tmp1, parity[1], in, kappa2, mu);
      } else if (!dagger) { // symmetric preconditioning
        // We need two cases because M = 1-ADAD and M^\dag = 1-D^\dag A D^dag A
        // where A is actually a clover inverse.
        // This is the non-dag case: AD
		Dslash(*tmp1, in, parity[0]);
        // Then x + AD (AD)
        DslashXpayTwistClovInv(out, *tmp1, parity[1], in, kappa2, mu);
      } else { // symmetric preconditioning, dagger

        // This is the dagger: 1 - DADA
        //  i) Apply A
        CloverInv(out, in, parity[1]);
        // ii) Apply A D => ADA
        Dslash(*tmp1, out, parity[0]);
        // iii) Apply  x + D(ADA)
        DslashXpayTwistNoClovInv(out, *tmp1, parity[1], in, kappa2, mu);
      }

      deleteTmp(&tmp1, reset1);
    }

    void DiracCloverHasenbuschTwistPC::MdagM(ColorSpinorField &out, const ColorSpinorField &in) const
    {
      // need extra temporary because of symmetric preconditioning dagger
      // and for multi-gpu the input and output fields cannot alias
      bool reset = newTmp(&tmp2, in);
      M(*tmp2, in);
      Mdag(out, *tmp2);
      deleteTmp(&tmp2, reset);
    }


    void DiracCloverHasenbuschTwistPC::createCoarseOp(GaugeField &Y, GaugeField &X, const Transfer &T,
  				     double kappa, double mass, double mu, double mu_factor) const {
	  if(matpcType != QUDA_MATPC_ODD_ODD){errorQuda("currently only support QUDA_MATPC_ODD_ODD");}
	  CoarseOp(Y, X, T, *gauge, &clover, kappa, 0.5*mu, mu_factor, QUDA_CLOVER_HASENBUSCH_TWISTPC_DIRAC, matpcType);
    }


} // namespace quda
