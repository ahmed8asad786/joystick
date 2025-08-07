#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main);

#define DT_SPEC_AND_COMMA(node_id, prop, idx) \
	ADC_DT_SPEC_GET_BY_IDX(node_id, idx),

static const struct adc_dt_spec adc_channels[] = {
	DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), io_channels,
			     DT_SPEC_AND_COMMA)
};

#define DISPLAY_WIDTH      256
#define DISPLAY_HEIGHT     120
#define DISPLAY_PITCH      DISPLAY_WIDTH
#define DISPLAY_BYTE_PITCH ((DISPLAY_WIDTH + 7) / 8)
#define DISPLAY_BUF_SIZE   (DISPLAY_BYTE_PITCH * DISPLAY_HEIGHT)

#define PIXEL_ON 0
#define PIXEL_OFF 1

static uint8_t raw_buf[DISPLAY_WIDTH * DISPLAY_HEIGHT];  
static uint8_t disp_buf[DISPLAY_BUF_SIZE];               
void set_pixel(int x, int y) {
   if (x < 0 || x >= DISPLAY_WIDTH || y < 0 || y >= DISPLAY_HEIGHT) return;
   raw_buf[y * DISPLAY_WIDTH + x] = PIXEL_ON;
}

static void pack_buffer(void) {
   memset(disp_buf, 0xFF, sizeof(disp_buf));  // Set all to white
   for (int y = 0; y < DISPLAY_HEIGHT; y++) {
       for (int x = 0; x < DISPLAY_WIDTH; x++) {
           if (raw_buf[y * DISPLAY_WIDTH + x] == PIXEL_ON) {
               int byte_index = x + (y / 8) * DISPLAY_WIDTH;
               int bit_pos = 7 - (y % 8);
               disp_buf[byte_index] &= ~(1 << bit_pos);
           }
       }
   }
}

void draw_square_5x5(int x_start, int y_start) {
    for (int y = y_start; y < y_start + 10; y++) {
        for (int x = x_start; x < x_start + 10; x++) {
            set_pixel(x, y);
        }
    }
}

int main(void)
{
    const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    if (!device_is_ready(display_dev)) {
        LOG_ERR("Display not ready");
        return 1;
    }

    const struct device *reset_dev = device_get_binding("GPIO_1");
    if (reset_dev) {
        gpio_pin_configure(reset_dev, 3, GPIO_OUTPUT);
        gpio_pin_set(reset_dev, 3, 0);
        k_sleep(K_MSEC(10));
        gpio_pin_set(reset_dev, 3, 1);
        k_sleep(K_MSEC(10));
    }

    int err;
    uint32_t count = 0;
    uint16_t adc_buf;  
    struct adc_sequence sequence = {
        .buffer = &adc_buf,
        .buffer_size = sizeof(adc_buf),
    };

    struct display_buffer_descriptor desc = {
        .width = DISPLAY_WIDTH,
        .height = DISPLAY_HEIGHT,
        .pitch = DISPLAY_PITCH,
        .buf_size = DISPLAY_BUF_SIZE,
    };

    for (size_t i = 0U; i < ARRAY_SIZE(adc_channels); i++) {
        if (!adc_is_ready_dt(&adc_channels[i])) {
            printk("ADC controller device %s not ready\n", adc_channels[i].dev->name);
            return 0;
        }

        err = adc_channel_setup_dt(&adc_channels[i]);
        if (err < 0) {
            printk("Could not setup channel #%d (%d)\n", i, err);
            return 0;
        }
    }
	    memset(raw_buf, PIXEL_OFF, sizeof(raw_buf));

    while (1) {
        printk("ADC reading[%u]:\n", count++);
        int32_t y_mv = 0;
        int32_t x_mv = 0;
		    memset(raw_buf, PIXEL_OFF, sizeof(raw_buf));
        for (size_t i = 0U; i < ARRAY_SIZE(adc_channels); i++) {
            int32_t val_mv;

            (void)adc_sequence_init_dt(&adc_channels[i], &sequence);
            err = adc_read_dt(&adc_channels[i], &sequence);
            if (err < 0) {
                printk("Could not read (%d)\n", err);
                continue;
            }

            val_mv = adc_channels[i].channel_cfg.differential
                   ? (int32_t)((int16_t)adc_buf)
                   : (int32_t)adc_buf;

            err = adc_raw_to_millivolts_dt(&adc_channels[i], &val_mv);

            if (i == 0) {
                y_mv = val_mv;
            } else if (i == 1) {
                x_mv = val_mv;
            }
        }

        printk("Joystick Position: x = %d mV, y = %d mV\n", x_mv, y_mv);

        if (y_mv > 2500) printk("Y: POSITIVE\n");
        else if (y_mv < 1500) printk("Y: NEGATIVE\n");
        else printk("Y: CENTERED\n");

        if (x_mv > 2500) printk("X: POSITIVE\n");
        else if (x_mv < 1500) printk("X: NEGATIVE\n");
        else printk("X: CENTERED\n");

		//conversions into dimensions

        int x_disp = x_mv * 250 / 3000;
        int y_disp = y_mv * 120 / 3000;

        // prevent out of bounds
        if (x_disp > DISPLAY_WIDTH - 10) x_disp = DISPLAY_WIDTH - 10;
        if (y_disp > DISPLAY_HEIGHT - 10) y_disp = DISPLAY_HEIGHT - 10;

        draw_square_5x5(x_disp, y_disp);
        pack_buffer();
        display_write(display_dev, 0, 0, &desc, disp_buf);

        k_sleep(K_MSEC(1));
    }

    return 0;
}
