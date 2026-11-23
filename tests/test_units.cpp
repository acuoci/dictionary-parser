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

#include "DictionaryUnits.h"
#include "test_framework.h"
#include "test_support.h"

#include <cmath>
#include <string>
#include <vector>

using namespace OpenSMOKEpp;
using namespace OpenSMOKEpp::units;
using testing::attach;
using testing::kw;
using testing::parse;

namespace {
auto close(const double actual, const double expected) -> bool {
  return std::abs(actual - expected) <= 1e-12 * std::max(1.0, std::abs(expected));
}

// Conversion factor to strict SI (mole), independent of the default system.
auto factor(const char *unit, const Dimension &expected) -> double {
  return to_si(1.0, unit, expected, si);
}

// Compile-time dimension algebra.
static_assert(dim::Pressure == dim::Force / dim::Area);
static_assert(dim::Energy == dim::Pressure * dim::Volume);
static_assert(dim::Diffusivity == dim::KinematicViscosity);
static_assert(dim::Velocity.exponents[0] == Rational{1, 1});
static_assert(dim::Velocity.exponents[2] == Rational{-1, 1});
static_assert(dim::TemperatureDifference == dim::Temperature);
static_assert(dim::TemperatureDifference.temperature_difference);
static_assert(dim::Length.pow(Rational::make(1, 2)).exponents[0] ==
              Rational{1, 2});
static_assert(Rational::make(2, -4) == Rational{-1, 2});
} // namespace

// ---------------------------------------------------------------------------
// Unit engine
// ---------------------------------------------------------------------------
TEST_CASE("units", "lengths, masses, times and prefixes") {
  CHECK(close(factor("m", dim::Length), 1.0));
  CHECK(close(factor("cm", dim::Length), 0.01));
  CHECK(close(factor("mm", dim::Length), 1e-3));
  CHECK(close(factor("km", dim::Length), 1e3));
  CHECK(close(factor("um", dim::Length), 1e-6));
  CHECK(close(factor("\xC2\xB5m", dim::Length), 1e-6));
  CHECK(close(factor("micron", dim::Length), 1e-6));
  CHECK(close(factor("nm", dim::Length), 1e-9));
  CHECK(close(factor("angstrom", dim::Length), 1e-10));
  CHECK(close(factor("dm", dim::Length), 0.1));
  CHECK(close(factor("dam", dim::Length), 10.0));
  CHECK(close(factor("in", dim::Length), 0.0254));
  CHECK(close(factor("ft", dim::Length), 0.3048));
  CHECK(close(factor("kg", dim::Mass), 1.0));
  CHECK(close(factor("g", dim::Mass), 1e-3));
  CHECK(close(factor("lb", dim::Mass), 0.45359237));
  CHECK(close(factor("oz", dim::Mass), 0.028349523125));
  CHECK(close(factor("ms", dim::Time), 1e-3));
  CHECK(close(factor("min", dim::Time), 60.0));
  CHECK(close(factor("h", dim::Time), 3600.0));
  CHECK(close(factor("hr", dim::Time), 3600.0));
  CHECK(close(factor("kmol", dim::Amount), 1e3));
}

TEST_CASE("units", "exact symbols win over prefixed readings") {
  CHECK(close(factor("min", dim::Time), 60.0));       // not milli-inch
  CHECK(close(factor("Pa", dim::Pressure), 1.0));     // not peta-year
  CHECK(close(factor("hPa", dim::Pressure), 100.0));  // hecto-pascal
  CHECK(close(factor("cd", dim::LuminousIntensity), 1.0));
  CHECK(check_unit("kmin", dim::Time).has_value()); // min is not prefixable
  CHECK(check_unit("kC", dim::Temperature).has_value());
}

