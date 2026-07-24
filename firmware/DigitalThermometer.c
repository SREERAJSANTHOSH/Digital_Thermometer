#include <reg51.h>

/* LCD control pins (8-bit data bus on Port 3). */
sbit LCD_RS = P2^5;
sbit LCD_RW = P2^6;
sbit LCD_EN = P2^7;

/* ADC0808 control pins (8-bit result on Port 1). */
sbit ADC_ALE   = P2^3;
sbit ADC_OE    = P2^4;
sbit ADC_START = P2^1;
sbit ADC_EOC   = P2^0;
sbit ADC_CLK   = P2^2;
sbit ADC_CH_C  = P0^7;
sbit ADC_CH_B  = P0^6;
sbit ADC_CH_A  = P0^5;

/*
 * Hardware configuration.
 *
 * Measure the real ADC reference and oscillator before deployment. A 2.560 V
 * reference gives approximately 10 mV per ADC count, closely matching the
 * LM35 response of 10 mV per degree Celsius.
 */
#define ADC_VREF_MV                    2560UL
#define OSCILLATOR_HZ              11059200UL
#define TIMER0_RELOAD                   0xD4U
#define MEASUREMENT_WINDOW_SECONDS          4U
#define TICK_PRESCALER                      8U
#define TIMER0_OVERFLOWS_PER_SECOND \
    (OSCILLATOR_HZ / 12UL / (256UL - TIMER0_RELOAD))
#define MEASUREMENT_WINDOW_TICKS \
    ((unsigned int)(TIMER0_OVERFLOWS_PER_SECOND * \
                    MEASUREMENT_WINDOW_SECONDS / TICK_PRESCALER))

/* Smart measurement configuration. */
#define SAMPLE_COUNT                       16U
#define TRIMMED_SAMPLE_COUNT               14U
#define TREND_THRESHOLD_X10                10U
#define MAX_VALID_TEMP_X10               1500U

/* Thermal history and forecast configuration. */
#define SPARK_LENGTH                       16U
#define MIN_FORECAST_POINTS                 4U
#define FORECAST_HORIZON_WINDOWS           15L
#define MAX_FORECAST_CHANGE_X10            200L

/* Time-to-threshold, contact detection, and dead-reckoning configuration. */
#define ALERT_THRESHOLD_X10               370U
#define ETA_INVALID                    0xFFFFU
#define MAX_ETA_SECONDS                  5940U
#define CONTACT_RISE_X10                   15U
#define DEAD_RECKON_WINDOWS                 5U

#define TREND_STABLE                        0U
#define TREND_RISING                        1U
#define TREND_FALLING                       2U

#define QUALITY_HIGH                        0U
#define QUALITY_MEDIUM                      1U
#define QUALITY_LOW                         2U

/* Eight HD44780 CGRAM characters: one through eight illuminated rows. */
unsigned char code bar_glyphs[8][8] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x1F},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x1F, 0x1F},
    {0x00, 0x00, 0x00, 0x00, 0x1F, 0x1F, 0x1F, 0x1F},
    {0x00, 0x00, 0x00, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F},
    {0x00, 0x00, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F},
    {0x00, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F},
    {0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F}
};

volatile unsigned int timer0_ticks = 0U;
unsigned char data isr_prescale = 0U;
unsigned int temperature_history[SPARK_LENGTH];
unsigned char history_count = 0U;

/* Fitted least-squares slope, cached for ETA and fault-time estimation. */
signed long slope_numerator = 0L;
signed long slope_denominator = 1L;
bit slope_valid = 0;

void delay(unsigned int ticks);
void lcd_init(void);
void lcd_command(unsigned char command);
void lcd_data(unsigned char value);
void lcd_print(const char *text);
void lcd_clear_line(unsigned char address);
void lcd_load_bar_glyphs(void);
void lcd_print_unsigned(unsigned int value);
void lcd_print_two_digits(unsigned int value);
void lcd_print_temperature(unsigned int temperature_x10);
void lcd_show_dashboard(unsigned int temperature_x10,
                        unsigned int forecast_x10,
                        unsigned char quality,
                        unsigned char trend,
                        unsigned char contact);
