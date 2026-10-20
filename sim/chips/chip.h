// A verilated chip module driven by its pin numbers, and checks of
// function tables over every input combination.
#pragma once

#include <doctest/doctest.h>
#include <verilated.h>

#include <functional>
#include <map>
#include <random>
#include <utility>
#include <vector>

// PIN(n): a table entry naming the port pN of the model type V.
#define PIN(n) {n, [](V& m) -> CData& { return m.p##n; }}

template <class V>
class Chip {
public:
    using Access = CData& (*)(V&);

    explicit Chip(std::map<int, Access> pins, std::map<int, Access> enables = {})
        : pins_(std::move(pins)), enables_(std::move(enables)) {}

    void set(int pin, bool level) { pins_.at(pin)(model_) = level ? 1 : 0; }
    bool get(int pin) { return pins_.at(pin)(model_) != 0; }
    bool has_enable(int pin) const { return enables_.count(pin) != 0; }
    bool driven(int pin) { return enables_.at(pin)(model_) != 0; }
    void eval() { model_.eval(); }
    V& model() { return model_; }

    // One period of the master clock: what the pins hold now is taken at
    // its rising edge.
    void tick()
    {
        model_.clk = 0;
        model_.eval();
        model_.clk = 1;
        model_.eval();
    }

private:
    V model_;
    std::map<int, Access> pins_;
    std::map<int, Access> enables_;
};

// OE(n): the enable port pN_oe of a three-state output.
#define OE(n) {n, [](V& m) -> CData& { return m.p##n##_oe; }}

// An expected output level: 0, 1, or Z - a three-state output let go.
enum Level { L0 = 0, L1 = 1, Z = 2 };

using TableFn = std::function<std::vector<Level>(const std::vector<bool>&)>;

// Every combination of the input pins (in[0] the lowest bit); fn gives the
// expected level of each output pin in `out`.
template <class V>
void check_table(Chip<V>& chip, const std::vector<int>& in, const std::vector<int>& out,
                 const TableFn& fn)
{
    for (unsigned m = 0; m < (1u << in.size()); ++m) {
        std::vector<bool> v;
        for (std::size_t i = 0; i < in.size(); ++i) {
            v.push_back(((m >> i) & 1u) != 0);
            chip.set(in[i], v.back());
        }
        chip.eval();
        const std::vector<Level> want = fn(v);
        for (std::size_t k = 0; k < out.size(); ++k) {
            const int pin = out[k];
            if (chip.has_enable(pin)) {
                CHECK_MESSAGE(chip.driven(pin) == (want[k] != Z), "pin " << pin << " inputs " << m);
                if (want[k] == Z)
                    continue;
            }
            CHECK_MESSAGE(chip.get(pin) == (want[k] == L1), "pin " << pin << " inputs " << m);
        }
    }
}

inline Level lv(bool b) { return b ? L1 : L0; }

// The number held by input bits v[first .. first+count-1], v[first] lowest.
inline unsigned bits(const std::vector<bool>& v, std::size_t first, std::size_t count)
{
    unsigned n = 0;
    for (std::size_t i = 0; i < count; ++i)
        n |= (v[first + i] ? 1u : 0u) << i;
    return n;
}

// A gate: its input pins and its output pin.
struct Gate {
    std::vector<int> in;
    int out;
};

using GateFn = std::function<bool(const std::vector<bool>&)>;

// Every gate through all its input combinations, then all the gates at
// once from random inputs (which catches a gate wired to another's pins).
template <class V>
void check_gates(Chip<V>& chip, const std::vector<Gate>& gates, const GateFn& fn)
{
    for (const Gate& g : gates) {
        for (unsigned m = 0; m < (1u << g.in.size()); ++m) {
            std::vector<bool> v;
            for (std::size_t i = 0; i < g.in.size(); ++i) {
                v.push_back(((m >> i) & 1u) != 0);
                chip.set(g.in[i], v.back());
            }
            chip.eval();
            CHECK_MESSAGE(chip.get(g.out) == fn(v), "pin " << g.out << " inputs " << m);
        }
    }
    std::mt19937 rng(515);
    for (int round = 0; round < 256; ++round) {
        std::vector<std::vector<bool>> all;
        for (const Gate& g : gates) {
            std::vector<bool> v;
            for (int pin : g.in) {
                v.push_back((rng() & 1u) != 0);
                chip.set(pin, v.back());
            }
            all.push_back(v);
        }
        chip.eval();
        for (std::size_t k = 0; k < gates.size(); ++k)
            CHECK(chip.get(gates[k].out) == fn(all[k]));
    }
}

inline bool all_of(const std::vector<bool>& v)
{
    for (bool b : v)
        if (!b)
            return false;
    return true;
}

inline bool any_of(const std::vector<bool>& v)
{
    for (bool b : v)
        if (b)
            return true;
    return false;
}
