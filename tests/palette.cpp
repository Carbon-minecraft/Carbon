#include "carbon/Palette.hpp"
#include <cassert>
#include <memory>
#include <string>
#include <vector>

using Value = std::shared_ptr<std::string>;

class Registry final : public carbon::IdMap<Value> {
public:
    std::vector<Value> entries;
    int getId(const Value& v) const override { for (int i=0;i<(int)entries.size();++i) if(entries[i].get()==v.get()) return i; return -1; }
    Value byId(int i) const override { return i>=0 && i<(int)entries.size()?entries[i]:Value{}; }
    int size() const override { return (int)entries.size(); }
};
class Resize final : public carbon::PaletteResize<Value> {
public:
    int bits=-1; Value value; int result=77;
    int onResize(int b, const Value& v) override { bits=b; value=v; return result; }
};
class NoResize final : public carbon::PaletteResize<Value> {
public: int onResize(int, const Value&) override { assert(false); return -1; }
};

int main() {
    auto a=std::make_shared<std::string>("same"), b=std::make_shared<std::string>("same"), c=std::make_shared<std::string>("third");
    Registry registry; registry.entries={a,b,c};
    Resize resize; NoResize none;

    carbon::SingleValuePalette<Value> single({a});
    assert(single.idFor(a,none)==0 && single.valueFor(0).get()==a.get());
    assert(single.idFor(b,resize)==77 && resize.bits==1 && resize.value.get()==b.get());
    auto singleCopy=single.copy(); assert(singleCopy->valueFor(0).get()==a.get());

    carbon::LinearPalette<Value> linear(1,{a,b});
    assert(linear.idFor(a,none)==0); // object identity, not string value equality
    assert(linear.idFor(b,none)==1);
    assert(linear.idFor(c,resize)==77 && resize.bits==2 && linear.getSize()==2);
    assert(linear.maybeHas([](const Value& v){return *v=="same";}));
    bool missing=false; try { (void)linear.valueFor(2); } catch(const carbon::MissingPaletteEntryException&) { missing=true; } assert(missing);

    carbon::HashMapPalette<Value> hash(1,{a,b});
    assert(hash.idFor(a,none)==0 && hash.idFor(b,none)==1);
    assert(hash.idFor(c,resize)==77 && resize.bits==2 && hash.getSize()==3); // Java adds before resize callback
    auto hashCopy=hash.copy(); assert(hashCopy->valueFor(2).get()==c.get());

    carbon::ByteWriter writer; linear.write(writer,registry);
    assert((int)writer.bytes().size()==linear.getSerializedSize(registry));
    carbon::ByteReader reader(writer.bytes()); carbon::LinearPalette<Value> decoded(1);
    decoded.read(reader,registry); assert(decoded.getSize()==2 && decoded.valueFor(0).get()==a.get() && decoded.valueFor(1).get()==b.get());
    carbon::ByteWriter hashWriter; hash.write(hashWriter,registry);
    carbon::ByteReader hashReader(hashWriter.bytes()); carbon::HashMapPalette<Value> hashDecoded(2);
    hashDecoded.read(hashReader,registry); assert(hashDecoded.getSize()==3 && hashDecoded.valueFor(2).get()==c.get());

    carbon::GlobalPalette<Value> global(registry);
    assert(global.idFor(c,none)==2 && global.idFor(std::make_shared<std::string>("unknown"),none)==0);
    assert(global.valueFor(1).get()==b.get() && global.maybeHas([](const Value&){return false;}));
    carbon::ByteWriter empty; global.write(empty,registry); assert(empty.bytes().empty() && global.getSerializedSize(registry)==0);
    carbon::ByteReader noBytes(empty.bytes()); global.read(noBytes,registry);
    return 0;
}
