/****************************************************************/
/* MOOSE - Multiphysics Object Oriented Simulation Environment  */
/*                                                              */
/*          All contents are licensed under LGPL V2.1           */
/*             See LICENSE for full restrictions                */
/****************************************************************/
#include "LinearIsoElasticPFDamageSym.h"
#include "libmesh/utility.h"

template <>
InputParameters
validParams<LinearIsoElasticPFDamageSym>()
{
  InputParameters params = validParams<ComputeStressBase>();
  params.addClassDescription("Phase-field fracture model energy contribution to damage "
                             "growth-isotropic elasticity and damaged stress under compressive "
                             " and tensile strain");
  params.addRequiredCoupledVar("c",
                               "Order parameter for damage, continuous between 0 and 1, 0 "
                               "represents no damage, 1 represents fully cracked");
  params.addParam<Real>("kdamage", 1e-6, "Stiffness of damaged matrix");

  return params;
}

LinearIsoElasticPFDamageSym::LinearIsoElasticPFDamageSym(const InputParameters & parameters)
  : ComputeStressBase(parameters),
    _c(coupledValue("c")),
    _kdamage(getParam<Real>("kdamage")),
    _G0_pos(declareProperty<Real>("G0_pos")),
    _dstress_dc(
        declarePropertyDerivative<RankTwoTensor>(_base_name + "stress", getVar("c", 0)->name())),
    _dG0_pos_dstrain(declareProperty<RankTwoTensor>("dG0_pos_dstrain")),
    _etens(LIBMESH_DIM),
    _epos(LIBMESH_DIM),
    _eigval(LIBMESH_DIM)
{
}

void
LinearIsoElasticPFDamageSym::computeQpStress()
{
  updateVar();
  updateJacobian();
}

void
LinearIsoElasticPFDamageSym::updateVar()
{
  // Isotropic elasticity is assumed
  Real lambda = _elasticity_tensor[_qp](0, 0, 1, 1);
  Real mu = _elasticity_tensor[_qp](0, 1, 0, 1);
  Real c = _c[_qp];
  Real xfac = _kdamage;
  if (c < 1.0)
    xfac += Utility::pow<2>(1.0 - c);

  _mechanical_strain[_qp].symmetricEigenvaluesEigenvectors(_eigval, _eigvec);

  // Tensors of outerproduct of eigen vectors
  for (unsigned int i = 0; i < LIBMESH_DIM; ++i)
    for (unsigned int j = 0; j < LIBMESH_DIM; ++j)
      for (unsigned int k = 0; k < LIBMESH_DIM; ++k)
        _etens[i](j, k) = _eigvec(j, i) * _eigvec(k, i);

  Real etr = 0.0;
  for (unsigned int i = 0; i < LIBMESH_DIM; ++i)
    etr += _eigval[i];


  RankTwoTensor stress0;
  for (unsigned int i = 0; i < LIBMESH_DIM; ++i)
  {
    stress0 +=
        _etens[i] * (lambda * etr + 2.0 * mu * _eigval[i]);

  }

  // Damage associated with stress
  _stress[_qp] = stress0 * xfac;

  Real val = 0.0;
  for (unsigned int i = 0; i < LIBMESH_DIM; ++i)
    val += Utility::pow<2>(_eigval[i]);
  val *= mu;

  // Energy with positive principal strains
  _G0_pos[_qp] = lambda * Utility::pow<2>(etr) / 2.0 + val;

  // Used in PFFracBulkRate Jacobian
  _dG0_pos_dstrain[_qp] = stress0;

  // Used in StressDivergencePFFracTensors Jacobian
  if (c < 1.0)
    _dstress_dc[_qp] = -stress0 * (2.0 * (1.0 - c));
  else
    _dstress_dc[_qp].zero();
}

void
LinearIsoElasticPFDamageSym::updateJacobian()
{
  _Jacobian_mult[_qp] = _elasticity_tensor[_qp];
}
