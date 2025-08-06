#include <zephyr/kernel.h>
#include <zephyr/input/input.h>
#include <zephyr/sys/printk.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zephyr/drivers/adc.h>

static void joystick_input_cb( struct input_event *evt, void *user_data)
{
    printk("Event type %d code %d value %d\n", evt->type, evt->code, evt->value);

    if (evt->type == INPUT_EV_ABS) {
        if (evt->code == INPUT_ABS_X) {
            printk("X Axis: %d\n", evt->value);
        } else if (evt->code == INPUT_ABS_Y) {
            printk("Y Axis: %d\n", evt->value);
        }
    } else if (evt->type == INPUT_EV_KEY) {
        printk("Button code %d: %s\n",
               evt->code,
               evt->value ? "Pressed" : "Released");
    }
}


INPUT_CALLBACK_DEFINE(NULL, joystick_input_cb, NULL);

int main(void)
{
    printk("Joystick input active\n");
    while (1) {
        k_msleep(200);
    }
    return 0;
}