TEST_CASE("units", "compound units with the OpenSMOKE++ conventions") {
  CHECK(close(factor("cm/s", dim::Velocity), 0.01));
  CHECK(close(factor("km/h", dim::Velocity), 1000.0 / 3600.0));
  CHECK(close(factor("km/hr", dim::Velocity), 1000.0 / 3600.0));
  CHECK(close(factor("g/cm3", dim::Density), 1000.0));
  CHECK(close(factor("kg/m3/s2", dim::Mass / dim::Volume / dim::Time.pow({2, 1})), 1.0));
  CHECK(close(factor("W/m2/K", dim::HeatTransferCoefficient), 1.0));
  CHECK(close(factor("W/(m2.K)", dim::HeatTransferCoefficient), 1.0));
  CHECK(close(factor("W/m2/C", dim::HeatTransferCoefficient), 1.0));
  CHECK(close(factor("kcal/m2/K/s", dim::HeatTransferCoefficient), 4184.0));
  CHECK(close(factor("kW/m2", dim::HeatFlux), 1000.0));
  CHECK(close(factor("W/m/K", dim::ThermalConductivity), 1.0));
  CHECK(close(factor("J/mol/K", dim::MolarHeatCapacity), 1.0));
  CHECK(close(factor("cal/mol", dim::MolarEnergy), 4.184));
  CHECK(close(factor("kcal/mol", dim::MolarEnergy), 4184.0));
  CHECK(close(factor("kJ/kg/K", dim::SpecificHeatCapacity), 1000.0));
  CHECK(close(factor("Pa.s", dim::DynamicViscosity), 1.0));
  CHECK(close(factor("g/cm/s", dim::DynamicViscosity), 0.1));
  CHECK(close(factor("cm2/s", dim::Diffusivity), 1e-4));
  CHECK(close(factor("dyn/cm", dim::SurfaceTension), 1e-3));
  CHECK(close(factor("mN/m", dim::SurfaceTension), 1e-3));
  CHECK(close(factor("kg/kmol", dim::MolarMass), 1e-3));
  CHECK(close(factor("kmol/m3/s", dim::Concentration / dim::Time), 1e3));
  CHECK(close(factor("#/cm3", dim::NumberDensity), 1e6));
  CHECK(close(factor("m3/s/kg", dim::VolumetricFlowRate / dim::Mass), 1.0));
  CHECK(close(factor("kg\xC2\xB7m-3", dim::Density), 1.0));
  CHECK(close(factor("kg*m^-3", dim::Density), 1.0));
  CHECK(close(factor("kg.m^(-3)", dim::Density), 1.0));
}

TEST_CASE("units", "pressure, volume, energy and other units") {
  CHECK(close(factor("bar", dim::Pressure), 1e5));
  CHECK(close(factor("mbar", dim::Pressure), 100.0));
  CHECK(close(factor("atm", dim::Pressure), 101325.0));
  CHECK(close(factor("torr", dim::Pressure), 101325.0 / 760.0));
  CHECK(close(factor("mmHg", dim::Pressure), 133.322387415));
  CHECK(close(factor("psi", dim::Pressure), 6894.757293168361));
  CHECK(close(factor("kPa", dim::Pressure), 1e3));
  CHECK(close(factor("MPa", dim::Pressure), 1e6));
  CHECK(close(factor("l", dim::Volume), 1e-3));
  CHECK(close(factor("L", dim::Volume), 1e-3));
  CHECK(close(factor("mL", dim::Volume), 1e-6));
  CHECK(close(factor("dm3", dim::Volume), 1e-3));
  CHECK(close(factor("cm3", dim::Volume), 1e-6));
  CHECK(close(factor("cal", dim::Energy), 4.184));
  CHECK(close(factor("kWh", dim::Energy), 3.6e6));
  CHECK(close(factor("BTU", dim::Energy), 1055.05585262));
  CHECK(close(factor("eV", dim::Energy), 1.602176634e-19));
  CHECK(close(factor("erg", dim::Energy), 1e-7));
  CHECK(close(factor("Hz", dim::Frequency), 1.0));
  CHECK(close(factor("1/s", dim::Frequency), 1.0));
  CHECK(close(factor("1/min", dim::Frequency), 1.0 / 60.0));
  CHECK(close(factor("rpm", dim::Frequency), 2.0 * 3.14159265358979323846 / 60.0));
  CHECK(close(factor("deg", dim::Dimensionless), 3.14159265358979323846 / 180.0));
  CHECK(close(factor("rad", dim::Dimensionless), 1.0));
  CHECK(close(factor("%", dim::Dimensionless), 0.01));
  CHECK(close(factor("ppm", dim::Dimensionless), 1e-6));
  CHECK(close(factor("-", dim::Dimensionless), 1.0));
  CHECK(close(factor("dimensionless", dim::Dimensionless), 1.0));
}

