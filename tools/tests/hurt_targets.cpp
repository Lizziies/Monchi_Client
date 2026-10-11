#ifdef NDEBUG
#undef NDEBUG
#endif
#include "flarial/Bridge/HurtTargets.hpp"
#include <cassert>
#include <string_view>
int main() {
    assert(hurtTargets::action(true, true, false, true) == hurtTargets::Action::Hide);
    assert(hurtTargets::action(true, false, true, false) == hurtTargets::Action::Hide);
    assert(hurtTargets::action(true, true, true, false) == hurtTargets::Action::Color);
    assert(hurtTargets::action(true, false, false, true) == hurtTargets::Action::Color);
    assert(hurtTargets::action(true, true, false, false) == hurtTargets::Action::Hide);
    assert(hurtTargets::action(true, false, false, false) == hurtTargets::Action::Hide);
    assert(hurtTargets::action(false, false, true, true) == hurtTargets::Action::Keep);
    assert(std::string_view(hurtTargets::key(true)) == "selfHurt");
    assert(std::string_view(hurtTargets::key(false)) == "hurt");
}
