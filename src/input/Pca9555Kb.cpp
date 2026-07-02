#include "Pca9555Kb.h"
#include "InputBroker.h"
#include "configuration.h"
#include <Wire.h>

#ifdef SHIPWRECKED

#ifdef ARCH_RP2040
struct PcaEvent {
    input_broker_event type;
    unsigned char kbchar;
};
static volatile PcaEvent pcaQueue[32];
static volatile uint8_t qHead = 0;
static volatile uint8_t qTail = 0;

static void queueEvent(input_broker_event type, unsigned char kbchar = 0) {
    uint8_t next = (qHead + 1) % 32;
    if (next != qTail) {
        pcaQueue[qHead].type = type;
        pcaQueue[qHead].kbchar = kbchar;
        qHead = next;
    }
}
#endif

Pca9555Kb *pca9555Kb;

// CORRECT mapping (verified by physical button test 2026-06-17)
static const char *sw_name[16] = {
    "SW3 (UP)",   "SW4 (OK)",   "SW5 (DOWN)", "SW6 (9)",
    "SW7 (5)",    "SW8 (2)",    "SW9 (4)",    "SW10 (8)",
    "SW11 (RIGHT)","SW12 (LEFT)","SW13 (7)",  "SW14 (BSP)",
    "SW15 (1)",   "SW16 (3)",   "SW17 (SPC)", "SW18 (6)",
};

static const char *sw_func[16] = {
    "UP",       "OK",      "DOWN",      "T9 9",
    "T9 5",     "T9 2",    "T9 4",      "T9 8",
    "RIGHT",    "LEFT",    "T9 7",      "BACKSPACE",
    "T9 1",     "T9 3",    "SPACE",     "T9 6",
};

// PCA9555 GPIO pin → T9 table index (-1 = not T9).
// CORRECT mapping (verified by physical button test 2026-06-17).
static const int pca_to_t9[16] = {
    -1, //  0 GPIO P00 (SW3)  = UP
    -1, //  1 GPIO P01 (SW4)  = OK
    -1, //  2 GPIO P02 (SW5)  = DOWN
    8,  //  3 GPIO P03 (SW6)  = T9 9 → wxyz9
    4,  //  4 GPIO P04 (SW7)  = T9 5 → jkl5
    1,  //  5 GPIO P05 (SW8)  = T9 2 → abc2
    3,  //  6 GPIO P06 (SW9)  = T9 4 → ghi4
    7,  //  7 GPIO P07 (SW10) = T9 8 → tuv8
    -1, //  8 GPIO P10 (SW11) = RIGHT
    -1, //  9 GPIO P11 (SW12) = LEFT
    6,  // 10 GPIO P12 (SW13) = T9 7 → pqrs7
    -1, // 11 GPIO P13 (SW14) = BACKSPACE
    0,  // 12 GPIO P14 (SW15) = T9 1 → symbols
    2,  // 13 GPIO P15 (SW16) = T9 3 → def3
    -1, // 14 GPIO P16 (SW17) = SPACE
    5,  // 15 GPIO P17 (SW18) = T9 6 → mno6
};

int Pca9555Kb::t9TableForIndex(int index)
{
    if (index >= 0 && index < 16)
        return pca_to_t9[index];
    return -1;
}

Pca9555Kb::Pca9555Kb() : concurrency::OSThread("PCA9555KB"), prev_state(0xFFFF), firstTime(true)
{
    if (inputBroker)
        inputBroker->registerSource(this);
}

void Pca9555Kb::writeRegister(uint8_t reg, uint8_t value)
{
    Wire.beginTransmission(ADDR);
    Wire.write(reg);
    Wire.write(value);
    Wire.endTransmission();
}

uint16_t Pca9555Kb::readInputs()
{
    Wire.beginTransmission(ADDR);
    Wire.write(REG_INPUT_0);
    Wire.endTransmission(false);
    uint8_t buf[2] = {0, 0};
    if (Wire.requestFrom(ADDR, 2) == 2) {
        buf[0] = Wire.read();
        buf[1] = Wire.read();
    }
    return buf[0] | ((uint16_t)buf[1] << 8);
}

