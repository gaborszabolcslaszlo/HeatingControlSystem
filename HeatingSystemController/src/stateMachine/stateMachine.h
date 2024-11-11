#include <iostream>
#include <string>
#include <map>
#include <functional>

// Állapotgép osztály
class StateMachine
{
public:
    using StateFunction = std::function<void()>;

    // Állapot regisztrálása
    void addState(const std::string &stateName, StateFunction function)
    {
        states[stateName] = function;
    }

    // Állapot beállítása
    void setState(const std::string &stateName)
    {
        currentState = stateName;
        std::cout << "Állapot váltás: " << currentState << std::endl;
    }

    // Állapot futtatása
    void run()
    {
        if (states.find(currentState) != states.end())
        {
            states[currentState](); // Futtatja az aktuális állapothoz tartozó függvényt
        }
        else
        {
            std::cout << "Ismeretlen állapot: " << currentState << std::endl;
        }
    }

private:
    std::map<std::string, StateFunction> states;
    std::string currentState;
};
