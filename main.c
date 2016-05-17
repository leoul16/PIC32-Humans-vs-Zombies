/* 
 * File:   main.c
 * Author: leoul_y
 *
 * Created on December 7, 2015, 4:22 PM
 */

// Comment this define out to *not* use the OledDrawGlyph() function
#define USE_OLED_DRAW_GLYPH

#ifdef USE_OLED_DRAW_GLYPH
// forward declaration of OledDrawGlyph() to satisfy the compiler
void OledDrawGlyph(char ch);
#endif

#include <plib.h>
#include <time.h>
#include <stdbool.h>
#include "delay.h"
#include "PmodOLED.h"
#include "OledGrph.h"
#include "OledChar.h"
#include "ADXL345.h"
#include "Communication.h"

// Return value check macro
#define CHECK_RET_VALUE(a) { \
if (a == 0) { \
LATGSET = 0xF << 12; \
return(EXIT_FAILURE) ; \
} \
}

// These are options for the configuration of your board.
// Do not modify these without the guidance of your instructor or GTA.
#pragma config FPLLMUL  = MUL_20        // PLL Multiplier
#pragma config FPLLIDIV = DIV_2         // PLL Input Divider
#pragma config FPLLODIV = DIV_1         // PLL Output Divider
#pragma config FPBDIV   = DIV_8         // Peripheral Clock divisor
#pragma config FWDTEN   = OFF           // Watchdog Timer
#pragma config WDTPS    = PS1           // Watchdog Timer Postscale
#pragma config FCKSM    = CSDCMD        // Clock Switching & Fail Safe Clock Monitor
#pragma config OSCIOFNC = OFF           // CLKO Enable
#pragma config POSCMOD  = HS            // Primary Oscillator
#pragma config IESO     = OFF           // Internal/External Switch-over
#pragma config FSOSCEN  = OFF           // Secondary Oscillator Enable
#pragma config FNOSC    = PRIPLL        // Oscillator Selection
#pragma config CP       = OFF           // Code Protect
#pragma config BWP      = OFF           // Boot Flash Write Protect
#pragma config PWP      = OFF           // Program Flash Write Protect
#pragma config ICESEL   = ICS_PGx1      // ICE/ICD Comm Channel Select
#pragma config DEBUG    = OFF           // Debugger Disabled for Starter Kit


// Programs can use GLOBAL VARIABLES when a variable
// needs be visible to the main function and to a function
// outside of main. In this case the main function uses a
// millisecond counter that the interrupt handler function
// updates. But the interrupt handler cannot receive arguments from main.
// This is updated once per millisecond by the interrupt handler.
// It is updated once per second because that is the way timer 2 was
// initialized. In general, global variables should be avoided.
unsigned int millisec;
unsigned int gameStart = 0;
int gameT;


typedef enum States {INIT, OPTIONS, MOVE, DISPLAY, WIN, LOSE};
typedef enum Direction {LEFT, RIGHT, UP, DOWN, FLEFT, FRIGHT, FUP, FDOWN, DONE};

typedef struct _position {
    int x;
    int y;
}position;

short x, y, z = 0;                          //stores values as X, Y, and Z

int channel = 0;

unsigned char BTN1Hist = 0x00;
unsigned char BTN2Hist = 0x00;
unsigned char BTN3Hist = 0x00;


