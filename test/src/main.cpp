#include <iostream>

// The following include directive is disjointed to force accurate parsing even when someone writes comments inside the directive.
// Added quotes and brackets for extra confusion.
/* comment */ #include /* comment "quotes" <brackets> */ "header.hpp" /* comment */

int main() {
	// std::cout << header_variable << std::endl;
}