void lcd_show_eta_page(unsigned int eta_seconds);
void lcd_show_fault(void);
void lcd_show_fault_with_estimate(unsigned int estimate_x10,
                                  unsigned char windows_remaining);

void adc_init(void);
unsigned char adc_read_channel_zero(void);
unsigned char adc_read_trimmed_mean(unsigned char *spread);
unsigned int adc_to_temperature_x10(unsigned char adc_count);
unsigned char classify_quality(unsigned char spread);
unsigned char classify_trend(unsigned int current_x10,
                             unsigned int previous_x10);

void history_reset(void);
void history_push(unsigned int temperature_x10);
unsigned int forecast_temperature_x10(void);
unsigned int eta_to_threshold_seconds(unsigned int current_x10);
unsigned char detect_contact(void);
unsigned int dead_reckon_estimate_x10(unsigned int last_valid_x10,
                                      unsigned char windows_elapsed);
void sparkline_render(void);
unsigned int timer0_snapshot(void);
void wait_for_next_measurement_window(void);

/*
 * Timer 0 supplies an approximately 10.5 kHz ADC clock and the measurement
 * timebase. Only every eighth overflow touches the 16-bit tick counter,
 * reducing ISR cost while preserving the four-second window.
 */
void timer0_isr(void) interrupt 1
{
    ADC_CLK = !ADC_CLK;
    isr_prescale++;

    if (isr_prescale >= TICK_PRESCALER)
    {
        isr_prescale = 0U;
        timer0_ticks++;
    }
}

void main(void)
{
    unsigned char filtered_adc;
    unsigned char sample_spread;
    unsigned char quality;
    unsigned char trend;
    unsigned char contact;
    unsigned char fault_windows = 0U;
    unsigned int temperature_x10;
    unsigned int forecast_x10;
    unsigned int eta_seconds;
    unsigned int estimate_x10;
    unsigned int previous_temperature_x10 = 0U;
    unsigned int last_valid_temperature_x10 = 0U;
    bit have_previous_measurement = 0;
    bit have_last_valid = 0;
    bit page_toggle = 0;

    lcd_init();
    lcd_load_bar_glyphs();
    adc_init();
    history_reset();

    lcd_clear_line(0x80);
    lcd_print("THERMAL MONITOR");
    lcd_clear_line(0xC0);
    lcd_print("INITIALIZING...");
    delay(80U);

    while (1)
    {
        filtered_adc = adc_read_trimmed_mean(&sample_spread);
        temperature_x10 = adc_to_temperature_x10(filtered_adc);
        quality = classify_quality(sample_spread);

        if (temperature_x10 > MAX_VALID_TEMP_X10)
        {
            if (fault_windows < 255U)
            {
                fault_windows++;
            }

            if (have_last_valid && slope_valid &&
                fault_windows <= DEAD_RECKON_WINDOWS)
            {
                estimate_x10 = dead_reckon_estimate_x10(
                    last_valid_temperature_x10, fault_windows);
                lcd_show_fault_with_estimate(
                    estimate_x10,
                    (unsigned char)(DEAD_RECKON_WINDOWS - fault_windows));
            }
            else
            {
                lcd_show_fault();
            }

            /* Keep the cached slope for bounded fault-time estimation. */
            history_reset();
            have_previous_measurement = 0;
            page_toggle = 0;
        }
        else
        {
            fault_windows = 0U;

            if (have_previous_measurement)
            {
                trend = classify_trend(temperature_x10,
                                       previous_temperature_x10);
            }
            else
            {
                trend = TREND_STABLE;
                have_previous_measurement = 1;
            }

            history_push(temperature_x10);
            contact = detect_contact();
            forecast_x10 = forecast_temperature_x10();
            eta_seconds = eta_to_threshold_seconds(temperature_x10);

            if (eta_seconds != ETA_INVALID)
            {
                page_toggle = !page_toggle;
            }
            else
            {
                page_toggle = 0;
            }

            if ((eta_seconds != ETA_INVALID) && page_toggle)
            {
                lcd_show_eta_page(eta_seconds);
            }
            else
            {
                lcd_show_dashboard(temperature_x10,
                                   forecast_x10,
                                   quality,
                                   trend,
                                   contact);
            }

            previous_temperature_x10 = temperature_x10;
            last_valid_temperature_x10 = temperature_x10;
            have_last_valid = 1;
        }

        wait_for_next_measurement_window();
    }
}

