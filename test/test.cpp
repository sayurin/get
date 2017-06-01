#include "../get.h"
#include <type_traits>	// std::is_same_v

void intp(int*) {}
void longp(int, long*) {}
struct st {
	void intp(int*) {}
	void longp(int, long*) {}
};

int main() {
	auto a = sayuri::get(intp); static_assert(std::is_same_v<decltype(a), int>, "");
	auto b = sayuri::get(longp, 0); static_assert(std::is_same_v<decltype(b), long>, "");
	st s; 
	auto c = GET(&s, intp); static_assert(std::is_same_v<decltype(c), int>, "");
	auto d = GET(&s, longp, 0); static_assert(std::is_same_v<decltype(d), long>, "");
	return 0;
}
