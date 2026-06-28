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

#include "Dictionary.h"
#include "DictionaryGrammar.h"
#include "DictionaryKeyWord.h"
#include "DictionaryManager.h"

#include <cmath>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
	class ThreeEquationsGrammar final : public OpenSMOKEpp::DictionaryGrammar
	{
	protected:
		void DefineRules() override
		{
			using OpenSMOKEpp::DictionaryKeyWord;

			AddKeyWord(DictionaryKeyWord("@ThreeEquations", OpenSMOKEpp::SINGLE_BOOL,
				"Three equations model: on/off (default: true)", true));
			AddKeyWord(DictionaryKeyWord("@NucleationModel", OpenSMOKEpp::SINGLE_INT,
				"Nucleation model: 0=off, 1=on (default: 1)", false));
			AddKeyWord(DictionaryKeyWord("@SurfaceGrowthModel", OpenSMOKEpp::SINGLE_INT,
				"Surface growth model: 0=off, 1=on (default: 1)", false));
			AddKeyWord(DictionaryKeyWord("@OxidationModel", OpenSMOKEpp::SINGLE_INT,
				"Oxidation model: 0=off, 1=on (default: 1)", false));
			AddKeyWord(DictionaryKeyWord("@CondensationModel", OpenSMOKEpp::SINGLE_INT,
				"Condensation model: 0=off, 1=on (default: 1)", false));
			AddKeyWord(DictionaryKeyWord("@CoagulationModel", OpenSMOKEpp::SINGLE_INT,
				"Coagulation model: 0=off, 1=on (default: 1)", false));
			AddKeyWord(DictionaryKeyWord("@ThermophoreticModel", OpenSMOKEpp::SINGLE_INT,
				"Thermophoretic model: 0=off, 1=on (default: 1)", false));
			AddKeyWord(DictionaryKeyWord("@SurfaceChemistryModel", OpenSMOKEpp::SINGLE_STRING,
				"Surface chemistry model (RC-PAH (default) | HMOM)", false));
			AddKeyWord(DictionaryKeyWord("@PAH", OpenSMOKEpp::SINGLE_STRING,
				"Species to be assumed as PAH", true));
			AddKeyWord(DictionaryKeyWord("@GasClosureDummySpecies", OpenSMOKEpp::SINGLE_STRING,
				"Species to be assumed as gaseous dummy species", true));
			AddKeyWord(DictionaryKeyWord("@GasConsumption", OpenSMOKEpp::SINGLE_BOOL,
				"Consumption of gaseous species is accounted for (default: true)", true));
			AddKeyWord(DictionaryKeyWord("@RadiativeHeatTransfer", OpenSMOKEpp::SINGLE_BOOL,
				"Radiative heat transfer (default: true)", false));
			AddKeyWord(DictionaryKeyWord("@PlanckCoefficient", OpenSMOKEpp::SINGLE_STRING,
				"Calculation of Planck Coefficient: Smooke | Kent | Sazhin | none", false));
			AddKeyWord(DictionaryKeyWord("@SchmidtNumber", OpenSMOKEpp::SINGLE_DOUBLE,
				"Schmidt number (default: 50)", false));
			AddKeyWord(DictionaryKeyWord("@SootDensity", OpenSMOKEpp::SINGLE_MEASURE,
				"Density of soot particles (default: 1800 kg/m3)", false));
			AddKeyWord(DictionaryKeyWord("@SimplifiedPAHMass", OpenSMOKEpp::SINGLE_BOOL,
				"Simplified calculation of PAH mass", false));
			AddKeyWord(DictionaryKeyWord("@epsNucleation", OpenSMOKEpp::SINGLE_DOUBLE,
				"Collision enhancement factor for nucleation", false));
			AddKeyWord(DictionaryKeyWord("@epsCondensation", OpenSMOKEpp::SINGLE_DOUBLE,
				"Collision enhancement factor for condensation", false));
			AddKeyWord(DictionaryKeyWord("@epsCoagulation", OpenSMOKEpp::SINGLE_DOUBLE,
				"Collision enhancement factor for coagulation", false));
			AddKeyWord(DictionaryKeyWord("@CorrectionCoefficientPAHPAH", OpenSMOKEpp::SINGLE_DOUBLE,
				"Correction coefficient for PAH-PAH nucleation kernel", false));
			AddKeyWord(DictionaryKeyWord("@MinimumNs", OpenSMOKEpp::SINGLE_MEASURE,
				"Minimum Ns for the calculation of soot properties", false));
			AddKeyWord(DictionaryKeyWord("@DimerModel", OpenSMOKEpp::SINGLE_STRING,
				"Dimer concentration model (qssa-rodrigues)", false));
			AddKeyWord(DictionaryKeyWord("@StickingCoefficientModel", OpenSMOKEpp::SINGLE_STRING,
				"Sticking coefficient model (constant | pah-dependent)", true));
			AddKeyWord(DictionaryKeyWord("@StickingCoefficientConstant", OpenSMOKEpp::SINGLE_DOUBLE,
				"Sticking coefficient constant", true));
			AddKeyWord(DictionaryKeyWord("@DebugMode", OpenSMOKEpp::SINGLE_BOOL,
				"Debug mode (default: false)", false));
		}
	};

	auto require_true(const bool condition, const std::string& message) -> void
	{
		if (!condition)
			throw std::runtime_error(message);
	}

	auto require_close(const double actual, const double expected, const std::string& name) -> void
	{
		const double tolerance = 1.e-12 * std::max(1.0, std::abs(expected));
		if (std::abs(actual - expected) > tolerance)
			throw std::runtime_error(name + " has an unexpected value");
	}

	auto exercise_three_equations_dictionary(OpenSMOKEpp::Dictionary& dict) -> void
	{
		ThreeEquationsGrammar grammar;
		dict.SetGrammar(grammar);

		bool flag = false;
		int integer_value = 0;
		double double_value = 0.0;
		std::string text;
		std::string units;

		require_true(dict.CheckOption("@ThreeEquations"), "Missing @ThreeEquations");
		dict.ReadBool("@ThreeEquations", flag);
		require_true(flag, "@ThreeEquations should be true");

		dict.ReadInt("@NucleationModel", integer_value);
		require_true(integer_value == 1, "@NucleationModel should be 1");
		dict.ReadInt("@SurfaceGrowthModel", integer_value);
		require_true(integer_value == 1, "@SurfaceGrowthModel should be 1");
		dict.ReadInt("@OxidationModel", integer_value);
		require_true(integer_value == 1, "@OxidationModel should be 1");
		dict.ReadInt("@CondensationModel", integer_value);
		require_true(integer_value == 1, "@CondensationModel should be 1");
		dict.ReadInt("@CoagulationModel", integer_value);
		require_true(integer_value == 1, "@CoagulationModel should be 1");
		dict.ReadInt("@ThermophoreticModel", integer_value);
		require_true(integer_value == 0, "@ThermophoreticModel should be 0");

		dict.ReadString("@DimerModel", text);
		require_true(text == "qssa-rodrigues", "@DimerModel should be qssa-rodrigues");
		dict.ReadString("@SurfaceChemistryModel", text);
		require_true(text == "HMOM", "@SurfaceChemistryModel should be HMOM");
		dict.ReadString("@PAH", text);
		require_true(text == "A2", "@PAH should be A2");
		dict.ReadString("@GasClosureDummySpecies", text);
		require_true(text == "CSOOT", "@GasClosureDummySpecies should be CSOOT");

		dict.ReadMeasure("@SootDensity", double_value, units);
		require_close(double_value, 1800.0, "@SootDensity");
		require_true(units == "kg/m3", "@SootDensity units should be kg/m3");

		dict.ReadString("@StickingCoefficientModel", text);
		require_true(text == "constant", "@StickingCoefficientModel should be constant");
		dict.ReadDouble("@StickingCoefficientConstant", double_value);
		require_close(double_value, 2.e-3, "@StickingCoefficientConstant");

		dict.ReadBool("@SimplifiedPAHMass", flag);
		require_true(flag, "@SimplifiedPAHMass should be true");
		dict.ReadBool("@GasConsumption", flag);
		require_true(!flag, "@GasConsumption should be false");
		dict.ReadBool("@RadiativeHeatTransfer", flag);
		require_true(flag, "@RadiativeHeatTransfer should be true");
		dict.ReadString("@PlanckCoefficient", text);
		require_true(text == "Smooke", "@PlanckCoefficient should be Smooke");
		dict.ReadDouble("@SchmidtNumber", double_value);
		require_close(double_value, 50.0, "@SchmidtNumber");

		dict.ReadDouble("@epsNucleation", double_value);
		require_close(double_value, 2.2, "@epsNucleation");
		dict.ReadDouble("@epsCondensation", double_value);
		require_close(double_value, 1.3, "@epsCondensation");
		dict.ReadDouble("@epsCoagulation", double_value);
		require_close(double_value, 2.2, "@epsCoagulation");

		dict.ReadBool("@DebugMode", flag);
		require_true(!flag, "@DebugMode should be false");
	}
}

auto main(const int argc, char* argv[]) -> int
{
	if (argc != 2)
	{
		std::cerr << "Usage: test_three_equations_grammar <dictionary-input-file>\n";
		return 1;
	}

	try
	{
		OpenSMOKEpp::DictionaryManager manager;
		manager.ReadDictionariesFromFile(argv[1]);
		auto& dictionary = manager("ThreeEquations");
		exercise_three_equations_dictionary(dictionary);
		return 0;
	}
	catch (const std::exception& exception)
	{
		std::cerr << "ThreeEquations grammar test failed for '" << argv[1] << "': "
		          << exception.what() << '\n';
		return 1;
	}
	catch (...)
	{
		std::cerr << "ThreeEquations grammar test failed for '" << argv[1]
		          << "': unknown non-standard exception\n";
		return 1;
	}
}