TEST_CASE("units", "fractional exponents") {
  const Dimension root_time = dim::Time.pow(Rational::make(1, 2));
  CHECK(parse_unit("m2/s^0.5/kg").dimension ==
        dim::Area / root_time / dim::Mass);
  CHECK(parse_unit("m2/s0.5").dimension == dim::Area / root_time);
  CHECK(parse_unit("s^(1/2)").dimension == root_time);
  CHECK(parse_unit("sqrt(s)").dimension == root_time);
  CHECK(close(parse_unit("sqrt(min)").factor, std::sqrt(60.0)));
  CHECK(parse_unit("kg.m/kmol/sqrt(K)/s").dimension ==
        dim::Mass * dim::Length / dim::Amount /
            dim::Temperature.pow(Rational::make(1, 2)) / dim::Time);
}

TEST_CASE("units", "absolute temperatures and temperature differences") {
  CHECK(close(to_si(300.0, "K", dim::Temperature), 300.0));
  CHECK(close(to_si(25.0, "C", dim::Temperature), 298.15));
  CHECK(close(to_si(25.0, "\xC2\xB0" "C", dim::Temperature), 298.15));
  CHECK(close(to_si(25.0, "degC", dim::Temperature), 298.15));
  CHECK(close(to_si(32.0, "F", dim::Temperature), 273.15));
  CHECK(close(to_si(212.0, "F", dim::Temperature), 373.15));
  CHECK(close(to_si(491.67, "R", dim::Temperature), 273.15));
  CHECK(close(to_si(10.0, "C", dim::TemperatureDifference), 10.0));
  CHECK(close(to_si(18.0, "F", dim::TemperatureDifference), 10.0));
  CHECK(close(to_si(60.0, "C/min", dim::HeatingRate), 1.0));
  CHECK(close(to_si(1.0, "W/m2/F", dim::HeatTransferCoefficient), 1.8));
  CHECK(close(from_si(298.15, "C", dim::Temperature), 25.0));
  CHECK(close(from_si(0.195, "cm/s", dim::Velocity), 19.5));
}

TEST_CASE("units", "errors are explicit and suggest corrections") {
  CHECK_THROWS_AS_WITH((void)parse_unit("Kg/m3"), UnitError,
                       "invalid unit 'Kg/m3': unknown unit symbol 'Kg' (did "
                       "you mean 'kg'?)");
  CHECK_THROWS_WITH((void)parse_unit("pa"), "(did you mean 'Pa'?)");
  CHECK_THROWS_WITH((void)parse_unit("moles"), "unknown unit symbol 'moles'");
  CHECK_THROWS_WITH((void)parse_unit(""), "empty unit expression");
  CHECK_THROWS_WITH((void)parse_unit("m/"), "missing unit after an operator");
  CHECK_THROWS_WITH((void)parse_unit("W/(m2.K"), "missing ')'");
  CHECK_THROWS_WITH((void)parse_unit("m^"), "invalid exponent");
  CHECK_THROWS_WITH((void)parse_unit("m-"), "unexpected '-'");
  CHECK_THROWS_WITH((void)to_si(1.0, "K", dim::Velocity),
                    "units 'K' (temperature [K]) are not compatible with the "
                    "expected velocity [m/s]");
  CHECK_THROWS_WITH((void)to_si(1.0, "kcal/m2/K", dim::HeatTransferCoefficient),
                    "are not compatible with the expected heat transfer "
                    "coefficient [kg/s3/K]");
  CHECK(check_unit("cm/s", dim::Velocity) == std::nullopt);
  CHECK(check_unit("cm", dim::Velocity).has_value());
}

