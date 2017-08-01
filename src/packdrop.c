/*
 * Copyright (c) 2016,2017 YAMAMOTO Masaya, KLab Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <wiringPi.h>
#include <wiringPiSPI.h>
#include <mcp3002.h>

#define APP_NAME     "PACKDROP"
#define APP_VERSION  "v1.2.1"

#define SPI_CHANNEL  (0)
#define PINBASE      (100)
#define VOLUME_DELAY (PINBASE)
#define VOLUME_LOSS  (PINBASE+1)
#define SW_ON        (0)
#define PIN_LED      (17)
#define PIN_SW       (22)

char **devices;
int devnum;

static void
tc_set (const char *dev, int delay, int loss) {
    char cmd[128];

    snprintf(cmd, sizeof(cmd), "tc qdisc change dev %s root netem delay %dms loss %d%%", dev, delay, loss);
    system(cmd);
}

static void
tc_init (const char *dev) {
    char cmd[128];

    snprintf(cmd, sizeof(cmd), "tc qdisc del dev %s root >/dev/null 2>&1", dev);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "tc qdisc add dev %s root netem delay 0ms loss 0%%", dev);
    system(cmd);
}

static void
lcd_write_core (const char *upper, const char *lower) {
    char cmd[128];

    system("i2cset -y 1 0x3e 0 0x80 b");
    snprintf(cmd, sizeof(cmd), "i2cset -y 1 0x3e 0x40 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x i",
        upper[0], upper[1], upper[2], upper[3], upper[4], upper[5], upper[6], upper[7]);
    system(cmd);

    system("i2cset -y 1 0x3e 0 0xc0 b");
    snprintf(cmd, sizeof(cmd), "i2cset -y 1 0x3e 0x40 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x i",
        lower[0], lower[1], lower[2], lower[3], lower[4], lower[5], lower[6], lower[7]);
    system(cmd);
}

static void
lcd_write (int delay, int loss) {
    char upper[9], lower[9];

    snprintf(upper, sizeof(upper), " %4d ms", delay);
    snprintf(lower, sizeof(lower), "  %3d %% ", loss);
    lcd_write_core(upper, lower);
}

static void
lcd_init (void) {
    char name[9], ver[9];

    system("i2cset -y 1 0x3e 0 0x38 0x39 0x14 0x78 0x5f 0x6a i");
    system("i2cset -y 1 0x3e 0 0x0c 0x01 i");
    system("i2cset -y 1 0x3e 0 0x06 i");
    snprintf(name, sizeof(name), "%8s", APP_NAME);
    snprintf(ver, sizeof(ver), "%8s", APP_VERSION);
    lcd_write_core(name, ver);
}

static void
burst_mode (void) {
    int n, v;

    for (n = 0; n < devnum; n++) {
        tc_set(devices[n], 0, 100);
    }
    lcd_write_core(" burst  ", "   mode ");
    digitalWrite(PIN_LED, 1);
    while (1) {
        v  = digitalRead(PIN_SW);
        if (v != SW_ON) {
            break;
        }
        sleep(1);
    }
}

int
main (int argc, char *argv[]){
    int n, v, b, d, l;
    int delay = 0, loss = 0;

    lcd_init();
    if (wiringPiSetupGpio() == -1) {
        printf("wiringPiSetupGpio: failed\n");
        return -1;
    }
    pinMode(PIN_LED, OUTPUT);
    pinMode(PIN_SW, INPUT);
    pullUpDnControl(PIN_SW, PUD_UP);
    if (mcp3002Setup(PINBASE, SPI_CHANNEL) < 0) {
        printf("mpc3002Setup: failed\n");
        return -1;
    }
    devices = argv + 1;
    devnum  = argc - 1;
    for (n = 0; n < devnum; n++) {
        tc_init(devices[n]);
    }
    while (1) {
        v = digitalRead(PIN_SW);
        b = (v == SW_ON) ? 1 : 0;
        if (b) {
            burst_mode();
        }
        v = analogRead(VOLUME_DELAY);
        d = ++v / 2;
        v = analogRead(VOLUME_LOSS);
        l = ((float)++v / 1024) * 100;
        if (d != delay || l != loss || b) {
            delay = d;
            loss  = l;
            for (n = 0; n < devnum; n++) {
                tc_set(devices[n], delay, loss);
            }
            lcd_write(delay, loss);
            digitalWrite(PIN_LED, (delay || loss) ? 1 : 0);
        }
        sleep(1);
    }
    return 0;
}
