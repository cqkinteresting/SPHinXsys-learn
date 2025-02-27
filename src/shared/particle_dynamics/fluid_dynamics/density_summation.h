/* ------------------------------------------------------------------------- *
 *                                SPHinXsys                                  *
 * ------------------------------------------------------------------------- *
 * SPHinXsys (pronunciation: s'finksis) is an acronym from Smoothed Particle *
 * Hydrodynamics for industrial compleX systems. It provides C++ APIs for    *
 * physical accurate simulation and aims to model coupled industrial dynamic *
 * systems including fluid, solid, multi-body dynamics and beyond with SPH   *
 * (smoothed particle hydrodynamics), a meshless computational method using  *
 * particle discretization.                                                  *
 *                                                                           *
 * SPHinXsys is partially funded by German Research Foundation               *
 * (Deutsche Forschungsgemeinschaft) DFG HU1527/6-1, HU1527/10-1,            *
 *  HU1527/12-1 and HU1527/12-4.                                             *
 *                                                                           *
 * Portions copyright (c) 2017-2023 Technical University of Munich and       *
 * the authors' affiliations.                                                *
 *                                                                           *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may   *
 * not use this file except in compliance with the License. You may obtain a *
 * copy of the License at http://www.apache.org/licenses/LICENSE-2.0.        *
 *                                                                           *
 * ------------------------------------------------------------------------- */
/**
 * @file density_summation.h
 * @brief Here, we define the algorithm classes for computing
 * the density of a continuum by kernel function summation.
 * @details We are using templates and their explicit or partial specializations
 * to identify variations of the interaction types..
 * @author Xiangyu Hu
 */

#ifndef DENSITY_SUMMATION_INNER_H
#define DENSITY_SUMMATION_INNER_H

#include "base_fluid_dynamics.h"

namespace SPH
{
namespace fluid_dynamics
{
template <typename... InteractionTypes>
class DensitySummation;

template <class DataDelegationType>
class DensitySummation<Base, DataDelegationType>
    : public LocalDynamics, public DataDelegationType
{
  public:
    template <class BaseRelationType>
    explicit DensitySummation(BaseRelationType &base_relation);
    virtual ~DensitySummation(){};

  protected:
    StdLargeVec<Real> &rho_, &mass_, &rho_sum_;
    Real rho0_, inv_sigma0_, W0_;
};

template <>
class DensitySummation<Inner<Base>> : public DensitySummation<Base, FluidDataInner>
{
  public:
    explicit DensitySummation(BaseInnerRelation &inner_relation)
        : DensitySummation<Base, FluidDataInner>(inner_relation){};
    virtual ~DensitySummation(){};

  protected:
    void assignDensity(size_t index_i) { rho_[index_i] = rho_sum_[index_i]; };
    void reinitializeDensity(size_t index_i) { rho_[index_i] = SMAX(rho_sum_[index_i], rho0_); };
};

template <>
class DensitySummation<Inner<>> : public DensitySummation<Inner<Base>>
{
  public:
    explicit DensitySummation(BaseInnerRelation &inner_relation)
        : DensitySummation<Inner<Base>>(inner_relation){};
    virtual ~DensitySummation(){};
    void interaction(size_t index_i, Real dt = 0.0);
    void update(size_t index_i, Real dt = 0.0) { assignDensity(index_i); };
};
using DensitySummationInner = DensitySummation<Inner<>>;

template <>
class DensitySummation<Inner<Adaptive>> : public DensitySummation<Inner<Base>>
{
  public:
    explicit DensitySummation(BaseInnerRelation &inner_relation);
    virtual ~DensitySummation(){};
    void interaction(size_t index_i, Real dt = 0.0);
    void update(size_t index_i, Real dt = 0.0) { assignDensity(index_i); };

  protected:
    SPHAdaptation &sph_adaptation_;
    Kernel &kernel_;
    StdLargeVec<Real> &h_ratio_;
};

template <>
class DensitySummation<Contact<Base>> : public DensitySummation<Base, FluidContactData>
{
  public:
    explicit DensitySummation(BaseContactRelation &contact_relation);
    virtual ~DensitySummation(){};

  protected:
    StdVec<Real> contact_inv_rho0_;
    StdVec<StdLargeVec<Real> *> contact_mass_;
    Real ContactSummation(size_t index_i);
};

template <>
class DensitySummation<Contact<>> : public DensitySummation<Contact<Base>>
{
  public:
    explicit DensitySummation(BaseContactRelation &contact_relation)
        : DensitySummation<Contact<Base>>(contact_relation){};
    virtual ~DensitySummation(){};
    void interaction(size_t index_i, Real dt = 0.0);
};

template <>
class DensitySummation<Contact<Adaptive>> : public DensitySummation<Contact<Base>>
{
  public:
    explicit DensitySummation(BaseContactRelation &contact_relation);
    virtual ~DensitySummation(){};
    void interaction(size_t index_i, Real dt = 0.0);

  protected:
    SPHAdaptation &sph_adaptation_;
    StdLargeVec<Real> &h_ratio_;
};

template <typename... SummationType>
class DensitySummation<Inner<FreeSurface, SummationType...>> : public DensitySummation<Inner<SummationType...>>
{
  public:
    template <typename... Args>
    explicit DensitySummation(Args &&...args)
        : DensitySummation<Inner<SummationType...>>(std::forward<Args>(args)...){};
    virtual ~DensitySummation(){};
    void update(size_t index_i, Real dt = 0.0)
    {
        DensitySummation<Inner<SummationType...>>::reinitializeDensity(index_i);
    };
};
using DensitySummationFreeSurfaceInner = DensitySummation<Inner<FreeSurface>>;

template <typename... SummationType>
class DensitySummation<Inner<FreeStream, SummationType...>> : public DensitySummation<Inner<SummationType...>>
{
  public:
    template <typename... Args>
    explicit DensitySummation(Args &&...args);
    virtual ~DensitySummation(){};
    void update(size_t index_i, Real dt = 0.0);

  protected:
    StdLargeVec<int> &indicator_;
    bool isNearFreeSurface(size_t index_i);
};

template <class InnerInteractionType, class... ContactInteractionTypes>
using BaseDensitySummationComplex = ComplexInteraction<DensitySummation<InnerInteractionType, ContactInteractionTypes...>>;

using DensitySummationComplex = BaseDensitySummationComplex<Inner<>, Contact<>>;
using DensitySummationComplexAdaptive = BaseDensitySummationComplex<Inner<Adaptive>, Contact<Adaptive>>;
using DensitySummationComplexFreeSurface = BaseDensitySummationComplex<Inner<FreeSurface>, Contact<>>;
using DensitySummationFreeSurfaceComplexAdaptive = BaseDensitySummationComplex<Inner<FreeSurface, Adaptive>, Contact<Adaptive>>;
using DensitySummationFreeStreamComplex = BaseDensitySummationComplex<Inner<FreeStream>, Contact<>>;
using DensitySummationFreeStreamComplexAdaptive = BaseDensitySummationComplex<Inner<FreeStream, Adaptive>, Contact<Adaptive>>;
} // namespace fluid_dynamics
} // namespace SPH
#endif // DENSITY_SUMMATION_INNER_H