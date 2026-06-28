#include "DictionaryManager.h"

#include <exception>
#include <iostream>

auto main(const int argc, char* argv[]) -> int
{
	if (argc != 2)
	{
		std::cerr << "Usage: test_parser_smoke <dictionary-input-file>\n";
		return 1;
	}

	try
	{
		OpenSMOKEpp::DictionaryManager manager;
		manager.ReadDictionariesFromFile(argv[1]);
		return 0;
	}
	catch (const std::exception& exception)
	{
		std::cerr << "Parser smoke test failed for '" << argv[1] << "': "
		          << exception.what() << '\n';
		return 1;
	}
	catch (...)
	{
		std::cerr << "Parser smoke test failed for '" << argv[1]
		          << "': unknown non-standard exception\n";
		return 1;
	}
}
