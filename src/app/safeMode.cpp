#include "safeMode.h"

#include "boards/board.h"
#include "hw/logger.h"

void safeMode(const String &reason)
{
    pinMode(LED_PIN, OUTPUT);
    while (true)
    {
        LOG("FATAL: " + reason + " - halted in safe mode");
        for (int i = 0; i < 10; i++)
        {
            digitalWrite(LED_PIN, !digitalRead(LED_PIN));
            delay(50);
        }
    }
}
