/*-----------------------------------------------------------------------*\
|    ___                   ____  __  __  ___  _  _______                  |
|   / _ \ _ __   ___ _ __ / ___||  \/  |/ _ \| |/ / ____| _     _         |
|  | | | | '_ \ / _ \ '_ \\___ \| |\/| | | | | ' /|  _| _| |_ _| |_       |
|  | |_| | |_) |  __/ | | |___) | |  | | |_| | . \| |__|_   _|_   _|      |
|   \___/| .__/ \___|_| |_|____/|_|  |_|\___/|_|\_\_____||_|   |_|        |
|        |_|                                                              |
|                                                                         |
|   Author: Alberto Cuoci <alberto.cuoci@polimi.it>                       |
|   CRECK Modeling Group <https://www.creckmodeling.polimi.it>            |
|   Department of Chemistry, Materials, and Chemical Engineering          |
|   Politecnico di Milano                                                 |
|   P.zza Leonardo da Vinci 32, 20133 Milano                              |
|                                                                         |
|-------------------------------------------------------------------------|
|                                                                         |
|   This file is part of the OpenSMOKE++ library.                         |
|                                                                         |
|   Copyright (C) 2026 Alberto Cuoci                                      |
|                                                                         |
|   OpenSMOKE++ is free software: you can redistribute it and/or modify   |
|   it under the terms of the GNU General Public License as published by  |
|   the Free Software Foundation, either version 3 of the License, or     |
|   (at your option) any later version.                                   |
|                                                                         |
|   OpenSMOKE++ is distributed in the hope that it will be useful,        |
|   but WITHOUT ANY WARRANTY; without even the implied warranty of        |
|   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         |
|   GNU General Public License for more details.                          |
|                                                                         |
|   You should have received a copy of the GNU General Public License     |
|   along with OpenSMOKE++. If not, see <https://www.gnu.org/licenses/>.  |
|                                                                         |
\*-----------------------------------------------------------------------*/

#ifndef OpenSMOKEpp_DictionaryUnits_H
#define OpenSMOKEpp_DictionaryUnits_H

#include <array>
#include <cstddef>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

/**
 * \brief Physical dimensions, unit parsing and conversion to SI.
 *
 * A `Dimension` is a vector of rational exponents over the seven SI base
 * quantities. Unit expressions found in input files (for example `cm/s`,
 * `kg/m3/s2`, `W/(m2.K)`, `m2/s^0.5/kg`, `kcal/mol`) are parsed at run time
 * into a factor, an offset and a dimension, checked against the dimension
 * expected by the grammar, and converted to SI:
 *
 * \code
 * value_SI = value * factor + offset
 * \endcode
 *
 * Amounts of substance are expressed in kilomoles by default (OpenSMOKE++
 * convention); see `UnitSystem`.
 *
 * The offset is non-zero only for an absolute temperature written in an
 * affine scale (`C`, `°C`, `degC`, `F`, `°F`, `degF`) as a stand-alone unit.
 * Inside compound units (`W/m2/C`, `C/min`) and for a temperature difference
 * those symbols denote intervals and only the factor is applied.
 */
namespace OpenSMOKEpp::units {

/**
 * \brief Exact rational number used for dimension exponents.
 */
struct Rational {
  /** \brief Numerator. */
  int num = 0;
  /** \brief Denominator, always positive. */
  int den = 1;

