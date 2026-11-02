
#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <Arduino.h>
#include "../comon.h"

template <typename Owner, typename State, size_t N>
class StateMachine
{
public:
    using StateFunction = void (Owner::*)();
    using ConditionFunction = bool (Owner::*)();

    struct Transition
    {
        State target;
        ConditionFunction condition;
        bool enabled;
    };

    StateMachine(Owner *owner, State initialState)
        : owner(owner), currentState(initialState), prevState(initialState)
    {
        for (size_t i = 0; i < N; i++)
        {
            states[i] = nullptr;

            for (size_t j = 0; j < N; j++)
            {
                transitions[i][j].target = static_cast<State>(j);
                transitions[i][j].condition = nullptr;
                transitions[i][j].enabled = false;
            }
        }
    }

    void addState(State state, StateFunction function)
    {
        states[index(state)] = function;
    }

    void addTransition(
        State from,
        State to,
        ConditionFunction condition)
    {
        Transition &t = transitions[index(from)][index(to)];

        t.target = to;
        t.condition = condition;
        t.enabled = true;
    }

    void setState(State state)
    {
        prevState = currentState;
        currentState = state;
    }

    State getState() const
    {
        return currentState;
    }

    State getPrevState() const
    {
        return prevState;
    }

    const char *getStateName() const
    {
        return owner->getStateName(currentState);
    }

    const char *getPrevStateName() const
    {
        return owner->getStateName(prevState);
    }

    void run()
    {
        size_t current = index(currentState);

        // Aktuális állapot végrehajtása
        if (states[current] != nullptr)
        {
            (owner->*states[current])();
        }

        // Átmenetek ellenőrzése
        for (size_t i = 0; i < N; i++)
        {
            Transition &t = transitions[current][i];

            if (!t.enabled)
                continue;

            if (t.condition == nullptr ||
                (owner->*t.condition)())
            {
                prevState = currentState;
                currentState = t.target;
                logInfo("[STATE] %s: %s -> %s\n",
                           owner->name.c_str(),
                           getPrevStateName(),
                           getStateName());
                break;
            }
        }
    }

    State prevState;

private:
    size_t index(State state) const
    {
        return static_cast<size_t>(state);
    }

    Owner *owner;
    State currentState;

    StateFunction states[N];
    Transition transitions[N][N];
};

#endif
