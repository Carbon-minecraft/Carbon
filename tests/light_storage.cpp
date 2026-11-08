#include "carbon/DataLayer.hpp"
#include "carbon/LevelHeightAccessor.hpp"

#include <iostream>
#include <sstream>
#include <string>

int main() {
    std::cout << std::boolalpha;
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream in(line);
        char kind{};
        in >> kind;
        if (kind == 'D') {
            int def; in >> def;
            carbon::DataLayer layer(def);
            std::string op;
            while (in >> op) {
                if (op == "g") { int x,y,z; in>>x>>y>>z; std::cout << "g=" << layer.get(x,y,z) << ';'; }
                else if (op == "s") { int x,y,z,v; in>>x>>y>>z>>v; try { layer.set(x,y,z,v); std::cout << "s=" << layer.get(x,y,z) << ';'; } catch (...) { std::cout << "err;"; } }
                else if (op == "f") { int v; in>>v; layer.fill(v); std::cout << "f=" << layer.defaultValue() << ',' << layer.isDefinitelyHomogenous() << ',' << layer.isEmpty() << ';'; }
                else if (op == "m") { auto& data=layer.getData(); std::cout << "m=" << static_cast<int>(data.front()) << ',' << layer.isDefinitelyHomogenous() << ';'; }
                else if (op == "q") { int v; in>>v; std::cout << "q=" << layer.isDefinitelyHomogenous() << ',' << layer.isDefinitelyFilledWith(v) << ',' << layer.isEmpty() << ';'; }
                else if (op == "c") {
                    auto copy=layer.copy();
                    const auto homogeneous=copy.isDefinitelyHomogenous();
                    const auto original=layer.get(0,0,0);
                    copy.set(0,0,0,12);
                    std::cout << "c=" << homogeneous << ',' << copy.get(0,0,0) << ',' << layer.get(0,0,0) << ',' << original << ';';
                }
            }
            std::cout << '\n';
        } else if (kind == 'H') {
            int minY,height; in>>minY>>height;
            auto h=carbon::LevelHeightAccessor::create(minY,height);
            std::cout << "h=" << h.getMaxY() << ',' << h.getMinSectionY() << ',' << h.getMaxSectionY() << ',' << h.getSectionsCount() << ';';
            int y;
            while(in>>y) {
                const carbon::BlockPos pos(0, y, 0);
                std::cout << "y=" << h.isInsideBuildHeight(y) << ',' << h.isOutsideBuildHeight(y) << ',' << h.getSectionIndex(y)
                          << ",p=" << h.isInsideBuildHeight(pos) << ',' << h.isOutsideBuildHeight(pos) << ';';
            }
            std::cout << '\n';
        }
    }
}