  /** \brief Builds a normalized rational `n/d`. */
  [[nodiscard]] static constexpr auto make(int n, int d = 1) -> Rational {
    if (d < 0) {
      n = -n;
      d = -d;
    }
    const int g = std::gcd(n < 0 ? -n : n, d);
    return g > 1 ? Rational{n / g, d / g} : Rational{n, d};
  }
  [[nodiscard]] constexpr auto is_zero() const noexcept -> bool {
    return num == 0;
  }
  friend constexpr auto operator==(const Rational &, const Rational &)
      -> bool = default;
  friend constexpr auto operator+(const Rational &a, const Rational &b)
      -> Rational {
    return make(a.num * b.den + b.num * a.den, a.den * b.den);
  }
  friend constexpr auto operator-(const Rational &a, const Rational &b)
      -> Rational {
    return make(a.num * b.den - b.num * a.den, a.den * b.den);
  }
  friend constexpr auto operator*(const Rational &a, const Rational &b)
      -> Rational {
    return make(a.num * b.num, a.den * b.den);
  }
  /** \brief Text such as `2`, `-3` or `1/2`. */
  [[nodiscard]] auto to_string() const -> std::string;
};

/**
 * \brief Indices of the SI base quantities in `Dimension::exponents`.
 */
enum class Base : std::size_t {
  Length = 0,          ///< metre (m)
  Mass = 1,            ///< kilogram (kg)
  Time = 2,            ///< second (s)
  Temperature = 3,     ///< kelvin (K)
  Amount = 4,          ///< mole (mol)
  Current = 5,         ///< ampere (A)
  LuminousIntensity = 6 ///< candela (cd)
};

/** \brief Number of SI base quantities. */
inline constexpr std::size_t number_of_bases = 7;

/**
 * \brief Physical dimension: rational exponents of the SI base quantities.
 *
 * `temperature_difference` marks a pure temperature that denotes an
 * interval (no offset is applied when converting `C` or `F`). It is ignored
 * by comparisons and dropped by arithmetic.
 */
struct Dimension {
  /** \brief Exponents indexed by `Base`. */
  std::array<Rational, number_of_bases> exponents{};
  /** \brief Interval semantics for a pure temperature. */
  bool temperature_difference = false;