TEST_CASE("units", "descriptions and SI units") {
  CHECK(si_unit(dim::Velocity) == "m/s");
  CHECK(si_unit(dim::Density) == "kg/m3");
  CHECK(si_unit(dim::Energy) == "kg.m2/s2");
  CHECK(si_unit(dim::HeatTransferCoefficient) == "kg/s3/K");
  CHECK(si_unit(dim::Frequency) == "1/s");
  CHECK(si_unit(dim::Dimensionless) == "-");
  CHECK(si_unit(dim::Length.pow(Rational::make(1, 2))) == "m^(1/2)");
  CHECK(describe(dim::Velocity) == "velocity [m/s]");
  CHECK(describe(dim::TemperatureDifference) == "temperature difference [K]");
  CHECK(describe(dim::Length.pow(Rational::make(1, 2))) == "dimension [m^(1/2)]");
}

// ---------------------------------------------------------------------------
// Integration with grammars and dictionaries
// ---------------------------------------------------------------------------
namespace {
const char *const measures_source = "Dictionary d\n"
                                    "{\n"
                                    "  @Velocity     19.5 cm/s;\n"
                                    "  @Temperature  25 C;\n"
                                    "  @DeltaT       10 C;\n"
                                    "  @Times        1 2 3 min;\n"
                                    "  @Lengths      1 m 20 cm;\n"
                                    "  @Raw          300 K;\n"
                                    "}\n";

auto measures_grammar() -> std::vector<DictionaryKeyWord> {
  return {kw("@Velocity", SINGLE_MEASURE).SetDimension(dim::Velocity),
          kw("@Temperature", SINGLE_MEASURE).SetDimension(dim::Temperature),
          kw("@DeltaT", SINGLE_MEASURE).SetDimension(dim::TemperatureDifference),
          kw("@Times", VECTOR_MEASURE).SetDimension(dim::Time),
          kw("@Lengths", VECTOR_MEASURE).SetDimension(dim::Length),
          kw("@Raw", SINGLE_MEASURE)};
}
} // namespace

TEST_CASE("measures", "values are converted to SI") {
  auto manager = parse(measures_source);
  auto &d = manager("d");
  attach(d, measures_grammar());
  CHECK(close(d.Read<double>("@Velocity"), 0.195));
  CHECK(close(d.Read<double>("@Temperature"), 298.15));
  CHECK(close(d.Read<double>("@DeltaT"), 10.0));
  CHECK(d.Read<std::vector<double>>("@Times") ==
        (std::vector<double>{60.0, 120.0, 180.0}));
  const auto lengths = d.Read<std::vector<double>>("@Lengths");
  CHECK(close(lengths.at(0), 1.0));
  CHECK(close(lengths.at(1), 0.2));

  // Without a dimension, a measure is read as (value, units).
  CHECK((d.Read<std::pair<double, std::string>>("@Raw") ==
        (std::pair<double, std::string>{300.0, "K"})));
}

TEST_CASE("measures", "units are validated when the grammar is attached") {
  auto manager = parse("Dictionary d\n"
                       "{\n"
                       "  @Velocity  19.5 K;\n"
                       "  @Length    1 Kg;\n"
                       "  @Times     1 min 2;\n"
                       "  @Pressure  1;\n"
                       "}\n");
  try {
    attach(manager("d"),
           {kw("@Velocity", SINGLE_MEASURE).SetDimension(dim::Velocity),
            kw("@Length", SINGLE_MEASURE).SetDimension(dim::Length),
            kw("@Times", VECTOR_MEASURE).SetDimension(dim::Time),
            kw("@Pressure", SINGLE_MEASURE).SetDimension(dim::Pressure),
            kw("@Mandatory", SINGLE_INT, true)});
    CHECK(false);
  } catch (const std::exception &error) {
    const std::string what = error.what();
    CHECK(what.find("5 errors found") != std::string::npos);
    CHECK(what.find("the compulsory keyword @Mandatory is missing") !=
          std::string::npos);
    CHECK(what.find("test.dic:3:19: error: keyword @Velocity: units 'K' "
                    "(temperature [K]) are not compatible with the expected "
                    "velocity [m/s]") != std::string::npos);
    CHECK(what.find("test.dic:4:16: error: keyword @Length: invalid unit 'Kg': "
                    "unknown unit symbol 'Kg' (did you mean 'kg'?)") !=
          std::string::npos);
    CHECK(what.find("test.dic:5:3: error: keyword @Times: pairs of a number "
                    "and units") != std::string::npos);
    CHECK(what.find("test.dic:6:3: error: keyword @Pressure: a measure "
                    "requires exactly two values") != std::string::npos);
  }
}

