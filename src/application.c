#include <application.h>

#define REPORT_INTERVAL (5 * 60 * 1000)
#define REPORT_INTERVAL_INITIAL (1 * 60 * 1000)
#define BATTERY_UPDATE_INTERVAL (60 * 60 * 1000)

#define TEMPERATURE_PUB_NO_CHANGE_INTEVAL (15 * 60 * 1000)
#define TEMPERATURE_PUB_VALUE_CHANGE 0.2f
#define TEMPERATURE_UPDATE_INTERVAL (1 * 1000)

typedef struct
{
    uint8_t channel;
    float value;
    twr_tick_t next_pub;

} event_param_t;

static event_param_t temperature_event_param = { .next_pub = 0 };

static struct
{
    twr_led_t led;
    twr_button_t button;
    float temperature;
    twr_tag_temperature_t temperature_tag;
    twr_button_t button_channel_b;
    uint32_t button_channel_b_count;

} app;

void pulse_counter_event_handler(twr_module_sensor_channel_t channel, twr_pulse_counter_event_t event, void *event_param);

void battery_event_handler(twr_module_battery_event_t event, void *event_param);

void button_event_handler(twr_button_t *self, twr_button_event_t event, void *event_param);

void temperature_tag_event_handler(twr_tag_temperature_t *self, twr_tag_temperature_event_t event, void *event_param);

void button_channel_b_event_handler(twr_button_t *self, twr_button_event_t event, void *event_param);

void application_init(void)
{
	twr_led_init(&app.led, TWR_GPIO_LED, false, false);
	twr_led_set_mode(&app.led, TWR_LED_MODE_OFF);

	twr_pulse_counter_init(TWR_MODULE_SENSOR_CHANNEL_A, TWR_PULSE_COUNTER_EDGE_FALL);
	twr_pulse_counter_set_event_handler(TWR_MODULE_SENSOR_CHANNEL_A, pulse_counter_event_handler, NULL);

    twr_module_sensor_set_mode(TWR_MODULE_SENSOR_CHANNEL_B, TWR_MODULE_SENSOR_MODE_INPUT);
    twr_module_sensor_set_pull(TWR_MODULE_SENSOR_CHANNEL_B, TWR_MODULE_SENSOR_PULL_NONE);
	twr_button_init(&app.button_channel_b, TWR_GPIO_P5, TWR_GPIO_PULL_UP, 1);
	twr_button_set_event_handler(&app.button_channel_b, button_channel_b_event_handler, NULL);

    // Initialize temperature
    temperature_event_param.channel = TWR_RADIO_PUB_CHANNEL_R1_I2C0_ADDRESS_ALTERNATE;
    twr_tag_temperature_init(&app.temperature_tag, TWR_I2C_I2C0, TWR_TAG_TEMPERATURE_I2C_ADDRESS_ALTERNATE);
    twr_tag_temperature_set_update_interval(&app.temperature_tag, TEMPERATURE_UPDATE_INTERVAL);
    twr_tag_temperature_set_event_handler(&app.temperature_tag, temperature_tag_event_handler, &temperature_event_param);

    // Initialize battery
    twr_module_battery_init();
    twr_module_battery_set_event_handler(battery_event_handler, NULL);
    twr_module_battery_set_update_interval(BATTERY_UPDATE_INTERVAL);

    twr_button_init(&app.button, TWR_GPIO_BUTTON, TWR_GPIO_PULL_DOWN, false);
    twr_button_set_event_handler(&app.button, button_event_handler, NULL);

    twr_radio_init(TWR_RADIO_MODE_NODE_SLEEPING);

    twr_scheduler_plan_relative(0, 3000);

    twr_radio_pairing_request("radio-pulse-counter", VERSION);

    twr_led_pulse(&app.led, 2000);
}

void application_task(void *param)
{
	(void) param;

    uint32_t channel_count_a;

    channel_count_a = twr_pulse_counter_get(TWR_MODULE_SENSOR_CHANNEL_A);

    twr_radio_pub_uint32("pulse-counter/a/count", &channel_count_a);

    twr_scheduler_plan_current_relative(REPORT_INTERVAL);
}

void pulse_counter_event_handler(twr_module_sensor_channel_t channel, twr_pulse_counter_event_t event, void *event_param)
{
    (void) channel;
    (void) event_param;

    if (event == TWR_PULSE_COUNTER_EVENT_UPDATE)
    {
        twr_led_pulse(&app.led, 20);
    }
    else if (event == TWR_PULSE_COUNTER_EVENT_OVERFLOW)
    {
        // TODO
    }
}

// This function dispatches battery events
void battery_event_handler(twr_module_battery_event_t event, void *event_param)
{
    // Update event?
    if (event == TWR_MODULE_BATTERY_EVENT_UPDATE)
    {
        float voltage;

        // Read battery voltage
        if (twr_module_battery_get_voltage(&voltage))
        {
            // Publish battery voltage
            twr_radio_pub_battery(&voltage);
        }
    }
}

void button_event_handler(twr_button_t *self, twr_button_event_t event, void *event_param)
{
    (void) self;
    (void) event_param;

    if (event == TWR_BUTTON_EVENT_CLICK)
    {
        twr_led_pulse(&app.led, 100);

        static uint16_t event_count = 0;

        event_count++;

        twr_radio_pub_push_button(&event_count);
    }
    else if (event == TWR_BUTTON_EVENT_HOLD)
    {
        twr_scheduler_plan_now(0);
    }
}

void temperature_tag_event_handler(twr_tag_temperature_t *self, twr_tag_temperature_event_t event, void *event_param)
{
    float value;
    event_param_t *param = (event_param_t *)event_param;

    if (event == TWR_TAG_TEMPERATURE_EVENT_UPDATE)
    {
        if (twr_tag_temperature_get_temperature_celsius(self, &value))
        {
            if ((fabs(value - param->value) >= TEMPERATURE_PUB_VALUE_CHANGE) || (param->next_pub < twr_scheduler_get_spin_tick()))
            {
                twr_radio_pub_temperature(param->channel, &value);

                param->value = value;
                param->next_pub = twr_scheduler_get_spin_tick() + TEMPERATURE_PUB_NO_CHANGE_INTEVAL;
            }
        }
    }
}

void button_channel_b_event_handler(twr_button_t *self, twr_button_event_t event, void *event_param)
{
    if (event == TWR_BUTTON_EVENT_CLICK)
    {
        twr_led_pulse(&app.led, 100);

        app.button_channel_b_count++;

        twr_radio_pub_uint32("push-button/b/event_count", &app.button_channel_b_count);
    }
    else if (event == TWR_BUTTON_EVENT_HOLD)
    {
        twr_scheduler_plan_now(0);
    }
}
