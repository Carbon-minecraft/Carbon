#include "carbon/StateProperties.hpp"
#include <cassert>
#include <iostream>

using namespace carbon;
enum class Shape { A, B, C };
int main() {
    auto flag = BooleanProperty::create("enabled");
    assert(flag->possibleValues() == std::vector<bool>({true,false}));
    assert(flag->getValue("true") == true && !flag->getValue("TRUE"));
    assert(flag->getInternalIndex(false) == 1);
    auto level = IntegerProperty::create("level", 2, 4);
    assert(level->possibleValues() == std::vector<int>({2,3,4}));
    assert(!level->getValue("1") && !level->getValue("5") && level->getValue("+2") == 2);
    assert(level->getValue("3") == 3 && level->getInternalIndex(1) == -1);
    bool threw=false; try { (void)IntegerProperty::create("bad", 0, 0); } catch(const std::invalid_argument&) { threw=true; } assert(threw);
    auto shape=EnumProperty<Shape>::createFiltered("shape", {Shape::A,Shape::B,Shape::C}, [](Shape s){return s!=Shape::B;}, [](Shape s){return s==Shape::A?"a":s==Shape::B?"b":"c";});
    assert(shape->possibleValues()==std::vector<Shape>({Shape::A,Shape::C}));
    assert(shape->getInternalIndex(Shape::B)==-1 && shape->getValue("c")==Shape::C);
    using Def=StateDefinition<int>;
    auto definition=Def::Builder(17).add(level, flag, shape).create();
    assert(definition.getPossibleStates().size()==3*2*2);
    assert(definition.getPossibleStates().front()->owner()==17);
    // Java StateDefinition sorts property names before generating their Cartesian product.
    auto props=definition.getPossibleStates().front()->getProperties();
    assert(props[0]->name()=="enabled" && props[1]->name()=="level" && props[2]->name()=="shape");
    auto initial=definition.any();
    assert(initial->getValue(flag)==true && initial->getValue(level)==2 && initial->getValue(shape)==Shape::A);
    auto next=initial->setValue(level,4);
    assert(next->getValue(level)==4 && next->getValue(flag));
    assert(next->setValue(level,2).get()==initial.get()); // cached state identity
    assert(initial->cycle(flag)->getValue(flag)==false);
    assert(initial->cycle(shape)->getValue(shape)==Shape::C);
    assert(initial->trySetValue(BooleanProperty::create("other"),false).get()==initial.get());
    threw=false; try { (void)initial->setValue(level,9); } catch(const std::invalid_argument&) { threw=true; } assert(threw);
    std::cout << "PASS: native state-property assertions; not a JVM differential test\n";
}