void adc_init(void)
{
    ADC_ALE = 0;
    ADC_OE = 0;
    ADC_START = 0;
    ADC_CLK = 0;

    /* Timer 0, mode 2 (8-bit auto reload), interrupt enabled. */
    TMOD = (TMOD & 0xF0) | 0x02;
    TH0 = (unsigned char)TIMER0_RELOAD;
    TL0 = (unsigned char)TIMER0_RELOAD;
    ET0 = 1;
    EA = 1;
    TR0 = 1;
}

unsigned char adc_read_channel_zero(void)
{
    unsigned char value;

    ADC_CH_C = 0;
    ADC_CH_B = 0;
    ADC_CH_A = 0;

    ADC_ALE = 1;
    ADC_START = 1;
    delay(1U);
    ADC_ALE = 0;
    ADC_START = 0;

    while (ADC_EOC == 1);
    while (ADC_EOC == 0);

    ADC_OE = 1;
    delay(1U);
    value = P1;
    ADC_OE = 0;

    return value;
}

/*
 * Read 16 samples, reject the minimum and maximum, then average the
 * remaining 14. This removes isolated spikes without sorting an array.
 */
unsigned char adc_read_trimmed_mean(unsigned char *spread)
{
    unsigned char index;
    unsigned char sample;
    unsigned char minimum = 255U;
    unsigned char maximum = 0U;
    unsigned int sum = 0U;
    unsigned int trimmed_sum;

    for (index = 0U; index < SAMPLE_COUNT; index++)
    {
        sample = adc_read_channel_zero();
        sum += sample;

        if (sample < minimum)
        {
            minimum = sample;
        }

        if (sample > maximum)
        {
            maximum = sample;
        }
    }

    *spread = maximum - minimum;
    trimmed_sum = sum - minimum - maximum;

    return (unsigned char)((trimmed_sum +
                            (TRIMMED_SAMPLE_COUNT / 2U)) /
                           TRIMMED_SAMPLE_COUNT);
}

/* LM35: 10 mV/degree C, so millivolts equal tenths of a degree C. */
unsigned int adc_to_temperature_x10(unsigned char adc_count)
{
    return (unsigned int)((((unsigned long)adc_count * ADC_VREF_MV) +
                           127UL) / 255UL);
}

unsigned char classify_quality(unsigned char spread)
{
    if (spread <= 1U)
    {
        return QUALITY_HIGH;
    }

    if (spread <= 3U)
    {
        return QUALITY_MEDIUM;
    }

    return QUALITY_LOW;
}

unsigned char classify_trend(unsigned int current_x10,
                             unsigned int previous_x10)
{
    if (current_x10 > (previous_x10 + TREND_THRESHOLD_X10))
    {
        return TREND_RISING;
    }

    if (previous_x10 > (current_x10 + TREND_THRESHOLD_X10))
    {
        return TREND_FALLING;
    }

    return TREND_STABLE;
}

void history_reset(void)
{
    history_count = 0U;
}

void history_push(unsigned int temperature_x10)
{
    unsigned char index;

    if (history_count < SPARK_LENGTH)
    {
        temperature_history[history_count] = temperature_x10;
        history_count++;
        return;
    }

    for (index = 0U; index < (SPARK_LENGTH - 1U); index++)
    {
        temperature_history[index] = temperature_history[index + 1U];
    }

    temperature_history[SPARK_LENGTH - 1U] = temperature_x10;
}

