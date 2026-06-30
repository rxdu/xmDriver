/*
 * units.hpp
 *
 * Strong scalar quantities for actuator commands. The old API took bare floats
 * (SetSpeed(float) vs SetPosition(float) looked identical), so unit/quantity
 * mix-ups were invisible. These tagged scalars make Rpm, Radian, Ampere, etc.
 * distinct types — the compiler rejects passing a current where a speed is
 * expected. Reach the raw number explicitly via .value().
 *
 * Units (documented, enforced by naming): Rpm = rev/min, Radian = rad,
 * Ampere = A, NewtonMetre = N·m, Ratio = dimensionless [0,1] (duty / brake).
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#pragma once

namespace xmotion::hal {

template <typename Tag>
class Scalar {
 public:
  constexpr Scalar() = default;
  constexpr explicit Scalar(double v) : v_(v) {}
  constexpr double value() const { return v_; }

  constexpr bool operator==(Scalar o) const { return v_ == o.v_; }
  constexpr bool operator!=(Scalar o) const { return v_ != o.v_; }
  constexpr bool operator<(Scalar o) const { return v_ < o.v_; }
  constexpr bool operator>(Scalar o) const { return v_ > o.v_; }

 private:
  double v_ = 0.0;
};

namespace unit_tag {
struct Rpm {};
struct Radian {};
struct Ampere {};
struct NewtonMetre {};
struct Ratio {};
}  // namespace unit_tag

using Rpm = Scalar<unit_tag::Rpm>;                  // rev/min
using Radian = Scalar<unit_tag::Radian>;            // rad
using Ampere = Scalar<unit_tag::Ampere>;            // A
using NewtonMetre = Scalar<unit_tag::NewtonMetre>;  // N·m
using Ratio = Scalar<unit_tag::Ratio>;              // dimensionless [0,1]

}  // namespace xmotion::hal