TEST_CASE("measures", "dimensions only on measure keywords") {
  auto manager = parse("Dictionary d { @X 1; }");
  CHECK_THROWS_WITH(
      attach(manager("d"), {kw("@X", SINGLE_DOUBLE).SetDimension(dim::Length)}),
      "the keyword @X declares a dimension (length [m]), but only "
      "single_measure and vector_measure keywords can have one");
}

TEST_CASE("measures", "grammar files can declare dimensions") {
  testing::TemporaryDirectory dir;
  const auto grammar = dir.write("velocity.grammar",
                                 "keyword: @Velocity\n"
                                 "type: single_measure\n"
                                 "short_comment: Inlet velocity\n"
                                 "is_compulsory: true\n"
                                 "compulsory_alternatives: none\n"
                                 "required_keywords: none\n"
                                 "conflicting_keywords: none\n"
                                 "dimension: m/s\n"
                                 "\n"
                                 "keyword: @DeltaT\n"
                                 "type: single_measure\n"
                                 "short_comment: Temperature rise\n"
                                 "is_compulsory: false\n"
                                 "compulsory_alternatives: none\n"
                                 "required_keywords: none\n"
                                 "conflicting_keywords: none\n"
                                 "dimension: K difference\n");
  auto manager = parse("Dictionary d { @Velocity 36 km/h; @DeltaT 9 F; }");
  auto &d = manager("d");
  d.SetGrammar(grammar.string());
  CHECK(close(d.Read<double>("@Velocity"), 10.0));
  CHECK(close(d.Read<double>("@DeltaT"), 5.0));

  const auto bad = dir.write("bad.grammar",
                             "keyword: @V\ntype: single_measure\n"
                             "short_comment: x\nis_compulsory: false\n"
                             "compulsory_alternatives: none\n"
                             "required_keywords: none\nconflicting_keywords: "
                             "none\ndimension: m/s difference\n");
  CHECK_THROWS_WITH(d.SetGrammar(bad.string()),
                    "Only a temperature dimension can be marked as "
                    "'difference'");
}

TEST_CASE("measures", "the keyword table shows the SI unit") {
  testing::ListGrammar grammar(
      {kw("@Velocity", SINGLE_MEASURE).SetDimension(dim::Velocity)});
  auto manager = parse("Dictionary d { @Help; }");
  CHECK_THROWS_WITH(manager("d").SetGrammar(grammar), "[m/s] test keyword");
}

TEST_CASE("units", "kilomole-based SI (OpenSMOKE++ convention)") {
  CHECK(close(to_si(1.0, "J/mol", dim::MolarEnergy, si_kmol), 1000.0));
  CHECK(close(to_si(1.0, "kcal/mol", dim::MolarEnergy, si_kmol), 4.184e6));
  CHECK(close(to_si(1.0, "kmol/s", dim::MolarFlowRate, si_kmol), 1.0));
  CHECK(close(to_si(1.0, "mol/s", dim::MolarFlowRate, si_kmol), 1e-3));
  CHECK(close(to_si(1.0, "kg/kmol", dim::MolarMass, si_kmol), 1.0));
  CHECK(close(to_si(1.0, "mol/cm3", dim::Concentration, si_kmol), 1e3));
  CHECK(close(to_si(1.0, "cm/s", dim::Velocity, si_kmol), 0.01)); // no amount
  CHECK(close(from_si(1000.0, "J/mol", dim::MolarEnergy, si_kmol), 1.0));
  CHECK(si_unit(dim::MolarEnergy, si_kmol) == "kg.m2/s2/kmol");
  CHECK(describe(dim::Concentration, si_kmol) == "concentration [kmol/m3]");
}