/*
 * Least-squares trend across the visible history. Multiplying the fitted
 * slope by 15 projects approximately 60 seconds ahead for 4-second windows.
 * The projected change is capped at +/-20.0 C to suppress absurd forecasts
 * after a transient or sensor disturbance.
 *
 * The fitted slope is retained for the threshold ETA and for a short,
 * explicitly labelled estimate if the sensor becomes invalid.
 */
unsigned int forecast_temperature_x10(void)
{
    unsigned char index;
    signed long n;
    signed long sum_x = 0L;
    signed long sum_y = 0L;
    signed long sum_xy = 0L;
    signed long sum_x2 = 0L;
    signed long numerator;
    signed long denominator;
    signed long projected_change;
    signed long predicted;

    if (history_count == 0U)
    {
        slope_valid = 0;
        return 0U;
    }

    if (history_count < MIN_FORECAST_POINTS)
    {
        slope_valid = 0;
        return temperature_history[history_count - 1U];
    }

    n = (signed long)history_count;

    for (index = 0U; index < history_count; index++)
    {
        sum_x += (signed long)index;
        sum_y += (signed long)temperature_history[index];
        sum_xy += ((signed long)index *
                   (signed long)temperature_history[index]);
        sum_x2 += ((signed long)index * (signed long)index);
    }

    numerator = (n * sum_xy) - (sum_x * sum_y);
    denominator = (n * sum_x2) - (sum_x * sum_x);

    if (denominator == 0L)
    {
        slope_valid = 0;
        return temperature_history[history_count - 1U];
    }

    slope_numerator = numerator;
    slope_denominator = denominator;
    slope_valid = 1;

    projected_change = (numerator * FORECAST_HORIZON_WINDOWS) /
                       denominator;

    if (projected_change > MAX_FORECAST_CHANGE_X10)
    {
        projected_change = MAX_FORECAST_CHANGE_X10;
    }
    else if (projected_change < -MAX_FORECAST_CHANGE_X10)
    {
        projected_change = -MAX_FORECAST_CHANGE_X10;
    }

    predicted = (signed long)temperature_history[history_count - 1U] +
                projected_change;

    if (predicted < 0L)
    {
        predicted = 0L;
    }
    else if (predicted > (signed long)MAX_VALID_TEMP_X10)
    {
        predicted = (signed long)MAX_VALID_TEMP_X10;
    }

    return (unsigned int)predicted;
}

/*
 * Return the time until the fitted trend reaches ALERT_THRESHOLD_X10.
 * The division rounds up so the display never claims the threshold will be
 * reached earlier than the fitted line predicts.
 */
unsigned int eta_to_threshold_seconds(unsigned int current_x10)
{
    signed long gap;
    signed long scaled_gap;
    signed long scaled_slope;
    unsigned long distance;
    unsigned long rate;
    unsigned long windows;

    if (!slope_valid || slope_numerator == 0L)
    {
        return ETA_INVALID;
    }

    gap = (signed long)ALERT_THRESHOLD_X10 - (signed long)current_x10;

    if (gap == 0L || ((gap > 0L) != (slope_numerator > 0L)))
    {
        return ETA_INVALID;
    }

    scaled_gap = gap * slope_denominator;
    scaled_slope = slope_numerator;
    distance = (unsigned long)((scaled_gap < 0L) ?
                               -scaled_gap : scaled_gap);
    rate = (unsigned long)((scaled_slope < 0L) ?
                           -scaled_slope : scaled_slope);
    windows = (distance + rate - 1UL) / rate;

    if (windows == 0UL ||
        windows > (unsigned long)(MAX_ETA_SECONDS /
                                  MEASUREMENT_WINDOW_SECONDS))
    {
        return ETA_INVALID;
    }

    return (unsigned int)(windows * MEASUREMENT_WINDOW_SECONDS);
}