  /** \brief Dimension of one base quantity raised to `power`. */
  [[nodiscard]] static constexpr auto base(Base b, Rational power = {1, 1})
      -> Dimension {
    Dimension d;
    d.exponents[static_cast<std::size_t>(b)] = power;
    return d;
  }
  /** \brief Returns `true` for a dimensionless quantity. */
  [[nodiscard]] constexpr auto is_dimensionless() const noexcept -> bool {
    for (const auto &e : exponents)
      if (!e.is_zero())
        return false;
    return true;
  }
  /** \brief Returns `true` for a pure temperature (Θ^1). */
  [[nodiscard]] constexpr auto is_temperature() const noexcept -> bool {
    return *this == base(Base::Temperature);
  }
  /** \brief Same dimension, flagged as a temperature difference. */
  [[nodiscard]] constexpr auto as_difference() const noexcept -> Dimension {
    Dimension d = *this;
    d.temperature_difference = true;
    return d;
  }
  /** \brief Raises the dimension to a rational power. */
  [[nodiscard]] constexpr auto pow(Rational p) const -> Dimension {
    Dimension d;
    for (std::size_t i = 0; i < number_of_bases; ++i)
      d.exponents[i] = exponents[i] * p;
    return d;
  }
  friend constexpr auto operator==(const Dimension &a, const Dimension &b)
      -> bool {
    return a.exponents == b.exponents;
  }
  friend constexpr auto operator*(const Dimension &a, const Dimension &b)
      -> Dimension {
    Dimension d;
    for (std::size_t i = 0; i < number_of_bases; ++i)
      d.exponents[i] = a.exponents[i] + b.exponents[i];
    return d;
  }
  friend constexpr auto operator/(const Dimension &a, const Dimension &b)
      -> Dimension {
    Dimension d;
    for (std::size_t i = 0; i < number_of_bases; ++i)
      d.exponents[i] = a.exponents[i] - b.exponents[i];
    return d;
  }
};

/**
 * \brief Predefined dimensions.
 *
 * Further dimensions can be composed: `dim::Mass / dim::Length.pow({3, 1})`.
 */
namespace dim {
inline constexpr Dimension Dimensionless{};
inline constexpr Dimension Length = Dimension::base(Base::Length);
inline constexpr Dimension Mass = Dimension::base(Base::Mass);
inline constexpr Dimension Time = Dimension::base(Base::Time);
/** \brief Absolute temperature (`C`/`F` converted with their offset). */
inline constexpr Dimension Temperature = Dimension::base(Base::Temperature);
/** \brief Temperature interval (`C`/`F` converted without offset). */
inline constexpr Dimension TemperatureDifference = Temperature.as_difference();
inline constexpr Dimension Amount = Dimension::base(Base::Amount);
inline constexpr Dimension Current = Dimension::base(Base::Current);
inline constexpr Dimension LuminousIntensity =
    Dimension::base(Base::LuminousIntensity);

inline constexpr Dimension Area = Length * Length;
inline constexpr Dimension Volume = Area * Length;
inline constexpr Dimension InverseLength = Dimensionless / Length;
inline constexpr Dimension Frequency = Dimensionless / Time;
inline constexpr Dimension Velocity = Length / Time;
inline constexpr Dimension Acceleration = Velocity / Time;
inline constexpr Dimension Force = Mass * Acceleration;
inline constexpr Dimension Pressure = Force / Area;
inline constexpr Dimension Energy = Force * Length;
inline constexpr Dimension Power = Energy / Time;
inline constexpr Dimension Density = Mass / Volume;
inline constexpr Dimension SpecificVolume = Volume / Mass;
inline constexpr Dimension NumberDensity = Dimensionless / Volume;
inline constexpr Dimension Concentration = Amount / Volume;
inline constexpr Dimension MolarMass = Mass / Amount;
inline constexpr Dimension MassFlowRate = Mass / Time;
inline constexpr Dimension MolarFlowRate = Amount / Time;
inline constexpr Dimension VolumetricFlowRate = Volume / Time;
inline constexpr Dimension MassFlux = Mass / Area / Time;
inline constexpr Dimension HeatFlux = Power / Area;
inline constexpr Dimension HeatTransferCoefficient = HeatFlux / Temperature;
inline constexpr Dimension ThermalConductivity = Power / Length / Temperature;
inline constexpr Dimension DynamicViscosity = Pressure * Time;
inline constexpr Dimension KinematicViscosity = Area / Time;
inline constexpr Dimension Diffusivity = Area / Time;
inline constexpr Dimension SurfaceTension = Force / Length;
inline constexpr Dimension SpecificEnergy = Energy / Mass;
inline constexpr Dimension MolarEnergy = Energy / Amount;
inline constexpr Dimension HeatCapacity = Energy / Temperature;
inline constexpr Dimension SpecificHeatCapacity = SpecificEnergy / Temperature;
inline constexpr Dimension MolarHeatCapacity = MolarEnergy / Temperature;
inline constexpr Dimension PowerDensity = Power / Volume;
inline constexpr Dimension HeatingRate = Temperature / Time;
inline constexpr Dimension SurfaceDensity = Mass / Area;
inline constexpr Dimension SurfaceConcentration = Amount / Area;
} // namespace dim

/**
 * \brief Unit of amount of substance used by the target unit system.
 */
enum class AmountBase {
  /** \brief Strict SI: mole (J/mol, mol/m3, kg/mol). */
  Mole,
  /** \brief SI with kilomole (J/kmol, kmol/m3, kg/kmol), the convention of
     OpenSMOKE++. */
  Kilomole
};

/**
 * \brief Target unit system for conversions.
 *
 * All conversions produce SI values; the only option is the unit of amount
 * of substance. The default is the kilomole, the convention of the whole
 * OpenSMOKE++ framework (J/kmol, kmol/m3, kg/kmol); with `AmountBase::Mole`
 * the result is strict SI.
 */
struct UnitSystem {
  /** \brief Unit of amount of substance (default: kilomole). */
  AmountBase amount = AmountBase::Kilomole;

