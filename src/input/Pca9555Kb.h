#pragma once
#include "configuration.h"
#ifdef SHIPWRECKED

#include "InputBroker.h"
#include "concurrency/OSThread.h"

class Pca9555Kb : public Observable<const InputEvent *>, public concurrency::OSThread
{
  public:
    explicit Pca9555Kb();
    int32_t runOnce() override;

    // WARNING: these enum VALUES correspond to PCA9555 GPIO bit positions,
    // which are DETERMINED BY PCB TRACE ROUTING. Do NOT renumber — always
    // keep (value == PCA9555 GPIO bit index). To change a function, rename
    // the enum label, not its value.
    //
    // CORRECT mapping (verified by physical button test 2026-06-17):
    //   0=SW3  1=SW4  2=SW5   3=SW6   4=SW7   5=SW8
    //   6=SW9  7=SW10 8=SW11  9=SW12 10=SW13 11=SW14
    //  12=SW15 13=SW16 14=SW17 15=SW18
    enum KeyIndex : uint8_t {
        KEY_UP    = 0,  // SW3  (GPIO P00)
        KEY_OK    = 1,  // SW4  (GPIO P01)
        KEY_DOWN  = 2,  // SW5  (GPIO P02)
        KEY_9     = 3,  // SW6  (GPIO P03)
        KEY_5     = 4,  // SW7  (GPIO P04)
        KEY_2     = 5,  // SW8  (GPIO P05)
        KEY_4     = 6,  // SW9  (GPIO P06)
        KEY_8     = 7,  // SW10 (GPIO P07)
        KEY_RIGHT = 8,  // SW11 (GPIO P10)
        KEY_LEFT  = 9,  // SW12 (GPIO P11)
        KEY_7     = 10, // SW13 (GPIO P12)
        KEY_BSP   = 11, // SW14 (GPIO P13)
        KEY_1     = 12, // SW15 (GPIO P14)
        KEY_3     = 13, // SW16 (GPIO P15)
        KEY_SPACE = 14, // SW17 (GPIO P16)
        KEY_6     = 15, // SW18 (GPIO P17)
    };

    // PCA9555 index → T9 table index (0-8), -1 = not a T9 key
    static int t9TableForIndex(int index);

    /// Read current GPIO input state (both ports, 16 bits).
    uint16_t readInputs();
#ifdef ARCH_RP2040
    void core1Poll();
#endif

  private:
    static constexpr uint8_t ADDR = PCA9555_ADDR;
    static constexpr uint8_t REG_INPUT_0 = 0x00;
    static constexpr uint8_t REG_INPUT_1 = 0x01;
    static constexpr uint8_t REG_CONFIG_0 = 0x06;
    static constexpr uint8_t REG_CONFIG_1 = 0x07;
    static constexpr uint32_t DEBOUNCE_MS = 50;

    void writeRegister(uint8_t reg, uint8_t value);
    void emitEvent(input_broker_event type, unsigned char kbchar = 0);

    uint16_t prev_state;
    bool firstTime;
};

extern Pca9555Kb *pca9555Kb;
#endif
