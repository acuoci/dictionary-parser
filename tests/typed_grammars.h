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


// Typed grammars shared by the typed front-end tests and the compile-fail
// tests.

#ifndef OpenSMOKEpp_TestTypedGrammars_H
#define OpenSMOKEpp_TestTypedGrammars_H

#include "DictionaryTyped.h"

#include <string_view>

namespace gas {
using namespace OpenSMOKEpp::typed;
namespace dim = OpenSMOKEpp::units::dim;

inline constexpr auto T =
    Key<"@Temperature">(Quantity(dim::Temperature).Minimum(0.0, false), "Temperature");
inline constexpr auto P =
    Key<"@Pressure">(Quantity(dim::Pressure).Minimum(0.0, false), "Pressure");
inline constexpr auto Rho = Key<"@Density">(Quantity(dim::Density), "Density");
inline constexpr auto X =
    Key<"@MoleFractions">(Composition().Minimum(0.0).Sum(1.0, 1e-6), "Mole fractions");
inline constexpr auto Y =
    Key<"@MassFractions">(Composition().Minimum(0.0).Sum(1.0, 1e-6), "Mass fractions");

struct Status : Grammar<Status> {
  static constexpr std::string_view display_name = "gas status";
  static constexpr auto keys = Keys(T, P, Rho, X, Y);
  static constexpr auto constraints =
      Rules(Exactly(2, T, P, Rho), ExactlyOneOf(X, Y));
};
} // namespace gas

namespace reactor {
using namespace OpenSMOKEpp::typed;
namespace dim = OpenSMOKEpp::units::dim;

enum class Solver { OpenSMOKE, CVODE, DASPK };

inline constexpr auto Type =
    Key<"@Type">(Enumeration("Isothermal", "NonIsothermal"), "Reactor type")
        .Compulsory();
inline constexpr auto Volume = Key<"@Volume">(Quantity(dim::Volume), "Volume");
inline constexpr auto EndTime =
    Key<"@EndTime">(Quantity(dim::Time).Minimum(0.0, false), "End time").Compulsory();
inline constexpr auto Steps =
    Key<"@Steps">(Integer().Minimum(1), "Number of steps").Default("100");
inline constexpr auto OdeSolver =
    Key<"@OdeSolver">(Enumeration<Solver>("OpenSMOKE++", "CVODE", "DASPK")
                          .Alias("Cvode", "CVODE")
                          .Disable("DASPK", "not available in this build"),
                      "ODE solver")
        .Default("OpenSMOKE++");
inline constexpr auto Verbose = Key<"@Verbose">(Boolean(), "Verbose output").Default("false");
inline constexpr auto InitialStatus =
    Key<"@InitialStatus">(Reference<gas::Status>(), "Initial status").Compulsory();
inline constexpr auto Inlets = Key<"@Inlets">(ReferenceList<gas::Status>(), "Inlet streams");
inline constexpr auto Profile =
    Key<"@Profile">(Table(Column("time", dim::Time), Column("temperature", dim::Temperature)),
                    "Temperature profile");
inline constexpr auto Links =
    Key<"@Links">(List(Record(Field("from", Integer().Minimum(1)),
                              Field("to", Integer().Minimum(1)),
                              Field("flow", Quantity(dim::MassFlowRate)))),
                  "Connections");
inline constexpr auto Output = Key<"@Output">(Path(), "Output folder").Default("Output");
inline constexpr auto Species =
    Key<"@OutputSpecies">(Choice(Enumeration("ALL", "NONE"), List(Word())), "Output species");
inline constexpr auto OldName =
    Key<"@OldVolume">(Quantity(dim::Volume), "Volume (old name)").Deprecated("use @Volume");

struct Batch : Grammar<Batch> {
  static constexpr std::string_view display_name = "batch reactor";
  static constexpr auto keys =
      Keys(Type, Volume, EndTime, Steps, OdeSolver, Verbose, InitialStatus, Inlets,
           Profile, Links, Output, Species, OldName);
  static constexpr auto constraints =
      Rules(When(Equals(Type, "NonIsothermal"), Required(Profile)),
            Requires(Links, Inlets), Conflicts(OldName, Volume));
};
} // namespace reactor

// A grammar whose dictionaries reference each other (to test cycles).
namespace chain {
using namespace OpenSMOKEpp::typed;
struct Node;
inline constexpr auto Next = Key<"@Next">(Reference<Node>(), "Next node");
inline constexpr auto Value = Key<"@Value">(Integer(), "Value").Default("0");
struct Node : Grammar<Node> {
  static constexpr auto keys = Keys(Next, Value);
};
} // namespace chain

#endif // OpenSMOKEpp_TestTypedGrammars_H
