#include <iostream>

// The following include directive is disjointed to force accurate parsing even when someone writes comments inside the directive.
// Added quotes and brackets for more complete parsing.
/* comment */ #include /* comment "quotes" <brackets> */ "header.hpp" /* comment */

// Space in the header below to make parsing more complete.
#include "touch header.hpp"

int main() {
	std::cout << get_string() << std::endl;
}