/* A rise of at least 1.5 C across two windows indicates likely contact. */
unsigned char detect_contact(void)
{
    if (history_count < 3U)
    {
        return 0U;
    }

    if (temperature_history[history_count - 1U] >=
        (temperature_history[history_count - 3U] + CONTACT_RISE_X10))
    {
        return 1U;
    }

    return 0U;
}

/* Extrapolate from the final valid reading using the cached fitted slope. */
unsigned int dead_reckon_estimate_x10(unsigned int last_valid_x10,
                                      unsigned char windows_elapsed)
{
    signed long estimate;

    estimate = (signed long)last_valid_x10 +
               ((slope_numerator * (signed long)windows_elapsed) /
                slope_denominator);

    if (estimate < 0L)
    {
        estimate = 0L;
    }
    else if (estimate > (signed long)MAX_VALID_TEMP_X10)
    {
        estimate = (signed long)MAX_VALID_TEMP_X10;
    }

    return (unsigned int)estimate;
}

/* Render a right-aligned, auto-ranging 16-column thermal sparkline. */
void sparkline_render(void)
{
    unsigned char column;
    unsigned char history_index;
    unsigned char first_column;
    unsigned char level;
    unsigned int minimum = 0xFFFFU;
    unsigned int maximum = 0U;
    unsigned int span;
    unsigned int offset;

    lcd_command(0xC0);

    if (history_count == 0U)
    {
        for (column = 0U; column < SPARK_LENGTH; column++)
        {
            lcd_data(' ');
        }
        return;
    }

    for (history_index = 0U;
         history_index < history_count;
         history_index++)
    {
        if (temperature_history[history_index] < minimum)
        {
            minimum = temperature_history[history_index];
        }

        if (temperature_history[history_index] > maximum)
        {
            maximum = temperature_history[history_index];
        }
    }

    span = maximum - minimum;
    first_column = SPARK_LENGTH - history_count;

    for (column = 0U; column < SPARK_LENGTH; column++)
    {
        if (column < first_column)
        {
            lcd_data(' ');
        }
        else
        {
            history_index = column - first_column;

            if (span == 0U)
            {
                level = 3U; /* Flat history is shown at mid-height. */
            }
            else
            {
                offset = temperature_history[history_index] - minimum;
                level = (unsigned char)(((offset * 7U) +
                                         (span / 2U)) / span);
            }

            lcd_data(level);
        }
    }
}

unsigned int timer0_snapshot(void)
{
    unsigned int snapshot;

    EA = 0;
    snapshot = timer0_ticks;
    EA = 1;

    return snapshot;
}

void wait_for_next_measurement_window(void)
{
    unsigned int start = timer0_snapshot();
    unsigned int elapsed;

    do
    {
        elapsed = timer0_snapshot() - start;
    }
    while (elapsed < MEASUREMENT_WINDOW_TICKS);
}

void lcd_init(void)
{
    delay(20U);
    lcd_command(0x38); /* 8-bit, two-line mode. */
    lcd_command(0x0C); /* Display on, cursor off. */
    lcd_command(0x06); /* Increment cursor. */
    lcd_command(0x01); /* Clear display. */
    delay(5U);
}

void lcd_load_bar_glyphs(void)
{
    unsigned char glyph;
    unsigned char row;

    lcd_command(0x40); /* CGRAM address zero. */

    for (glyph = 0U; glyph < 8U; glyph++)
    {
        for (row = 0U; row < 8U; row++)
        {
            lcd_data(bar_glyphs[glyph][row]);
        }
    }

    lcd_command(0x80); /* Return to display data RAM. */
}

void lcd_command(unsigned char command)
{
    P3 = command;
    LCD_RS = 0;
    LCD_RW = 0;
    LCD_EN = 1;
    delay(1U);
    LCD_EN = 0;
    delay(2U);
}

