#include "carbon/Nbt.hpp"
#include <cassert>
#include <cmath>
#include <limits>

using namespace carbon::nbt;

int main() {
    assert(Tag::byteTag(-1).id() == TagId::Byte);
    assert(NumericTag{std::int16_t{255}}.byteValue() == -1);
    assert(NumericTag{std::int32_t{65535}}.shortValue() == -1);
    assert(NumericTag{std::int64_t{0x100000001LL}}.intValue() == 1);

    // Java FloatTag/DoubleTag intValue use Mth.floor, while longValue casts.
    assert(NumericTag{-1.2f}.intValue() == -2);
    assert(NumericTag{-1.2f}.longValue() == -1);
    assert(NumericTag{-1.2}.intValue() == -2);
    assert(NumericTag{-1.2}.longValue() == -2);
    assert(NumericTag{1.0e12}.byteValue() == -1);
    assert(NumericTag{1.0e12}.shortValue() == -1);
    assert(NumericTag{std::numeric_limits<double>::quiet_NaN()}.intValue() == 0);
    assert(NumericTag{std::numeric_limits<double>::infinity()}.intValue() == std::numeric_limits<std::int32_t>::max());
    assert(NumericTag{-std::numeric_limits<double>::infinity()}.longValue() == std::numeric_limits<std::int64_t>::min());

    ListTag list;
    list.add(Tag::intTag(3));
    list.add(Tag::string(u"wide \u03bb"));
    assert(list.size() == 2 && list.rawElementType() == TagId::Compound);
    auto listCopy = Tag(list).copy();
    assert(listCopy == Tag(list));

    CompoundTag compound;
    compound.putByte(u"flag", 2);
    compound.putDouble(u"value", -3.75);
    compound.put(u"text", Tag::string(u"\U0001F600"));
    compound.put(u"bytes", Tag(ByteArray{1, -2}));
    assert(compound.getBoolean(u"flag") == true);
    assert(compound.getInt(u"value") == -4);
    assert(!compound.getInt(u"absent"));
    assert(compound.getString(u"text") == Utf16String(u"\U0001F600"));

    Tag compoundValue(compound);
    auto deepCopy = compoundValue.copy();
    assert(deepCopy == compoundValue);
    assert(deepCopy.compound() != compoundValue.compound());
    auto* copiedBytes = deepCopy.compound()->get(u"bytes")->byteArray();
    (*copiedBytes)[0] = 9;
    assert((*compoundValue.compound()->get(u"bytes")->byteArray())[0] == 1);

    bool threw = false;
    try { list.insert(4, Tag::intTag(1)); } catch (const std::out_of_range&) { threw = true; }
    assert(threw);
    return 0;
}
