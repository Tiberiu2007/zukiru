// ActionMap — named, rebindable actions on top of raw device state.
//
// Game code should ask "is Jump pressed?", not "is Space or gamepad A down?".
// An ActionMap binds a logical action name to one or more physical inputs and
// evaluates it against an InputState:
//
//   ActionMap map;
//   map.bindAction("Jump", ButtonBinding::key(Key::Space));
//   map.bindAction("Jump", ButtonBinding::pad(GamepadButton::A));   // extra binding
//   map.bindAxis("MoveX", AxisBinding::keys(Key::D, Key::A));       // +D / -A
//   map.bindAxis("MoveX", AxisBinding::gamepad(GamepadAxis::LeftX, 1.0f, 0.15f));
//
//   if (map.actionPressed("Jump", state)) jump();
//   f32 x = map.axisValue("MoveX", state);   // [-1, 1]
//
// A button action fires if ANY of its bindings fires. An axis action sums the
// contributions of its bindings and clamps to [-1, 1]. Bindings may require
// keyboard modifiers (e.g. Ctrl+S) via ButtonBinding::withMods().
#pragma once

#include <zukiru/core/types.hpp>
#include <zukiru/input/input_state.hpp>
#include <zukiru/input/key_code.hpp>

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace zukiru::input {

// One physical trigger for a button action: a key, mouse button, or pad button,
// optionally gated on modifier keys. Build with the static factories.
struct ButtonBinding {
    enum class Source : u8 { Key, Mouse, Gamepad };

    Source source = Source::Key;
    u16 code = 0;  // reinterpreted per `source`
    KeyMods requiredMods = KeyMods::None;

    [[nodiscard]] static ButtonBinding key(Key k) {
        return {Source::Key, static_cast<u16>(k), KeyMods::None};
    }
    [[nodiscard]] static ButtonBinding mouse(MouseButton b) {
        return {Source::Mouse, static_cast<u16>(b), KeyMods::None};
    }
    [[nodiscard]] static ButtonBinding pad(GamepadButton b) {
        return {Source::Gamepad, static_cast<u16>(b), KeyMods::None};
    }
    // Require these modifiers to be held for the binding to fire.
    [[nodiscard]] ButtonBinding withMods(KeyMods mods) const {
        return {source, code, mods};
    }

    [[nodiscard]] bool down(const InputState& s) const {
        if (source != Source::Mouse && !hasMods(s.modifiers(), requiredMods)) return false;
        switch (source) {
            case Source::Key: return s.keyDown(static_cast<Key>(code));
            case Source::Mouse: return s.mouseButtonDown(static_cast<MouseButton>(code));
            case Source::Gamepad: return s.gamepadButtonDown(static_cast<GamepadButton>(code));
        }
        return false;
    }
    [[nodiscard]] bool pressed(const InputState& s) const {
        if (source != Source::Mouse && !hasMods(s.modifiers(), requiredMods)) return false;
        switch (source) {
            case Source::Key: return s.keyPressed(static_cast<Key>(code));
            case Source::Mouse: return s.mouseButtonPressed(static_cast<MouseButton>(code));
            case Source::Gamepad: return s.gamepadButtonPressed(static_cast<GamepadButton>(code));
        }
        return false;
    }
    [[nodiscard]] bool released(const InputState& s) const {
        switch (source) {
            case Source::Key: return s.keyReleased(static_cast<Key>(code));
            case Source::Mouse: return s.mouseButtonReleased(static_cast<MouseButton>(code));
            case Source::Gamepad: return s.gamepadButtonReleased(static_cast<GamepadButton>(code));
        }
        return false;
    }
};

// One source for an axis action: either a gamepad analog axis (with scale and a
// dead zone) or a pair of digital buttons giving +1 / -1.
struct AxisBinding {
    enum class Kind : u8 { Gamepad, Digital };

    Kind kind = Kind::Gamepad;
    GamepadAxis axis = GamepadAxis::LeftX;
    f32 scale = 1.0f;
    f32 deadzone = 0.0f;
    ButtonBinding positive{};
    ButtonBinding negative{};

    // Analog: read `axis`, apply dead zone, then scale (use -1 scale to invert).
    [[nodiscard]] static AxisBinding gamepad(GamepadAxis axis, f32 scale = 1.0f,
                                             f32 deadzone = 0.0f) {
        AxisBinding b;
        b.kind = Kind::Gamepad;
        b.axis = axis;
        b.scale = scale;
        b.deadzone = deadzone;
        return b;
    }
    // Digital: `positive` contributes +1, `negative` -1 (both held cancels out).
    [[nodiscard]] static AxisBinding keys(Key positive, Key negative) {
        return buttons(ButtonBinding::key(positive), ButtonBinding::key(negative));
    }
    [[nodiscard]] static AxisBinding buttons(ButtonBinding positive, ButtonBinding negative) {
        AxisBinding b;
        b.kind = Kind::Digital;
        b.positive = positive;
        b.negative = negative;
        return b;
    }

    [[nodiscard]] f32 value(const InputState& s) const {
        if (kind == Kind::Gamepad) {
            const f32 raw = s.gamepadAxis(axis);
            const f32 mag = raw < 0.0f ? -raw : raw;
            if (mag <= deadzone) return 0.0f;
            return raw * scale;
        }
        f32 v = 0.0f;
        if (positive.down(s)) v += 1.0f;
        if (negative.down(s)) v -= 1.0f;
        return v;
    }
};

class ActionMap {
public:
    // Add a physical trigger to a button action (creating the action if new).
    void bindAction(std::string action, ButtonBinding binding) {
        buttonActions_[std::move(action)].push_back(binding);
    }
    // Add a source to an axis action (creating the action if new).
    void bindAxis(std::string action, AxisBinding binding) {
        axisActions_[std::move(action)].push_back(binding);
    }

    // Drop every binding for a named action.
    void clearAction(std::string_view action) {
        buttonActions_.erase(std::string{action});
        axisActions_.erase(std::string{action});
    }
    void clear() {
        buttonActions_.clear();
        axisActions_.clear();
    }

    // A button action is active/pressed/released if ANY of its bindings is.
    [[nodiscard]] bool actionActive(std::string_view action, const InputState& s) const {
        return anyBinding(action, s, [](const ButtonBinding& b, const InputState& st) {
            return b.down(st);
        });
    }
    [[nodiscard]] bool actionPressed(std::string_view action, const InputState& s) const {
        return anyBinding(action, s, [](const ButtonBinding& b, const InputState& st) {
            return b.pressed(st);
        });
    }
    [[nodiscard]] bool actionReleased(std::string_view action, const InputState& s) const {
        return anyBinding(action, s, [](const ButtonBinding& b, const InputState& st) {
            return b.released(st);
        });
    }

    // Summed contribution of every binding, clamped to [-1, 1]. Unknown/unbound
    // actions read 0.
    [[nodiscard]] f32 axisValue(std::string_view action, const InputState& s) const {
        const auto it = axisActions_.find(std::string{action});
        if (it == axisActions_.end()) return 0.0f;
        f32 sum = 0.0f;
        for (const AxisBinding& b : it->second) sum += b.value(s);
        if (sum > 1.0f) return 1.0f;
        if (sum < -1.0f) return -1.0f;
        return sum;
    }

    [[nodiscard]] bool hasAction(std::string_view action) const {
        return buttonActions_.contains(std::string{action});
    }
    [[nodiscard]] bool hasAxis(std::string_view action) const {
        return axisActions_.contains(std::string{action});
    }

private:
    template <class Pred>
    [[nodiscard]] bool anyBinding(std::string_view action, const InputState& s, Pred pred) const {
        const auto it = buttonActions_.find(std::string{action});
        if (it == buttonActions_.end()) return false;
        for (const ButtonBinding& b : it->second) {
            if (pred(b, s)) return true;
        }
        return false;
    }

    std::unordered_map<std::string, std::vector<ButtonBinding>> buttonActions_;
    std::unordered_map<std::string, std::vector<AxisBinding>> axisActions_;
};

}  // namespace zukiru::input