void lcd_data(unsigned char value)
{
    P3 = value;
    LCD_RS = 1;
    LCD_RW = 0;
    LCD_EN = 1;
    delay(1U);
    LCD_EN = 0;
    delay(2U);
}

void lcd_print(const char *text)
{
    while (*text != '\0')
    {
        lcd_data((unsigned char)*text);
        text++;
    }
}

void lcd_clear_line(unsigned char address)
{
    unsigned char index;

    lcd_command(address);
    for (index = 0U; index < 16U; index++)
    {
        lcd_data(' ');
    }
    lcd_command(address);
}

void lcd_print_unsigned(unsigned int value)
{
    if (value >= 100U)
    {
        lcd_data((unsigned char)('0' + ((value / 100U) % 10U)));
    }

    if (value >= 10U)
    {
        lcd_data((unsigned char)('0' + ((value / 10U) % 10U)));
    }

    lcd_data((unsigned char)('0' + (value % 10U)));
}

void lcd_print_two_digits(unsigned int value)
{
    lcd_data((unsigned char)('0' + ((value / 10U) % 10U)));
    lcd_data((unsigned char)('0' + (value % 10U)));
}

void lcd_print_temperature(unsigned int temperature_x10)
{
    lcd_print_unsigned(temperature_x10 / 10U);
    lcd_data('.');
    lcd_data((unsigned char)('0' + (temperature_x10 % 10U)));
    lcd_data('C');
}

/*
 * Maximum-width example (15 characters): 150.0C>150.0C^H
 * ^, v, =, and ! are ROM characters; all eight CGRAM slots remain available
 * for the bar graph. ! indicates likely contact with the sensor.
 */
void lcd_show_dashboard(unsigned int temperature_x10,
                        unsigned int forecast_x10,
                        unsigned char quality,
                        unsigned char trend,
                        unsigned char contact)
{
    lcd_clear_line(0x80);
    lcd_print_temperature(temperature_x10);
    lcd_data('>');
    lcd_print_temperature(forecast_x10);

    if (contact != 0U)
    {
        lcd_data('!');
    }
    else if (trend == TREND_RISING)
    {
        lcd_data('^');
    }
    else if (trend == TREND_FALLING)
    {
        lcd_data('v');
    }
    else
    {
        lcd_data('=');
    }

    if (quality == QUALITY_HIGH)
    {
        lcd_data('H');
    }
    else if (quality == QUALITY_MEDIUM)
    {
        lcd_data('M');
    }
    else
    {
        lcd_data('L');
    }

    sparkline_render();
}

/* Time-to-threshold page, for example: "37.0C IN 02:30". */
void lcd_show_eta_page(unsigned int eta_seconds)
{
    lcd_clear_line(0x80);
    lcd_print_temperature(ALERT_THRESHOLD_X10);
    lcd_print(" IN ");
    lcd_print_two_digits(eta_seconds / 60U);
    lcd_data(':');
    lcd_print_two_digits(eta_seconds % 60U);

    sparkline_render();
}

void lcd_show_fault(void)
{
    lcd_clear_line(0x80);
    lcd_print(" SENSOR FAULT");
    lcd_clear_line(0xC0);
    lcd_print(" CHECK LM35/ADC");
}

/* Fault page with a bounded estimate, for example: "EST 26.7C (3)". */
void lcd_show_fault_with_estimate(unsigned int estimate_x10,
                                  unsigned char windows_remaining)
{
    lcd_clear_line(0x80);
    lcd_print(" SENSOR FAULT");
    lcd_clear_line(0xC0);
    lcd_print("EST ");
    lcd_print_temperature(estimate_x10);
    lcd_print(" (");
    lcd_data((unsigned char)('0' + windows_remaining));
    lcd_data(')');
}

void delay(unsigned int ticks)
{
    unsigned int count;

    while (ticks > 0U)
    {
        for (count = 0U; count < 400U; count++);
        ticks--;
    }
}
