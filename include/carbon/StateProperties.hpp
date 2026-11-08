#pragma once

// Source-backed subset of Java 26.3 Property, BooleanProperty,
// IntegerProperty, EnumProperty, StateDefinition and StateHolder.
// Serialization codecs and Minecraft block/registry types are intentionally
// outside this value/state lattice.

#include <algorithm>
#include <any>
#include <charconv>
#include <cctype>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <typeindex>
#include <type_traits>
#include <utility>
#include <vector>

namespace carbon {

class PropertyBase {
public:
    virtual ~PropertyBase() = default;
    const std::string& name() const noexcept { return name_; }
    std::type_index valueType() const noexcept { return type_; }
    virtual std::size_t valueCount() const noexcept = 0;
    virtual std::any valueAtAny(std::size_t) const = 0;
    virtual std::string nameForAny(const std::any&) const = 0;
    virtual std::optional<std::any> parseAny(std::string_view) const = 0;
    virtual int indexAny(const std::any&) const = 0;
protected:
    PropertyBase(std::string name, std::type_index type) : name_(std::move(name)), type_(type) {}
private:
    std::string name_;
    std::type_index type_;
};

template<class T>
class Property : public PropertyBase {
public:
    using value_type = T;
    const std::vector<T>& possibleValues() const noexcept { return values_; }
    virtual std::string getName(const T& value) const = 0;
    virtual std::optional<T> getValue(std::string_view serializedName) const = 0;
    virtual int getInternalIndex(const T& value) const = 0;
    std::size_t valueCount() const noexcept final { return values_.size(); }
    std::any valueAtAny(std::size_t index) const final { return values_.at(index); }
    std::string nameForAny(const std::any& value) const final {
        try { return getName(std::any_cast<const T&>(value)); }
        catch (const std::bad_any_cast&) { throw std::invalid_argument("Property value has the wrong type"); }
    }
    std::optional<std::any> parseAny(std::string_view name) const final {
        auto value = getValue(name);
        if (!value) return std::nullopt;
        return std::any(std::move(*value));
    }
    int indexAny(const std::any& value) const final {
        try { return getInternalIndex(std::any_cast<const T&>(value)); }
        catch (const std::bad_any_cast&) { return -1; }
    }
protected:
    Property(std::string name, std::vector<T> values)
        : PropertyBase(std::move(name), typeid(T)), values_(std::move(values)) {}
    std::vector<T> values_;
};

class BooleanProperty final : public Property<bool> {
public:
    static std::shared_ptr<BooleanProperty> create(std::string name) {
        return std::shared_ptr<BooleanProperty>(new BooleanProperty(std::move(name)));
    }
    std::string getName(const bool& value) const override { return value ? "true" : "false"; }
    std::optional<bool> getValue(std::string_view name) const override {
        if (name == "true") return true;
        if (name == "false") return false;
        return std::nullopt;
    }
    int getInternalIndex(const bool& value) const override { return value ? 0 : 1; }
private:
    explicit BooleanProperty(std::string name) : Property<bool>(std::move(name), {true, false}) {}
};

class IntegerProperty final : public Property<int> {
public:
    static std::shared_ptr<IntegerProperty> create(std::string name, int min, int max) {
        return std::shared_ptr<IntegerProperty>(new IntegerProperty(std::move(name), min, max));
    }
    int minValue() const noexcept { return min_; }
    int maxValue() const noexcept { return max_; }
    std::string getName(const int& value) const override { return std::to_string(value); }
    std::optional<int> getValue(std::string_view name) const override {
        if (name.empty()) return std::nullopt;
        int value{};
        const char* first = name.data(); const char* last = first + name.size();
        // Java Integer.parseInt accepts a leading plus; std::from_chars does not.
        if (*first == '+') { if (++first == last) return std::nullopt; }
        auto result = std::from_chars(first, last, value);
        if (result.ec != std::errc{} || result.ptr != last || value < min_ || value > max_) return std::nullopt;
        return value;
    }
    int getInternalIndex(const int& value) const override { return value <= max_ ? value - min_ : -1; }
private:
    IntegerProperty(std::string name, int min, int max)
        : Property<int>(name, makeValues(name, min, max)), min_(min), max_(max) {
        // Validation happens before values are constructed in makeValues.
    }
    static std::vector<int> makeValues(const std::string& name, int min, int max) {
        if (min < 0) throw std::invalid_argument("Min value of " + name + " must be 0 or greater");
        if (max <= min) throw std::invalid_argument("Max value of " + name + " must be greater than min (" + std::to_string(min) + ")");
        std::vector<int> values;
        const auto count = static_cast<unsigned long long>(static_cast<long long>(max) - min) + 1ULL;
        if (count > values.max_size()) throw std::length_error("IntegerProperty range is too large");
        values.reserve(static_cast<std::size_t>(count));
        for (long long i = min; i <= static_cast<long long>(max); ++i) values.push_back(static_cast<int>(i));
        return values;
    }
    int min_, max_;
};

template<class E>
class EnumProperty final : public Property<E> {
    static_assert(std::is_enum_v<E>, "EnumProperty requires an enum type");
public:
    using Serializer = std::function<std::string(E)>;
    // enumOrder must be the enum constants in declaration order, matching
    // clazz.getEnumConstants(); values is the Java create(List) selection.
    static std::shared_ptr<EnumProperty> create(std::string name, std::vector<E> enumOrder,
                                                std::vector<E> values, Serializer serializer) {
        return std::shared_ptr<EnumProperty>(new EnumProperty(std::move(name), std::move(enumOrder), std::move(values), std::move(serializer)));
    }
    static std::shared_ptr<EnumProperty> create(std::string name, std::vector<E> enumOrder, Serializer serializer) {
        return create(std::move(name), enumOrder, enumOrder, std::move(serializer));
    }
    static std::shared_ptr<EnumProperty> createFiltered(std::string name, std::vector<E> enumOrder,
                                                         const std::function<bool(E)>& filter, Serializer serializer) {
        std::vector<E> accepted;
        for (E value : enumOrder) if (filter(value)) accepted.push_back(value);
        return create(std::move(name), std::move(enumOrder), std::move(accepted), std::move(serializer));
    }
    std::string getName(const E& value) const override { return serializer_(value); }
    std::optional<E> getValue(std::string_view name) const override {
        for (E value : this->values_) if (serializer_(value) == name) return value;
        return std::nullopt;
    }
    int getInternalIndex(const E& value) const override {
        for (std::size_t i=0; i<this->values_.size(); ++i) if (this->values_[i] == value) return static_cast<int>(i);
        return -1;
    }
private:
    EnumProperty(std::string name, std::vector<E> enumOrder, std::vector<E> values, Serializer serializer)
        : Property<E>(std::move(name), validateValues(enumOrder, std::move(values), serializer)), serializer_(std::move(serializer)), enumOrder_(std::move(enumOrder)) {
        for (std::size_t i=0; i<enumOrder_.size(); ++i) ordinalIndex_.push_back(this->getInternalIndex(enumOrder_[i]));
    }
    static std::vector<E> validateValues(const std::vector<E>& enumOrder, std::vector<E> values, const Serializer& serializer) {
        if (values.empty()) throw std::invalid_argument("Trying to make empty EnumProperty");
        if (!serializer) throw std::invalid_argument("EnumProperty requires serialized-name function");
        for (E value : values) {
            if (std::find(enumOrder.begin(), enumOrder.end(), value) == enumOrder.end()) throw std::invalid_argument("EnumProperty value is not an enum constant");
        }
        // ImmutableMap.Builder rejects duplicate serialized names.
        std::vector<std::string> names;
        for (E value : values) {
            auto n=serializer(value);
            if (std::find(names.begin(), names.end(), n)!=names.end()) throw std::invalid_argument("Duplicate enum serialized name: " + n);
            names.push_back(std::move(n));
        }
        return values;
    }
    Serializer serializer_;
    std::vector<E> enumOrder_;
    std::vector<int> ordinalIndex_;
};

template<class Owner> class StateDefinition;

template<class Owner>
class BlockState : public std::enable_shared_from_this<BlockState<Owner>> {
public:
    using Ptr = std::shared_ptr<BlockState>;
    const Owner& owner() const noexcept { return owner_; }
    bool isSingletonState() const noexcept { return properties_.empty(); }
    const std::vector<std::shared_ptr<PropertyBase>>& getProperties() const noexcept { return properties_; }
    bool hasProperty(const std::shared_ptr<PropertyBase>& property) const noexcept { return valueIndex(property.get()) >= 0; }
    template<class P, class T = typename P::value_type> T getValue(const std::shared_ptr<P>& property) const {
        int i=valueIndex(property.get());
        if (i<0) throw std::invalid_argument("Cannot get property " + property->name() + " as it does not exist in owner");
        return std::any_cast<T>(values_[i]);
    }
    template<class P, class T = typename P::value_type> std::optional<T> getOptionalValue(const std::shared_ptr<P>& property) const {
        int i=valueIndex(property.get()); if(i<0) return std::nullopt; return std::any_cast<T>(values_[i]);
    }
    template<class P, class T = typename P::value_type> T getValueOrElse(const std::shared_ptr<P>& property, T fallback) const {
        auto value=getOptionalValue(property); return value ? *value : std::move(fallback);
    }
    template<class P, class T = typename P::value_type> Ptr setValue(const std::shared_ptr<P>& property, const T& value) const {
        int i=valueIndex(property.get());
        if(i<0) throw std::invalid_argument("Cannot set property " + property->name() + " as it does not exist in owner");
        int vi=property->getInternalIndex(value);
        if(vi<0) throw std::invalid_argument("Cannot set property " + property->name() + " to a value not allowed");
        return neighbors_[i][static_cast<std::size_t>(vi)];
    }
    template<class P, class T = typename P::value_type> Ptr trySetValue(const std::shared_ptr<P>& property, const T& value) const {
        if(valueIndex(property.get())<0) return std::const_pointer_cast<BlockState>(this->shared_from_this());
        return setValue(property,value);
    }
    template<class P, class T = typename P::value_type> Ptr cycle(const std::shared_ptr<P>& property) const {
        const auto& vals=property->possibleValues();
        const T current=getValue(property);
        auto it=std::find(vals.begin(),vals.end(),current);
        std::size_t next=static_cast<std::size_t>(it-vals.begin()+1);
        if(next==vals.size()) next=0;
        return setValue(property,vals[next]);
    }
    template<class P, class T = typename P::value_type> std::string serializedValue(const std::shared_ptr<P>& property) const { return property->getName(getValue(property)); }
private:
    friend class StateDefinition<Owner>;
    BlockState(Owner owner, std::vector<std::shared_ptr<PropertyBase>> properties, std::vector<std::any> values)
        : owner_(std::move(owner)), properties_(std::move(properties)), values_(std::move(values)) {}
    int valueIndex(const PropertyBase* property) const noexcept {
        for(std::size_t i=0;i<properties_.size();++i) if(properties_[i].get()==property) return static_cast<int>(i);
        return -1;
    }
    Owner owner_;
    std::vector<std::shared_ptr<PropertyBase>> properties_;
    std::vector<std::any> values_;
    std::vector<std::vector<Ptr>> neighbors_;
};

template<class Owner>
class StateDefinition {
public:
    using State = BlockState<Owner>;
    using StatePtr = std::shared_ptr<State>;
    class Builder {
    public:
        explicit Builder(Owner owner) : owner_(std::move(owner)) {}
        template<class... P> Builder& add(const std::shared_ptr<P>&... properties) {
            (addOne(std::static_pointer_cast<PropertyBase>(properties)), ...); return *this;
        }
        StateDefinition create() const { return StateDefinition(owner_, properties_); }
    private:
        void addOne(std::shared_ptr<PropertyBase> property) {
            if (!property) throw std::invalid_argument("null state property");
            auto valid=[](std::string_view s){ if(s.empty()) return false; for(unsigned char c:s) if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_')) return false; return true; };
            if(!valid(property->name())) throw std::invalid_argument("invalidly named property: " + property->name());
            if(property->valueCount()<=1) throw std::invalid_argument("property has <= 1 possible values: " + property->name());
            // Validate serialized names in the same domain as Java's regex.
            for(std::size_t i=0;i<property->valueCount();++i) {
                // Values are validated by the concrete property; expose names through helper below.
                auto name=propertyValueName(property.get(),i);
                if(!valid(name)) throw std::invalid_argument("invalidly named property value: " + name);
            }
            if(std::any_of(properties_.begin(),properties_.end(),[&](const auto& p){return p->name()==property->name();})) throw std::invalid_argument("duplicate property: " + property->name());
            properties_.push_back(std::move(property));
        }
        template<class T> static std::string propertyValueNameTyped(const Property<T>& p,std::size_t i){return p.getName(p.possibleValues()[i]);}
        static std::string propertyValueName(const PropertyBase* p,std::size_t i) {
            if(auto b=dynamic_cast<const Property<bool>*>(p)) return propertyValueNameTyped(*b,i);
            if(auto n=dynamic_cast<const Property<int>*>(p)) return propertyValueNameTyped(*n,i);
            // Enum types are caller-defined; use the type-erased name adapter.
            return p->nameForAny(p->valueAtAny(i));
        }
        Owner owner_;
        std::vector<std::shared_ptr<PropertyBase>> properties_;
    };
    const std::vector<StatePtr>& getPossibleStates() const noexcept { return states_; }
    StatePtr any() const { return states_.front(); }
    const Owner& getOwner() const noexcept { return owner_; }
    std::shared_ptr<PropertyBase> getProperty(std::string_view name) const {
        auto it=std::lower_bound(properties_.begin(),properties_.end(),name,[](const auto& p,std::string_view n){return p->name()<n;});
        return it!=properties_.end() && (*it)->name()==name ? *it : nullptr;
    }
    bool isSingletonState() const noexcept { return properties_.empty(); }
private:
    StateDefinition(Owner owner, std::vector<std::shared_ptr<PropertyBase>> properties)
        : owner_(std::move(owner)), properties_(std::move(properties)) {
        std::sort(properties_.begin(),properties_.end(),[](const auto& a,const auto& b){return a->name()<b->name();});
        generate();
    }
    void generate() {
        std::vector<std::any> values(properties_.size());
        std::function<void(std::size_t)> build=[&](std::size_t p) {
            if(p==properties_.size()) { states_.push_back(StatePtr(new State(owner_,properties_,values))); return; }
            for(std::size_t i=0;i<properties_[p]->valueCount();++i) { values[p]=properties_[p]->valueAtAny(i); build(p+1); }
        };
        build(0);
        for(const auto& state:states_) {
            state->neighbors_.resize(properties_.size());
            for(std::size_t p=0;p<properties_.size();++p) {
                auto& row=state->neighbors_[p]; row.resize(properties_[p]->valueCount());
                for(std::size_t i=0;i<row.size();++i) {
                    auto target=state->values_; target[p]=properties_[p]->valueAtAny(i);
                    std::size_t rank=0;
                    for(std::size_t j=0;j<target.size();++j) {
                        std::size_t stride=1;
                        for(std::size_t k=j+1;k<target.size();++k) stride*=properties_[k]->valueCount();
                        rank += static_cast<std::size_t>(properties_[j]->indexAny(target[j]))*stride;
                    }
                    row[i]=states_.at(rank);
                }
            }
        }
    }
    Owner owner_;
    std::vector<std::shared_ptr<PropertyBase>> properties_;
    std::vector<StatePtr> states_;
};

} // namespace carbon
