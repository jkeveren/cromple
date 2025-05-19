#include <iostream>

// The following include directive is disjointed to test accurate parsing even when someone writes comments inside the directive..
/* comment */ #include /* comment "quotes" <brackets> */ "header.hpp" /* comment */

// Space in the header below to test parsing accuracy.
#include "touch header.hpp"

int main() {
	std::cout << get_string() << std::endl;
}