TEST_CASE("measures", "the unit system is selected on the manager") {
  auto manager = parse("Dictionary d { @Ea 30 kcal/mol; @MW 28 g/mol; }");
  const std::vector<DictionaryKeyWord> grammar = {
      kw("@Ea", SINGLE_MEASURE).SetDimension(dim::MolarEnergy),
      kw("@MW", SINGLE_MEASURE).SetDimension(dim::MolarMass)};
  attach(manager("d"), grammar);
  // Default: kilomole (OpenSMOKE++ convention).
  CHECK(manager("d").unit_system() == si_kmol);
  CHECK(close(manager("d").Read<double>("@Ea"), 1.2552e8)); // J/kmol
  CHECK(close(manager("d").Read<double>("@MW"), 28.0)); // kg/kmol

  // Strict SI on request.
  manager.SetUnitSystem(si);
  CHECK(close(manager("d").Read<double>("@Ea"), 125520.0)); // J/mol
  CHECK(close(manager("d").Read<double>("@MW"), 0.028)); // kg/mol

  // Dictionaries loaded after the call inherit the system.
  manager.ReadDictionariesFromString("Dictionary e { @MW 2 g/mol; }", "e.dic");
  CHECK(manager("e").unit_system() == si);
}

TEST_CASE("units", "kilomole is the default amount unit") {
  CHECK(UnitSystem{} == si_kmol);
  CHECK(close(to_si(1.0, "J/mol", dim::MolarEnergy), 1000.0));
  CHECK(close(to_si(1.0, "kmol/m3", dim::Concentration), 1.0));
  CHECK(close(to_si(28.0, "g/mol", dim::MolarMass), 28.0));
  CHECK(describe(dim::MolarMass) == "molar mass [kg/kmol]");
  CHECK(si_unit(dim::MolarMass, si) == "kg/mol");
  Dictionary dictionary;
  CHECK(dictionary.unit_system() == si_kmol);
}

TEST_CASE("units", "conversion factors follow their exact definitions") {
  // Mechanical units from their definitions (NIST SP 811, exact values).
  const double g0 = 9.80665;                  // standard gravity, m/s2
  const double lbf = 0.45359237 * g0;         // pound-force, N
  CHECK(close(factor("psi", dim::Pressure), lbf / (0.0254 * 0.0254)));
  CHECK(close(factor("oz", dim::Mass), factor("lb", dim::Mass) / 16.0));
  CHECK(close(factor("ft", dim::Length), 12.0 * factor("in", dim::Length)));
  CHECK(close(factor("torr", dim::Pressure), factor("atm", dim::Pressure) / 760.0));
  CHECK(close(factor("mmHg", dim::Pressure), 13595.1 * g0 * 1e-3));
  CHECK(close(factor("kWh", dim::Energy), 1000.0 * 3600.0));
  CHECK(close(factor("kcal", dim::Energy), 4184.0)); // thermochemical
  CHECK(close(factor("BTU", dim::Energy), 1055.05585262)); // International Table
  CHECK(close(factor("eV", dim::Energy), 1.602176634e-19)); // exact since 2019
  CHECK(close(factor("dyn", dim::Force), factor("g", dim::Mass) * 0.01));
  CHECK(close(factor("erg", dim::Energy), factor("dyn", dim::Force) * 0.01));
  CHECK(close(factor("rpm", dim::Frequency), 2.0 * 3.14159265358979323846 / 60.0));

  // Temperature scales: fixed points.
  CHECK(close(to_si(-40.0, "C", dim::Temperature), to_si(-40.0, "F", dim::Temperature)));
  CHECK(close(to_si(0.0, "R", dim::Temperature), 0.0));
  CHECK(close(to_si(-459.67, "F", dim::Temperature), 0.0));
  CHECK(close(to_si(-273.15, "C", dim::Temperature), 0.0));
  CHECK(close(to_si(1.8, "R", dim::Temperature), 1.0));

  // Gas constant in different units, kilomole system.
  const double R = 8314.462618;               // J/kmol/K
  CHECK(close(to_si(8.314462618, "J/mol/K", dim::MolarHeatCapacity), R));
  CHECK(close(to_si(8.314462618 / 4.184, "cal/mol/K", dim::MolarHeatCapacity), R));
  CHECK(close(to_si(8.314462618 / 101325.0 * 1e6, "atm.cm3/mol/K",
                    dim::MolarHeatCapacity), R));
  CHECK(close(to_si(0.08314462618, "bar.L/mol/K", dim::MolarHeatCapacity), R));
  CHECK(close(to_si(8.314462618e7, "erg/mol/K", dim::MolarHeatCapacity), R));
  CHECK(close(to_si(8.314462618, "kJ/kmol/K", dim::MolarHeatCapacity), R));

  // Round trips.
  for (const char *unit : {"cm/s", "km/h", "ft/min", "in/s"})
    CHECK(close(from_si(to_si(3.7, unit, dim::Velocity), unit, dim::Velocity), 3.7));
  for (const char *unit : {"C", "F", "R", "K"})
    CHECK(close(from_si(to_si(321.5, unit, dim::Temperature), unit,
                        dim::Temperature), 321.5));
}