void Pca9555Kb::emitEvent(input_broker_event type, unsigned char kbchar)
{
    InputEvent evt = {};
    evt.source = "PCA9555KB";
    evt.inputEvent = type;
    evt.kbchar = kbchar;
    this->notifyObservers(&evt);
}

int32_t Pca9555Kb::runOnce()
{
    if (firstTime) {
        firstTime = false;
        writeRegister(REG_CONFIG_0, 0xFF);
        writeRegister(REG_CONFIG_1, 0xFF);
        prev_state = readInputs();
    }

#ifdef ARCH_RP2040
    // Pop from Core 1 queue
    while (qHead != qTail) {
        input_broker_event type = pcaQueue[qTail].type;
        unsigned char kbchar = pcaQueue[qTail].kbchar;
        qTail = (qTail + 1) % 32;
        emitEvent(type, kbchar);
    }
    return 10;
#else
    uint16_t state = readInputs();
    uint16_t pressed = (prev_state ^ state) & ~state;
    prev_state = state;

    if (!pressed)
        return 10;

    for (int i = 0; i < 16; i++) {
        if (!(pressed & (1 << i)))
            continue;

        Serial.printf("BUTTON: GPIO %02d = %s → %s\n",
                      i, sw_name[i], sw_func[i]);

        int tbl = pca_to_t9[i];

        if (tbl >= 0) {
            // T9 key — emit MATRIXKEY with table index
            emitEvent(INPUT_BROKER_MATRIXKEY, (unsigned char)tbl);
        } else {
            switch (i) {
            case KEY_DOWN:
                emitEvent(INPUT_BROKER_DOWN);
                break;
            case KEY_OK:
                emitEvent(INPUT_BROKER_SELECT);
                break;
            case KEY_RIGHT:
                emitEvent(INPUT_BROKER_RIGHT);
                break;
            case KEY_LEFT:
                emitEvent(INPUT_BROKER_LEFT);
                break;
            case KEY_UP:
                emitEvent(INPUT_BROKER_UP);
                break;
            case KEY_BSP:
                emitEvent(INPUT_BROKER_BACK);
                break;
            case KEY_SPACE:
                emitEvent(INPUT_BROKER_ANYKEY, ' ');
                break;
            default:
                break;
            }
        }
    }

    return 10;
#endif
}

#ifdef ARCH_RP2040
void Pca9555Kb::core1Poll() {
    if (firstTime) return; // Wait until Core 0 initializes registers
    
    uint16_t state = readInputs();
    uint16_t pressed = (prev_state ^ state) & ~state;
    prev_state = state;

    if (!pressed) return;

    for (int i = 0; i < 16; i++) {
        if (!(pressed & (1 << i)))
            continue;

        Serial.printf("BUTTON: GPIO %02d = %s → %s\n",
                      i, sw_name[i], sw_func[i]);

        int tbl = pca_to_t9[i];

        if (tbl >= 0) {
            queueEvent(INPUT_BROKER_MATRIXKEY, (unsigned char)tbl);
        } else {
            switch (i) {
            case KEY_DOWN:
                queueEvent(INPUT_BROKER_DOWN);
                break;
            case KEY_OK:
                queueEvent(INPUT_BROKER_SELECT);
                break;
            case KEY_RIGHT:
                queueEvent(INPUT_BROKER_RIGHT);
                break;
            case KEY_LEFT:
                queueEvent(INPUT_BROKER_LEFT);
                break;
            case KEY_UP:
                queueEvent(INPUT_BROKER_UP);
                break;
            case KEY_BSP:
                queueEvent(INPUT_BROKER_BACK);
                break;
            case KEY_SPACE:
                queueEvent(INPUT_BROKER_ANYKEY, ' ');
                break;
            default:
                break;
            }
        }
    }
}

void setup1() {
}

void loop1() {
    if (pca9555Kb) {
        pca9555Kb->core1Poll();
    }
    delay(15); // Poll super fast on Core 1 (15ms)
}
#endif

#endif
