#include "carbon/BitStorage.hpp"

#include <cstdint>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>

int main() {
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream in(line);
        char kind; int bits, size, n;
        in >> kind >> bits >> size >> n;
        if (kind == 'C') {
            try {
                carbon::SimpleBitStorage storage(bits, size, std::vector<std::uint64_t>(static_cast<std::size_t>(std::max(n, 0))));
                std::cout << "ctorok\n";
            } catch (const std::exception&) { std::cout << "ctorerr\n"; }
            continue;
        }
        if (kind == 'Z') {
            carbon::ZeroBitStorage storage(size);
            std::vector<std::int32_t> out(static_cast<std::size_t>(size));
            storage.unpack(out);
            std::cout << "init=";
            storage.getAll([&](std::int32_t v) { std::cout << v << ','; });
            std::string op;
            while (in >> op) {
                int index, value; in >> index >> value;
                try {
                    if (op == "s") storage.set(index, value);
                    else if (op == "a") std::cout << "old=" << storage.getAndSet(index, value) << ';';
                    else if (op == "g") std::cout << "get=" << storage.get(index) << ';';
                    else if (op == "f") storage.fill(value);
                } catch (const std::exception&) { std::cout << "err;"; }
            }
            std::cout << " vals=";
            for (auto v : out) std::cout << v << ',';
            std::cout << " raw=" << storage.getRaw().size() << '\n';
            continue;
        }
        std::vector<std::int32_t> values;
        for (int i = 0, v; i < n; ++i) { in >> v; values.push_back(v); }
        carbon::SimpleBitStorage storage(bits, size, values);
        std::cout << "init=";
        storage.getAll([&](std::int32_t v) { std::cout << v << ','; });
        std::string op;
        while (in >> op) {
            int index, value; in >> index >> value;
            try {
                if (op == "s") storage.set(index, value);
                else if (op == "a") std::cout << "old=" << storage.getAndSet(index, value) << ';';
                else if (op == "g") std::cout << "get=" << storage.get(index) << ';';
                else if (op == "f") storage.fill(value);
                else return 2;
            } catch (const std::exception&) { std::cout << "err;"; }
        }
        std::vector<std::int32_t> unpacked(static_cast<std::size_t>(size));
        storage.unpack(unpacked);
        std::cout << " vals=";
        for (auto v : unpacked) std::cout << v << ',';
        std::cout << " raw=";
        for (auto v : storage.getRaw()) std::cout << v << ',';
        auto cloned = storage.copy();
        if (cloned->getRaw().size()) cloned->getRaw()[0] ^= 1;
        std::cout << " copy=" << ((storage.getRaw().empty() || cloned->getRaw()[0] != storage.getRaw()[0]) ? "true" : "false") << '\n';
    }
}