BYTE human[8] = {0x00, 0x00, 0x96, 0x7F, 0x7F, 0x96, 0x00, 0x00};
BYTE zomb[8] = {0x08, 0x96, 0x75, 0x71, 0x75, 0x96, 0x08, 0x00};
BYTE blank[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

char zomb_char = 0x00;
char blank_char = 0x01;
char hum_char = 0x02;
//char body_char = 0x03;

enum Direction direc;


// This configuration option defines the timer interrupt handler.
#pragma interrupt TimerInterruptHandler ipl1 vector 0

// Timer compare setting
// The period of Timer 2 is (16 * (624+1))/(10 MHz) = 1 ms.
#define TIMER_PERIOD_1_MSEC 624

void TimerInterruptHandler(void) {
    if (INTGetFlag(INT_T2)) {

        millisec++; //increment ms counter

        BTN1Hist <<= 1; //updates history
        if (PORTG & 0x40)
            BTN1Hist |= 0x01;

        BTN2Hist <<= 1;
        if (PORTG & 0x80)
            BTN2Hist |= 0x01;

        BTN3Hist <<= 1;
        if (PORTA & 0x01)
            BTN3Hist |= 0x01;

        INTClearFlag(INT_T2); // Acknowledge the interrupt source by clearing its flag.

    }


}

bool isLost(position* person, position* zombie)
{
    if ((person->x == zombie->x) & (person->y == zombie->y))
        return true;
    else
        return false;

}

bool isWon(int gameTime, int goalTime)
{
    if (gameTime >= goalTime)
        return true;
    else
        return false;
}
int computeSpeed(int x, int y)
{
    if ((y > 5) & (y < 20))
        direc = RIGHT;

    else if (y > 20)
        direc = FRIGHT;

    else if ((y < -5) & (y > -20))
        direc = LEFT;

    else if (y < -20)
        direc = FLEFT;

    else if ((x > 5) & (x < 18))
        direc = UP;

    else if (x > 18)
        direc = FUP;

    else if ((x < -5) & (x > -18))
        direc = DOWN;

    else if (x < -18)
        direc = FDOWN;
}


/*
 * 
 */
int main() {

    DDPCONbits.JTAGEN = 0;


    // Initialize GPIO for LEDs
    TRISGCLR = 0xf000; // For LEDs: configure PortG pins for output
    ODCGCLR = 0xf000; // For LEDs: configure as normal output (not open drain)

    TRISGSET = 0x40;
    TRISGSET = 0x80;
    TRISASET = 0x01;

    TRISBCLR = 1<<6;
    ODCBCLR  = 1<<6;
    LATBCLR  = 1<<6;
    LATBSET  = 1<<6;

    DelayInit();
    OledInit();

    // Configure Timer 2 to request a real-time interrupt once
    // per millisecond based on constant from above.
    OpenTimer2(T2_ON|T2_IDLE_CON|T2_SOURCE_INT|T2_PS_1_16|T2_GATE_OFF,
            TIMER_PERIOD_1_MSEC);

    // Configure the CPU to respond to Timer 2's interrupt requests.
    INTConfigureSystem(INT_SYSTEM_CONFIG_SINGLE_VECTOR);
    INTSetVectorPriority(INT_TIMER_2_VECTOR, INT_PRIORITY_LEVEL_1);
    INTClearFlag(INT_T2);
    INTEnable(INT_T2, INT_ENABLED);
    INTEnableInterrupts();

    channel = ADXL345_Init();
    ADXL345_SetPowerMode(0x1);
    ADXL345_SetRegisterValue(ADXL345_DATA_FORMAT, ADXL345_RANGE(3));


    enum States state = INIT;

    char oledstring[17];

    int winTime = 20;

    int retValue = 0;

    bool check;

    unsigned int ms_since_last_update = 0;
    unsigned int ms_since_zomb_update = 0;
    unsigned int lastupdate = 0;
    unsigned int zombieupdate = 0;

    position person;
    person.x = 0;
    person.y = 0;

    position zombie;
    zombie.x = 0;
    zombie.y = 0;


    retValue = OledDefUserChar(blank_char, blank);
    CHECK_RET_VALUE(retValue);
    retValue = OledDefUserChar(hum_char, human);
    CHECK_RET_VALUE(retValue);
    retValue = OledDefUserChar(zomb_char, zomb);
    CHECK_RET_VALUE(retValue);

    srand( time(NULL));


    OledClearBuffer();

    while (1) {

        switch (state) {
            case INIT:
                // SPLASH SCREEN
                OledSetCursor(0, 0);
                #ifdef USE_OLED_DRAW_GLYPH
                    OledDrawGlyph(hum_char);
                #else
                    OledPutChar(head_char);
                #endif

                OledSetCursor(2, 0);
                #ifdef USE_OLED_DRAW_GLYPH
                    OledDrawGlyph(hum_char);
                #else
                    OledPutChar(head_char);
                #endif

                OledSetCursor(5, 0);
                OledPutString("HUMANS");
                OledSetCursor(12, 0);
                #ifdef USE_OLED_DRAW_GLYPH
                    OledDrawGlyph(hum_char);
                #else
                    OledPutChar(head_char);
                #endif

                OledSetCursor(14, 0);
                #ifdef USE_OLED_DRAW_GLYPH
                    OledDrawGlyph(hum_char);
                #else
                    OledPutChar(head_char);
                #endif
                OledSetCursor(7, 1);
                OledPutString("VS");
                OledSetCursor(0, 2);
                #ifdef USE_OLED_DRAW_GLYPH
                    OledDrawGlyph(zomb_char);
                #else
                    OledPutChar(apple_char);
                #endif
                OledSetCursor(2, 2);
                #ifdef USE_OLED_DRAW_GLYPH
                    OledDrawGlyph(zomb_char);
                #else
                    OledPutChar(apple_char);
                #endif
                    OledSetCursor(4, 2);
                    OledPutString("ZOMBIES");
                     OledSetCursor(12, 2);
                #ifdef USE_OLED_DRAW_GLYPH
                    OledDrawGlyph(zomb_char);
                #else
                    OledPutChar(apple_char);
                #endif
                OledSetCursor(14, 2);
                #ifdef USE_OLED_DRAW_GLYPH
                    OledDrawGlyph(zomb_char);
                #else
                    OledPutChar(apple_char);
                #endif
                OledSetCursor(1, 3);
                OledPutString("Leoul Yiheyis");
                OledUpdate();
                
                if ((millisec/1000) == 5)
                {
                    OledClearBuffer();
                    state = OPTIONS;
                }


               // RANDOMIZATION OF HUMAN & ZOMBIE POSITION
               person.x = rand() % 10 + 1;
               person.y = rand() % 2 + 1;

               srand((2* time(NULL))/3);

               zombie.x = rand() % 9 + 2;
               zombie.y = rand() % 2 + 2;
               break;

            case OPTIONS:
                // OPTIONS SCREEN
                OledSetCursor(0, 0);
                sprintf(oledstring, "Win Time:  %4d", winTime);
                OledPutString(oledstring);
                OledSetCursor(0, 1);
                OledPutString("BTN 1:  Time++");
                OledSetCursor(0, 2);
                OledPutString("BTN 2:  Time--");
                OledSetCursor(0, 3);
                OledPutString("BTN 3:  Accept");
                OledUpdate();

                if (BTN1Hist == 0xFF) //button 1 increments
                {
                    winTime = winTime + 10;
                    BTN1Hist = 0x00;
                    OledUpdate();
                }

                if (BTN2Hist == 0xFF) //button 2 decrements
                {
                    winTime = winTime - 10;
                    BTN2Hist = 0x00;
                    OledUpdate();
                }

                if (BTN3Hist == 0xFF) //button 3 statrs game
                {
                    OledClearBuffer();
                    state = DISPLAY;
                    gameStart = millisec;
                    BTN3Hist = 0x00;

                // PRINTS INTIAL HUMAN & ZOMBIE POSITIONS
                OledSetCursor(person.x, person.y);
                #ifdef USE_OLED_DRAW_GLYPH
                    OledDrawGlyph(hum_char);
                    OledUpdate();
                #else
                    OledPutChar(head_char);
                #endif
                    OledSetCursor(zombie.x, zombie.y);
                #ifdef USE_OLED_DRAW_GLYPH
                    OledDrawGlyph(zomb_char);
                #else
                    OledPutChar(blank_char);
                #endif
                    OledUpdate();
                }

                

                break;

            case DISPLAY:
                while (direc != DONE)
                {
                    ADXL345_GetXyz(&x, &y, &z);
                    computeSpeed(x, y);

                    if (BTN3Hist == 0xFF) //button 3 restatrs
                    {
                        OledClearBuffer();
                        state = INIT;;
                        BTN3Hist = 0x00;
                    }

                    // DISPLAY RIGHT MOVEMENT
                    while (direc == RIGHT)
                    {
                        ms_since_last_update = millisec - lastupdate;
                        ms_since_zomb_update = millisec - zombieupdate;

                        // LOSE CASES
                        if (person.y == zombie.y)
                        {
                            if ((person.x == 9) && (zombie.x == 11))
                            {
                                state = LOSE;
                                direc = DONE;
                            }

                            else if ((person.x == 11) && (zombie.x = 9))
                            {
                                state = LOSE;
                                direc = DONE;
                            }
                        }

                        //HUMAN DISPLAY UPDATE
                        if ((ms_since_last_update >= 800) && (state != LOSE))
                        {
                            if (person.x < 11)
                            {
                                lastupdate = millisec;

                                OledSetCursor(person.x, person.y);
                                #ifdef USE_OLED_DRAW_GLYPH
                                            OledDrawGlyph(blank_char);
                                #else
                                            OledPutChar(blank_char);
                                #endif
                                // Update the glyph position

                                person.x++;

                                OledSetCursor(person.x, person.y);
                                #ifdef USE_OLED_DRAW_GLYPH
                                            OledDrawGlyph(hum_char);
                                            //OledUpdate();
                                #else
                                            OledPutChar(head_char);
                                #endif

                                            updateScore();
                                check = isLost(&person, &zombie);
                                if (check == true)
                                {
                                    state = LOSE;
                                    direc = DONE;
                                }
                                check = isWon(gameT, winTime);
                                if (check == true)
                                {
                                    state = WIN;
                                    direc = DONE;
                                }
                            }
                            // ELASTIC COLLISION
                            else if(person. x > 10)
                            {
                                direc = LEFT;
                            }
                            if (state == WIN)
                            {
                                break;
                            }
                            else
                            {
                                ADXL345_GetXyz(&x, &y, &z);
                                computeSpeed(x, y);
                            }
                        }

                        // ZOMBIE DISPLAY UPDATE
                        if ((ms_since_zomb_update >= 1200) && (state != LOSE))
                        {
                            zombieupdate = millisec;
                            OledSetCursor(zombie.x, zombie.y);
                            #ifdef USE_OLED_DRAW_GLYPH
                                        OledDrawGlyph(blank_char);
                            #else
                                        OledPutChar(blank_char);
                            #endif

                            // ALGORITHM FOR ZOMBIE TO FOLLOW HUMAN
                            if (person.x > zombie.x)
                                zombie.x++;
                            else if (person.x < zombie.x)
                                zombie.x--;
                            else
                            {
                                if (person.y > zombie.y)
                                    zombie.y++;
                                else
                                    zombie.y--;
                            }
                            OledSetCursor(zombie.x, zombie.y);
                            #ifdef USE_OLED_DRAW_GLYPH
                                       OledDrawGlyph(zomb_char);
                            #else
                                       OledPutChar(blank_char);
                            #endif
                                       updateScore();
                            check = isLost(&person, &zombie);
                            if (check == true)
                            {
                                state = LOSE;
                                direc = DONE;
                            }
                            if (state == LOSE)
                            {
                                break;
                            }
                            else
                            {
                                ADXL345_GetXyz(&x, &y, &z);
                                computeSpeed(x, y);
                            }
                        }
                        if ((state == WIN) || (state == LOSE))
                        {
                            state = state;
                            direc = DONE;
                        }
                        else
                            {
                                ADXL345_GetXyz(&x, &y, &z);
                                computeSpeed(x, y);
                            }

                    }

                    // FAST RIGHT MOVEMENT
                    while (direc == FRIGHT)
                    {
                        ms_since_last_update = millisec - lastupdate;
                        ms_since_zomb_update = millisec - zombieupdate;


                        if (person.y == zombie.y)
                        {
                            if ((person.x == 9) && (zombie.x == 11))
                            {
                                state = LOSE;
                                direc = DONE;
                            }
                            else if ((person.x == 11) && (zombie.x == 9))
                            {
                                state = LOSE;
                                direc = DONE;
                            }
                        }

                        if ((ms_since_last_update >= 400) && (state != DONE))
                        {
                            if (person.x < 11)
                            {
                                lastupdate = millisec;

                                OledSetCursor(person.x, person.y);
                                #ifdef USE_OLED_DRAW_GLYPH
                                            OledDrawGlyph(blank_char);
                                #else
                                            OledPutChar(blank_char);
                                #endif

                                // Update the glyph position
                                person.x++;

                                OledSetCursor(person.x, person.y);
                                #ifdef USE_OLED_DRAW_GLYPH
                                            OledDrawGlyph(hum_char);
                                            //OledUpdate();
                                #else
                                            OledPutChar(head_char);
                                #endif

                                updateScore();
                                check = isLost(&person, &zombie);
                                if (check == true)
                                {
                                    state = LOSE;
                                    direc = DONE;
                                }
                                check = isWon(gameT, winTime);
                                if (check == true)
                                {
                                    state = WIN;
                                    direc = DONE;
                                }
                            }

                            else if(person.x > 10)
                            {
                                direc = FLEFT;
                            }
                            if ((state == WIN) || (state == LOSE))
                            {
                                break;
                            }
                            else
                            {
                                ADXL345_GetXyz(&x, &y, &z);
                                computeSpeed(x, y);
                            }
                        }
                        if ((ms_since_zomb_update >= 1200) && (state != LOSE))
                        {
                            zombieupdate = millisec;
                            OledSetCursor(zombie.x, zombie.y);
                            #ifdef USE_OLED_DRAW_GLYPH
                                        OledDrawGlyph(blank_char);
                            #else
                                        OledPutChar(blank_char);
                            #endif

                            if (person.x > zombie.x)
                                zombie.x++;
                            else if (person.x < zombie.x)
                                zombie.x--;
                            else
                            {
                                if (person.y > zombie.y)
                                    zombie.y++;
                                else
                                    zombie.y--;
                            }

                            OledSetCursor(zombie.x, zombie.y);
                            #ifdef USE_OLED_DRAW_GLYPH
                                       OledDrawGlyph(zomb_char);
                            #else
                                       OledPutChar(blank_char);
                            #endif
                                       updateScore();
                            check = isLost(&person, &zombie);
                            if (check == true)
                            {
                                state = LOSE;
                                direc = DONE;
                            }
                            if (state == LOSE)
                            {
                                break;
                            }
                            else
                            {
                                ADXL345_GetXyz(&x, &y, &z);
                                computeSpeed(x, y);
                            }
                        }
                        if ((state == WIN) || (state == LOSE))
                        {
                            state = state;
                            direc = DONE;
                        }
                        else
                            {
                                ADXL345_GetXyz(&x, &y, &z);
                                computeSpeed(x, y);
                            }

                    }

                    // DISPLAY LEFT MOVEMENT
                    while (direc == LEFT)
                    {
                        ms_since_last_update = millisec - lastupdate;
                        ms_since_zomb_update = millisec - zombieupdate;

                        if (person.y == zombie.y)
                        {
                            if ((person.x == 2) && (zombie.x == 0))
                            {
                                state = LOSE;
                                direc = DONE;
                            }
                            else if ((person.x == 0) && (zombie.x == 2))
                            {
                                state = LOSE;
                                direc = DONE;
                            }
                        }
                        if ((ms_since_last_update >= 800) && (state != LOSE))
                        {
                            if (person.x > 0)
                            {
                                 lastupdate = millisec;

                                OledSetCursor(person.x, person.y);
                                #ifdef USE_OLED_DRAW_GLYPH
                                            OledDrawGlyph(blank_char);
                                #else
                                            OledPutChar(blank_char);
                                #endif

                                // Update the glyph position
                                person.x--;

                                OledSetCursor(person.x, person.y);
                                #ifdef USE_OLED_DRAW_GLYPH
                                            OledDrawGlyph(hum_char);
                                            //OledUpdate();
                                #else
                                            OledPutChar(head_char);
                                #endif

                                updateScore();
                                check = isLost(&person, &zombie);
                                if (check == true)
                                {
                                    state = LOSE;
                                    direc = DONE;
                                }
                                check = isWon(gameT, winTime);
                                if (check == true)
                                {
                                    state = WIN;
                                    direc = DONE;
                                }
                            }
                            else if (person.x < 1)
                            {
                                direc = RIGHT;
                            }
                            if ((state == WIN) || (state == LOSE))
                            {
                                break;
                            }
                            else
                            {
                                ADXL345_GetXyz(&x, &y, &z);
                                computeSpeed(x, y);
                            }
                        }

                        if ((ms_since_zomb_update >= 1200) && (state != LOSE))
                        {
                            zombieupdate = millisec;
                            OledSetCursor(zombie.x, zombie.y);
                            #ifdef USE_OLED_DRAW_GLYPH
                                        OledDrawGlyph(blank_char);
                            #else
                                        OledPutChar(blank_char);
                            #endif

                            if (person.x > zombie.x)
                                zombie.x++;
                            else if (person.x < zombie.x)
                                zombie.x--;
                            else
                            {
                                if (person.y > zombie.y)
                                    zombie.y++;
                                else
                                    zombie.y--;
                            }


                            OledSetCursor(zombie.x, zombie.y);
                            #ifdef USE_OLED_DRAW_GLYPH
                                       OledDrawGlyph(zomb_char);
                            #else
                                       OledPutChar(blank_char);
                            #endif
                                       updateScore();
                            check = isLost(&person, &zombie);
                            if (check == true)
                            {
                                state = LOSE;
                                direc = DONE;
                            }
                            if (state == LOSE)
                            {
                                break;
                            }
                            else
                            {
                                ADXL345_GetXyz(&x, &y, &z);
                                computeSpeed(x, y);
                            }
                        }
                        if ((state == WIN) || (state == LOSE))
                        {
                            state = state;
                            direc = DONE;
                        }
                        else
                            {
                                ADXL345_GetXyz(&x, &y, &z);
                                computeSpeed(x, y);
                            }

                        //}
                    }

                    // DISPLAY LEFT MOVEMENT
                    while (direc == FLEFT)
                    {
                        ms_since_last_update = millisec - lastupdate;
                        ms_since_zomb_update = millisec - zombieupdate;

                        if (person.y == zombie.y)
                        {
                            if ((person.x == 2) && (zombie.x == 0))
                            {
                                state = LOSE;
                                direc = DONE;
                            }
                            else if ((person.x == 0) && (zombie.x == 2))
                            {
                                state = LOSE;
                                direc = DONE;
                            }
                        }
                        if ((ms_since_last_update >= 400) && (state != LOSE))
                        {
                            if (person.x > 0)
                            {
                                lastupdate = millisec;

                                OledSetCursor(person.x, person.y);
                                #ifdef USE_OLED_DRAW_GLYPH
                                            OledDrawGlyph(blank_char);
                                #else
                                            OledPutChar(blank_char);
                                #endif

                                // Update the glyph position
                                person.x--;


                                OledSetCursor(person.x, person.y);
                                #ifdef USE_OLED_DRAW_GLYPH
                                            OledDrawGlyph(hum_char);
                                            //OledUpdate();
                                #else
                                            OledPutChar(head_char);
                                #endif

                                updateScore();
                                check = isLost(&person, &zombie);
                                if (check == true)
                                {
                                    state = LOSE;
                                    direc = DONE;
                                }
                                check = isWon(gameT, winTime);
                                if (check == true)
                                {
                                    state = WIN;
                                    direc = DONE;
                                }
                            }

                            else if (person.x < 1)
                            {
                                direc = FRIGHT;
                            }
                            if ((state == WIN) || (state == LOSE))
                            {
                                break;
                            }
                            else
                            {
                                ADXL345_GetXyz(&x, &y, &z);
                                computeSpeed(x, y);
                            }
                        }
                        if ((ms_since_zomb_update >= 1200) && (state != LOSE))
                        {
                            zombieupdate = millisec;
                            OledSetCursor(zombie.x, zombie.y);
                            #ifdef USE_OLED_DRAW_GLYPH
                                        OledDrawGlyph(blank_char);
                            #else
                                        OledPutChar(blank_char);
                            #endif

                            if (person.x > zombie.x)
                                zombie.x++;
                            else if (person.x < zombie.x)
                                zombie.x--;
                            else
                            {
                                if (person.y > zombie.y)
                                    zombie.y++;
                                else
                                    zombie.y--;
                            }

                            OledSetCursor(zombie.x, zombie.y);
                            #ifdef USE_OLED_DRAW_GLYPH
                                       OledDrawGlyph(zomb_char);
                            #else
                                       OledPutChar(blank_char);
                            #endif
                                       updateScore();
                            check = isLost(&person, &zombie);
                            if (check == true)
                            {
                                state = LOSE;
                                direc = DONE;
                            }
                            if (state == LOSE)
                            {
                                break;
                            }
                            else
                            {
                                ADXL345_GetXyz(&x, &y, &z);
                                computeSpeed(x, y);
                            }
                        }
                        if ((state == WIN) || (state == LOSE))
                        {
                            state = state;
                            direc = DONE;
                        }
                        else
                            {
                                ADXL345_GetXyz(&x, &y, &z);
                                computeSpeed(x, y);
                            }
                    }

                    // DISPLAY UP MOVEMENT
                    while (direc == UP)
                    {
                        ms_since_last_update = millisec - lastupdate;
                        ms_since_zomb_update = millisec - zombieupdate;

                        if (person.x == zombie.x)
                        {
                            if ((person.y == 1) && (zombie.y == 0))
                            {
                                state = LOSE;
                                direc = DONE;
                            }
                            else if ((person.y == 0) && (zombie.y == 1))
                            {
                                state = LOSE;
                                direc = DONE;
                            }
                        }
                        if ((ms_since_last_update >= 800) && (state != LOSE))
                        {
                            if (person.y > 0)
                            {
                                lastupdate = millisec;

                                OledSetCursor(person.x, person.y);
                                #ifdef USE_OLED_DRAW_GLYPH
                                            OledDrawGlyph(blank_char);
                                #else
                                            OledPutChar(blank_char);
                                #endif

                                // Update the glyph position
                                person.y--;

                                OledSetCursor(person.x, person.y);
                                #ifdef USE_OLED_DRAW_GLYPH
                                            OledDrawGlyph(hum_char);
                                            //OledUpdate();
                                #else
                                            OledPutChar(head_char);
                                #endif

                                updateScore();
                                check = isLost(&person, &zombie);
                                if (check == true)
                                {
                                    state = LOSE;
                                    direc = DONE;
                                }
                                check = isWon(gameT, winTime);
                                if (check == true)
                                {
                                    state = WIN;
                                    direc = DONE;
                                }
                            }
                            else if (person.y < 1)
                            {
                                direc = DOWN;
                            }
                            if ((state == WIN) || (state == LOSE))
                            {
                                break;
                            }
                            else
                            {
                                ADXL345_GetXyz(&x, &y, &z);
                                computeSpeed(x, y);
                            }
                        }
                        if ((ms_since_zomb_update >= 1200) && (state != LOSE))
                        {
                            zombieupdate = millisec;
                            OledSetCursor(zombie.x, zombie.y);
                            #ifdef USE_OLED_DRAW_GLYPH
                                        OledDrawGlyph(blank_char);
                            #else
                                        OledPutChar(blank_char);
                            #endif

                            if (person.y > zombie.y)
                                zombie.y++;
                            else if (person.y < zombie.y)
                                zombie.y--;
                            else
                            {
                                if (person.x > zombie.x)
                                    zombie.x++;
                                else
                                    zombie.x--;
                            }

                            OledSetCursor(zombie.x, zombie.y);
                            #ifdef USE_OLED_DRAW_GLYPH
                                       OledDrawGlyph(zomb_char);
                            #else
                                       OledPutChar(blank_char);
                            #endif
                                       updateScore();
                            check = isLost(&person, &zombie);
                            if (check == true)
                            {
                                state = LOSE;
                                direc = DONE;
                            }
                            if (state == LOSE)
                            {
                                break;
                            }
                            else
                            {
                                ADXL345_GetXyz(&x, &y, &z);
                                computeSpeed(x, y);
                            }
                        }
                        if ((state == WIN) || (state == LOSE))
                        {
                            state = state;
                            direc = DONE;
                        }
                        else
                            {
                                ADXL345_GetXyz(&x, &y, &z);
                                computeSpeed(x, y);
                            }
                    }

                    // DISPLAY FAST UP MOVEMENT
                    while (direc == FUP)
                    {
                        ms_since_last_update = millisec - lastupdate;
                        ms_since_zomb_update = millisec - zombieupdate;

                        if (person.x == zombie.x)
                        {
                            if ((person.y == 1) && (zombie.y == 0))
                            {
                                state = LOSE;
                                direc = DONE;
                            }
                            else if ((person.y == 0) && (zombie.y == 1))
                            {
                                state = LOSE;
                                direc = DONE;
                            }
                        }

                        if ((ms_since_last_update >= 400) && (state != LOSE))
                        {
                            if (person.y > 0)
                            {
                               lastupdate = millisec;

                                OledSetCursor(person.x, person.y);
                                #ifdef USE_OLED_DRAW_GLYPH
                                            OledDrawGlyph(blank_char);
                                #else
                                            OledPutChar(blank_char);
                                #endif

                                // Update the glyph position
                                person.y--;

                                OledSetCursor(person.x, person.y);
                                #ifdef USE_OLED_DRAW_GLYPH
                                            OledDrawGlyph(hum_char);
                                            //OledUpdate();
                                #else
                                            OledPutChar(head_char);
                                #endif

                                updateScore();
                                check = isLost(&person, &zombie);
                                if (check == true)
                                {
                                    state = LOSE;
                                    direc = DONE;
                                }
                                check = isWon(gameT, winTime);
                                if (check == true)
                                {
                                    state = WIN;
                                    direc = DONE;
                                }
                            }

                            else if (person.y < 1)
                            {
                                direc = FDOWN;
                            }
                            if ((state == WIN) || (state == LOSE))
                            {
                                break;
                            }
                            else
                            {
                                ADXL345_GetXyz(&x, &y, &z);
                                computeSpeed(x, y);
                            }
                        }
                        if ((ms_since_zomb_update >= 1200) && (state != LOSE))
                        {
                            zombieupdate = millisec;
                            OledSetCursor(zombie.x, zombie.y);
                            #ifdef USE_OLED_DRAW_GLYPH
                                        OledDrawGlyph(blank_char);
                            #else
                                        OledPutChar(blank_char);
                            #endif

                            if (person.y > zombie.y)
                                zombie.y++;
                            else if (person.y < zombie.y)
                                zombie.y--;
                            else
                            {
                                if (person.x > zombie.x)
                                    zombie.x++;
                                else
                                    zombie.x--;
                            }

                            OledSetCursor(zombie.x, zombie.y);
                            #ifdef USE_OLED_DRAW_GLYPH
                                       OledDrawGlyph(zomb_char);
                            #else
                                       OledPutChar(blank_char);
                            #endif
                                       updateScore();
                            check = isLost(&person, &zombie);
                            if (check == true)
                            {
                                state = LOSE;
                                direc = DONE;
                            }
                            if (state == LOSE)
                            {
                                break;
                            }
                            else
                            {
                                ADXL345_GetXyz(&x, &y, &z);
                                computeSpeed(x, y);
                            }
                        }
                        if ((state == WIN) || (state == LOSE))
                        {
                            state = state;
                            direc = DONE;
                        }
                        else
                            {
                                ADXL345_GetXyz(&x, &y, &z);
                                computeSpeed(x, y);
                            }
                    }

                    // DISPLAY DOWN MOVEMENT
                    while (direc == DOWN)
                    {
                        ms_since_last_update = millisec - lastupdate;
                        ms_since_zomb_update = millisec - zombieupdate;

                        if (person.x == zombie.x)
                        {
                            if ((person.y == 2) && (zombie.y == 3))
                            {
                                state = LOSE;
                                direc = DONE;

                            }
                            else if ((person.y == 3) && (zombie.y == 2))
                            {
                                state = LOSE;
                                direc = DONE;
                            }
                        }
                        if ((ms_since_last_update >= 800) && (state != DONE))
                        {

                            if (person.y < 3)
                            {
                                lastupdate = millisec;

                                OledSetCursor(person.x, person.y);
                                #ifdef USE_OLED_DRAW_GLYPH
                                            OledDrawGlyph(blank_char);
                                #else
                                            OledPutChar(blank_char);
                                #endif

                                // Update the glyph position
                                person.y++;

                                OledSetCursor(person.x, person.y);
                                #ifdef USE_OLED_DRAW_GLYPH
                                            OledDrawGlyph(hum_char);
                                            //OledUpdate();
                                #else
                                            OledPutChar(head_char);
                                #endif

                                updateScore();
                                check = isLost(&person, &zombie);
                                if (check == true)
                                {
                                    state = LOSE;
                                    direc = DONE;
                                }
                                check = isWon(gameT, winTime);
                                if (check == true)
                                {
                                    state = WIN;
                                    direc = DONE;
                                }
                            }

                            else if (person.y > 2)
                            {
                                direc = UP;
                            }
                            if ((state == WIN) || (state == LOSE))
                            {
                                break;
                            }
                            else
                            {
                                ADXL345_GetXyz(&x, &y, &z);
                                computeSpeed(x, y);
                            }
                        }
                        if ((ms_since_zomb_update >= 1200) && (state != LOSE))
                        {
                            zombieupdate = millisec;
                            OledSetCursor(zombie.x, zombie.y);
                            #ifdef USE_OLED_DRAW_GLYPH
                                        OledDrawGlyph(blank_char);
                            #else
                                        OledPutChar(blank_char);
                            #endif

                            if (person.y > zombie.y)
                                zombie.y++;
                            else if (person.y < zombie.y)
                                zombie.y--;
                            else
                            {
                                if (person.x > zombie.x)
                                    zombie.x++;
                                else
                                    zombie.x--;
                            }

                            OledSetCursor(zombie.x, zombie.y);
                            #ifdef USE_OLED_DRAW_GLYPH
                                       OledDrawGlyph(zomb_char);
                            #else
                                       OledPutChar(blank_char);
                            #endif
                                       updateScore();
                            check = isLost(&person, &zombie);
                            if (check == true)
                            {
                                state = LOSE;
                                direc = DONE;
                            }
                            if (state == LOSE)
                            {
                                break;
                            }
                            else
                            {
                                ADXL345_GetXyz(&x, &y, &z);
                                computeSpeed(x, y);
                            }
                        }
                        if ((state == WIN) || (state == LOSE))
                        {
                            state = state;
                            direc = DONE;
                        }
                        else
                            {
                                ADXL345_GetXyz(&x, &y, &z);
                                computeSpeed(x, y);
                            }
                    }

                    // DISPLAY FAST DOWN MOVEMENT
                    while (direc == FDOWN)
                    {
                        ms_since_last_update = millisec - lastupdate;
                        ms_since_zomb_update = millisec - zombieupdate;

                        if (person.x == zombie.x)
                        {
                            if ((person.y == 2) && (zombie.y == 3))
                            {
                                state = LOSE;
                                direc = DONE;

                            }
                            else if ((person.y == 3) && (zombie.y == 2))
                            {
                                state = LOSE;
                                direc = DONE;
                            }
                        }
                        if ((ms_since_last_update >= 400) && (state != DONE))
                        {
                            if (person.y < 3)
                            {
                                lastupdate = millisec;

                                OledSetCursor(person.x, person.y);
                                #ifdef USE_OLED_DRAW_GLYPH
                                            OledDrawGlyph(blank_char);
                                #else
                                            OledPutChar(blank_char);
                                #endif

                                // Update the glyph position
                                person.y++;

                                OledSetCursor(person.x, person.y);
                                #ifdef USE_OLED_DRAW_GLYPH
                                            OledDrawGlyph(hum_char);
                                #else
                                            OledPutChar(head_char);
                                #endif

                                updateScore();
                                check = isLost(&person, &zombie);
                                if (check == true)
                                {
                                    state = LOSE;
                                    direc = DONE;
                                }
                                check = isWon(gameT, winTime);
                                if (check == true)
                                {
                                    state = WIN;
                                    direc = DONE;
                                }
                            }
                            else if (person.y > 2)
                            {
                                direc = FUP;
                            }
                            if ((state == WIN) || (state == LOSE))
                            {
                                break;
                            }
                            else
                            {
                                ADXL345_GetXyz(&x, &y, &z);
                                computeSpeed(x, y);
                            }
                        }
                        if ((ms_since_zomb_update >= 1200) && (state != LOSE))
                        {
                            zombieupdate = millisec;
                            OledSetCursor(zombie.x, zombie.y);
                            #ifdef USE_OLED_DRAW_GLYPH
                                        OledDrawGlyph(blank_char);
                            #else
                                        OledPutChar(blank_char);
                            #endif

                            if (person.y > zombie.y)
                                zombie.y++;
                            else if (person.y < zombie.y)
                                zombie.y--;
                            else
                            {
                                if (person.x > zombie.x)
                                    zombie.x++;
                                else
                                    zombie.x--;
                            }

                            OledSetCursor(zombie.x, zombie.y);
                            #ifdef USE_OLED_DRAW_GLYPH
                                       OledDrawGlyph(zomb_char);
                            #else
                                       OledPutChar(blank_char);
                            #endif
                                       updateScore();
                            check = isLost(&person, &zombie);
                            if (check == true)
                            {
                                state = LOSE;
                                direc = DONE;
                            }
                            if (state == LOSE)
                            {
                                break;
                            }
                            else
                            {
                                ADXL345_GetXyz(&x, &y, &z);
                                computeSpeed(x, y);
                            }
                        }
                        if ((state == WIN) || (state == LOSE))
                        {
                            state = state;
                            direc = DONE;
                        }
                        else
                            {
                                ADXL345_GetXyz(&x, &y, &z);
                                computeSpeed(x, y);
                            }
                    }
                    OledClearBuffer();

                }
                if (direc == DONE)
                {
                    state = state;
                }
                else
                {
                    state = DISPLAY;
                }

                break;
            case LOSE:
                // LOSING SCREEN
                OledClearBuffer();
                OledSetCursor(4, 0);
                OledPutString("YOU LOSE");
                OledSetCursor(4, 1);
                sprintf(oledstring, "Score: %u", gameT);
                OledPutString(oledstring);
                
                zombie.x = 0;
                zombie.y = 2;

                OledSetCursor(zombie.x, zombie.y);
                #ifdef USE_OLED_DRAW_GLYPH
                    OledDrawGlyph(zomb_char);
                #else
                    OledPutChar(apple_char);
                #endif
                    OledUpdate();

                while (BTN3Hist != 0xFF)
                {

                ms_since_last_update = millisec - lastupdate;

                if (ms_since_last_update >= 100)
                {
                    lastupdate = millisec;
                     OledSetCursor(zombie.x, zombie.y);
                #ifdef USE_OLED_DRAW_GLYPH
                    OledDrawGlyph(blank_char);
                #else
                    OledPutChar(apple_char);
                #endif

                    if (zombie.x == 14)
                    {
                        zombie.x = 0;

                        if (zombie.y == 3)
                            zombie.y = 2;
                        else
                            zombie.y++;
                    }
                    else
                        zombie.x++;
                OledSetCursor(zombie.x, zombie.y);
                #ifdef USE_OLED_DRAW_GLYPH
                    OledDrawGlyph(zomb_char);
                    //OledUpdate();
                #else
                    OledPutChar(apple_char);
                #endif
                    OledUpdate();
                }
                }

                if (BTN3Hist == 0xFF) //button 3 statrs game
                {
                    OledClearBuffer();
                    state = OPTIONS;
                    direc = LEFT;
                    //gameStart = millisec;
                    BTN3Hist = 0x00;
                }
                break;

            case WIN:
                // PRINT WIN SCREN
                OledClearBuffer();
                OledSetCursor(4, 0);
                OledPutString("YOU WIN");
                OledSetCursor(3, 1);
                sprintf(oledstring, "Score: %u", gameT);
                OledPutString(oledstring);
                
                person.x = 0;
                person.y = 2;

                while (BTN3Hist != 0xFF)
                {

                ms_since_last_update = millisec - lastupdate;

                if (ms_since_last_update >= 100)
                {
                    lastupdate = millisec;
                     OledSetCursor(person.x, person.y);
                #ifdef USE_OLED_DRAW_GLYPH
                    OledDrawGlyph(blank_char);
                #else
                    OledPutChar(zomb_char);
                #endif

                    if (person.x == 14)
                    {
                        person.x = 0;

                        if (person.y == 3)
                            person.y = 2;
                        else
                            person.y++;
                    }
                    else
                        person.x++;
                OledSetCursor(person.x, person.y);
                #ifdef USE_OLED_DRAW_GLYPH
                    OledDrawGlyph(hum_char);
                #else
                    OledPutChar(hum_char);
                #endif
                    OledUpdate();
                }
                }


                if (BTN3Hist == 0xFF) //button 3 statrs game
                {
                    OledClearBuffer();
                    state = OPTIONS;
                    direc = LEFT;
                    //gameStart = millisec;
                    BTN3Hist = 0x00;
                }
                break;

        }
    }

    return (1);
}


void updateScore()
{
    char oledstring[17];

    //int
    gameT = millisec - gameStart;

    gameT = gameT / 1000;

    OledSetCursor(12,0);
    OledPutString("|");
    OledSetCursor(13, 0);
    OledPutString("Sco");
    OledSetCursor(12,1);
    OledPutString("|");
    OledSetCursor(13,1);
    OledPutString("re:");
    OledSetCursor(13,2);
    sprintf(oledstring, "%u", gameT);
    OledPutString(oledstring);
    OledSetCursor(12,2);
    OledPutString("|");
    OledSetCursor(12,3);
    OledPutString("|");

    OledUpdate();
}