TEST_CASE("units", "exact factors: one rounding, as hand-written conversions") {
  // Exact representation of the factor.
  const UnitConversion gcm3 = parse_unit("g/cm3");
  CHECK(gcm3.numerator == 1.0 && gcm3.denominator == 1.0 &&
        gcm3.exponent10 == 3);
  CHECK(gcm3.factor == 1000.0);
  CHECK(parse_unit("ms").exponent10 == -3);
  CHECK(parse_unit("kcal").factor == 4184.0);
  const UnitConversion kmh = parse_unit("km/h"); // 10^3 / 3600
  CHECK(kmh.numerator == 1.0 && kmh.denominator == 36.0 &&
        kmh.exponent10 == 1);
  const UnitConversion torr = parse_unit("torr"); // 101325 / 760
  CHECK(torr.numerator == 20265.0 && torr.denominator == 152.0 &&
        torr.exponent10 == 0);

  // Values are rounded once: the same doubles as value * 10^n or value / 10^n
  // written with exact powers of ten, for every value.
  const double values[] = {0.7, 7.1, 0.021, 0.019, 0.009, 1.1, 2.3e-5,
                           300.15, 1.0 / 3.0, 9.87654321e7};
  for (const double v : values) {
    CHECK(to_si(v, "g/cm3", dim::Density) == v * 1e3);
    CHECK(to_si(v, "cm3", dim::Volume) == v / 1e6);
    CHECK(to_si(v, "mm3", dim::Volume) == v / 1e9);
    CHECK(to_si(v, "cm", dim::Length) == v / 100.0);
    CHECK(to_si(v, "ms", dim::Time) == v / 1000.0);
    CHECK(to_si(v, "min", dim::Time) == v * 60.0);
    CHECK(to_si(v, "h", dim::Time) == v * 3600.0);
    CHECK(to_si(v, "kg/m2/min", dim::MassFlux) == v / 60.0);
    CHECK(to_si(v, "bar", dim::Pressure) == v * 1e5);
    CHECK(to_si(v, "atm", dim::Pressure) == v * 101325.0);
    CHECK(to_si(v, "C", dim::Temperature) == v + 273.15);
    CHECK(to_si(v, "mol/cm3", dim::Concentration, si_kmol) == v * 1e3);
    CHECK(to_si(v, "mol/s", dim::MolarFlowRate, si_kmol) == v / 1e3);
    CHECK(to_si(v, "kcal/mol", dim::MolarEnergy, si_kmol) == v * 4.184e6);
    CHECK(to_si(v, "cal/mol", dim::MolarEnergy, si_kmol) == v * 4184.0);
    CHECK(from_si(v, "cm3", dim::Volume) == v * 1e6);
    CHECK(from_si(v, "ms", dim::Time) == v * 1000.0);
  }
  CHECK(to_si(0.7, "g/cm3", dim::Density) == 700.0);
  CHECK(to_si(7.1, "cm3", dim::Volume) == 7.1 / 1e6);

  // Special values pass through.
  CHECK(to_si(0.0, "cm3", dim::Volume) == 0.0);
  CHECK(std::isinf(to_si(HUGE_VAL, "cm3", dim::Volume)));
  CHECK(std::isnan(to_si(std::nan(""), "cm3", dim::Volume)));

  // Fractional powers stay correct.
  CHECK(close(parse_unit("sqrt(cm)").factor, 0.1));
  CHECK(close(parse_unit("cm^1.5").factor, 1e-3));
  CHECK(close(to_si(1.0, "mm^(1/3)", dim::Length.pow(Rational::make(1, 3))),
              0.1));
}
