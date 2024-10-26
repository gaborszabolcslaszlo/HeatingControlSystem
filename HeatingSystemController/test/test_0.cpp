#include <Arduino.h>
#include <unity.h>
#include "sensors.h"

// Tesztelés előtti beállítások
void setUp() {
    // Inicializációs kód a tesztek előtt
}

// Tesztelés utáni takarítás
void tearDown() {
    // Takarítás a tesztek után
}

// Egy egyszerű teszt
void test_led_builtin_pin_number() {
    TEST_ASSERT_EQUAL(2, LED_BUILTIN);  // Ellenőrzi, hogy az LED_BUILTIN a helyes érték-e
}

void test_one_wire_pin_config() {
    TEST_ASSERT_EQUAL(2, ONE_WIRE_BUS);  // Ellenőrzi, hogy az LED_BUILTIN a helyes érték-e
}

void setup() {
    // Tesztelés indítása
    UNITY_BEGIN();

    // Hívjuk meg a teszt funkciókat
    RUN_TEST(test_led_builtin_pin_number);

    RUN_TEST(test_one_wire_pin_config);
    // Tesztek lezárása
    UNITY_END();
}

void loop() {
    // Az ESP8266 folyamatosan futtatja a loop-ot, de a tesztek csak egyszer futnak le
}
