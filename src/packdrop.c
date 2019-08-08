#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <wiringPi.h>
#include <wiringPiSPI.h>

#define APP_NAME     "PACKDROP"
#define APP_VERSION  "v2.0a"

#define PIN_LED (27)
#define PIN_SW  (22)
#define PIN_D_A (5)
#define PIN_D_B (6)
#define PIN_L_A (26)
#define PIN_L_B (16)

#define SW_ON   (0)

struct {
    int a;
    int b;
    struct {
        int a;
        int b;
    } p;
    int num;
    int min;
    int max;
} d = {1, 1, {0, 0}, 0, 0, 10000}, l = {1, 1, {0, 0}, 0, 0, 100};

struct {
    int num;
} s = {!SW_ON};

char **devices;
int devnum;

static void
tc_set (const char *dev, int delay, int loss) {
    char cmd[128];

    snprintf(cmd, sizeof(cmd), "tc qdisc change dev %s root netem limit 10000 delay %dms loss %d%%", dev, delay, loss);
    system(cmd);
}

static void
tc_init (const char *dev) {
    char cmd[128];

    snprintf(cmd, sizeof(cmd), "tc qdisc del dev %s root >/dev/null 2>&1", dev);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "tc qdisc add dev %s root netem limit 10000 delay 0ms loss 0%%", dev);
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

    snprintf(upper, sizeof(upper), "%5d ms", delay);
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
delay_inc (void) {
    int v;

    if (s.num == SW_ON) {
        return;
    }
    v = digitalRead(PIN_D_B);
    if (v != d.b) {
        d.p.b = d.b; d.b = v;
        if (!d.a && !d.b && d.p.a && d.p.b) {
            if (d.num < d.max) {
                d.num += d.num < 100 ? 1 : (d.num < 1000 ? 10 : 100);
            }
        }
    }
}

static void
delay_dec (void) {
    int v;

    if (s.num == SW_ON) {
        return;
    }
    v = digitalRead(PIN_D_A);
    if (v != d.a) {
        d.p.a = d.a; d.a = v;
        if (!d.a && !d.b && d.p.a && d.p.b) {
            if (d.num > d.min) {
                d.num -= d.num <= 100 ? 1 : (d.num <= 1000 ? 10 : 100);
            }
        }
    }
}

static void
loss_inc (void) {
    int v;

    if (s.num == SW_ON) {
        return;
    }
    v = digitalRead(PIN_L_B);
    if (v != l.b) {
        l.p.b = l.b; l.b = v;
        if (!l.a && !l.b && l.p.a && l.p.b) {
            if (l.num < l.max) {
                l.num += 1;
            }
        }
    }
}

void
loss_dec (void) {
    int v;

    if (s.num == SW_ON) {
        return;
    }
    v = digitalRead(PIN_L_A);
    if (v != l.a) {
        l.p.a = l.a; l.a = v;
        if (!l.a && !l.b && l.p.a && l.p.b) {
            if (l.num > l.min) {
                l.num -= 1;
            }
        }
    }
}

static void
sw_event (void) {
    s.num = digitalRead(PIN_SW);
}

static void
burst_mode (void) {
    int n;

    for (n = 0; n < devnum; n++) {
        tc_set(devices[n], 0, 100);
    }
    lcd_write_core(" burst  ", "   mode ");
    digitalWrite(PIN_LED, 1);
    while (1) {
        if (s.num != SW_ON) {
            break;
        }
        usleep(10000);
    }
}

int
main (int argc, char *argv[]) {
    int n, delay = 0, loss = 0;

    lcd_init();
    if (wiringPiSetupGpio() == -1) {
        fprintf(stderr, "wiringPiSetup: error\n");
        return -1;
    }
    /* LED */
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, 0);
    /* SW */
    pinMode(PIN_SW, INPUT);
    pullUpDnControl(PIN_SW, PUD_UP);
    wiringPiISR(PIN_SW, INT_EDGE_BOTH, sw_event);
    /* ENC (DELAY) */
    pinMode(PIN_D_A, INPUT);
    pinMode(PIN_D_B, INPUT);
    pullUpDnControl(PIN_D_A, PUD_UP);
    pullUpDnControl(PIN_D_B, PUD_UP);
    wiringPiISR(PIN_D_A, INT_EDGE_BOTH, delay_dec);
    wiringPiISR(PIN_D_B, INT_EDGE_BOTH, delay_inc);
    /* ENC (LOSS) */
    pinMode(PIN_L_A, INPUT);
    pinMode(PIN_L_B, INPUT);
    pullUpDnControl(PIN_L_A, PUD_UP);
    pullUpDnControl(PIN_L_B, PUD_UP);
    wiringPiISR(PIN_L_A, INT_EDGE_BOTH, loss_dec);
    wiringPiISR(PIN_L_B, INT_EDGE_BOTH, loss_inc);
    devices = argv + 1;
    devnum  = argc - 1;
    for (n = 0; n < devnum; n++) {
        tc_init(devices[n]);
    }
    while (1){
	if (s.num == SW_ON) {
            burst_mode();
            delay = loss = -1;
        }
        if (delay != d.num || loss != l.num) {
            delay = d.num;
            loss = l.num;
            for (n = 0; n < devnum; n++) {
                tc_set(devices[n], delay, loss);
            }
            lcd_write(delay, loss);
	    digitalWrite(PIN_LED, (delay || loss) ? 1 : 0);
        }
        usleep(10000);
    }
    return 0;
}