  friend constexpr auto operator==(const UnitSystem &, const UnitSystem &)
      -> bool = default;
};

/** \brief SI with kilomole, as used by OpenSMOKE++ (the default). */
inline constexpr UnitSystem si_kmol{AmountBase::Kilomole};
/** \brief Strict SI (mole). */
inline constexpr UnitSystem si{AmountBase::Mole};

/**
 * \brief Exception raised for unknown or malformed unit expressions and for
 * dimension mismatches.
 */
class UnitError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

/**
 * \brief Result of parsing a unit expression.
 */
struct UnitConversion {
  /** \brief Multiplicative factor to SI. */
  double factor = 1.0;
  /**
   * \brief Additive offset to SI (K) when the expression is a stand-alone
   * affine temperature unit (`C`, `F`); zero otherwise.
   */
  double offset = 0.0;
  /** \brief Dimension of the expression. */
  Dimension dimension{};
};

/**
 * \brief Parses a unit expression.
 *
 * Syntax:
 * - products with `.`, `*` or `·`; each `/` divides by the **next factor
 *   only**, so `kg/m3/s2` is kg·m⁻³·s⁻²; parentheses group: `W/(m2.K)`;
 * - exponents glued (`m3`, `s-1`, `s0.5`) or with `^` (`m^3`, `s^-1`,
 *   `s^0.5`, `s^(1/2)`); `sqrt(...)` is the power 1/2;
 * - a number is a pure factor (`1/s`);
 * - `-`, `dimensionless`, `#`, `%`, `ppm` are dimensionless units.
 *
 * Symbols are case-sensitive. SI prefixes (`Y Z E P T G M k h da d c m u µ
 * n p f a`) are accepted on metric units; an exact symbol always wins over a
 * prefixed reading (`min` is the minute, `Pa` the pascal). See the README
 * for the full list of symbols.
 *
 * \param[in] text Unit expression.
 * \return Factor, offset and dimension.
 * \warning Throws `UnitError` with a descriptive message (and a suggestion
 * for misspelled symbols) if the expression is not valid.
 */
[[nodiscard]] auto parse_unit(std::string_view text) -> UnitConversion;

/**
 * \brief Converts a value to SI after checking the dimension.
 *
 * \param[in] value Magnitude expressed in `unit`.
 * \param[in] unit Unit expression.
 * \param[in] expected Expected dimension; `dim::Temperature` applies the
 * offset of `C`/`F`, `dim::TemperatureDifference` does not.
 * \param[in] system Target system (default: kilomole as amount unit).
 * \return Value in SI units.
 * \warning Throws `UnitError` if the unit is invalid or its dimension
 * differs from `expected`.
 */
[[nodiscard]] auto to_si(double value, std::string_view unit,
                         const Dimension &expected,
                         const UnitSystem &system = si_kmol) -> double;

/**
 * \brief Converts a value expressed in SI to the given unit.
 *
 * \param[in] value_si Value in SI units.
 * \param[in] unit Target unit expression.
 * \param[in] expected Expected dimension (see `to_si`).
 * \param[in] system Unit system of `value_si`.
 * \return Value expressed in `unit`.
 * \warning Throws `UnitError` as `to_si`.
 */
[[nodiscard]] auto from_si(double value_si, std::string_view unit,
                           const Dimension &expected,
                           const UnitSystem &system = si_kmol) -> double;

/**
 * \brief Checks a unit expression against an expected dimension.
 *
 * \param[in] unit Unit expression.
 * \param[in] expected Expected dimension.
 * \return Empty if the unit is valid and compatible, otherwise the reason.
 */
[[nodiscard]] auto check_unit(std::string_view unit, const Dimension &expected)
    -> std::optional<std::string>;

/**
 * \brief Returns the SI unit of a dimension, e.g. `kg/m3/s2`, `1/s`, `-`.
 *
 * \param[in] dimension Dimension.
 * \param[in] system Unit system (`kmol` for `AmountBase::Kilomole`, the
 * default; `mol` for `AmountBase::Mole`).
 */
[[nodiscard]] auto si_unit(const Dimension &dimension,
                           const UnitSystem &system = si_kmol) -> std::string;

/**
 * \brief Returns a readable description, e.g. `velocity [m/s]` or, for
 * dimensions without a common name, just the SI unit in brackets.
 *
 * \param[in] dimension Dimension.
 * \param[in] system Unit system used for the unit in brackets.
 */
[[nodiscard]] auto describe(const Dimension &dimension,
                            const UnitSystem &system = si_kmol) -> std::string;
} // namespace OpenSMOKEpp::units

#endif // OpenSMOKEpp_DictionaryUnits_H
