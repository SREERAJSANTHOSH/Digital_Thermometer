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
 * Measure the ADC reference on the real circuit and update this value.
 * 2.560 V gives approximately 10 mV per ADC count, which closely matches
 * the LM35 response of 10 mV per degree Celsius.
 */
#define ADC_VREF_MV             2560UL
#define SAMPLE_COUNT            16U
#define TRIMMED_SAMPLE_COUNT    14U
#define TREND_THRESHOLD_X10     10U
#define MAX_VALID_TEMP_X10      1500U

#define TREND_STABLE            0U
#define TREND_RISING            1U
#define TREND_FALLING           2U

#define QUALITY_HIGH            0U
#define QUALITY_MEDIUM          1U
#define QUALITY_LOW             2U

void delay(unsigned int ticks);
void lcd_init(void);
void lcd_command(unsigned char command);
void lcd_data(unsigned char value);
void lcd_print(const char *text);
void lcd_clear_line(unsigned char address);
void lcd_print_unsigned(unsigned int value);
void lcd_show_measurement(unsigned int temperature_x10,
                          unsigned char quality,
                          unsigned char trend);
void lcd_show_fault(void);

void adc_init(void);
unsigned char adc_read_channel_zero(void);
unsigned char adc_read_trimmed_mean(unsigned char *spread);
unsigned int adc_to_temperature_x10(unsigned char adc_count);
unsigned char classify_quality(unsigned char spread);
unsigned char classify_trend(unsigned int current_x10,
                             unsigned int previous_x10);

/* Timer 0 supplies the external clock required by the ADC0808. */
void timer0_isr(void) interrupt 1
{
    ADC_CLK = !ADC_CLK;
}

void main(void)
{
    unsigned char filtered_adc;
    unsigned char sample_spread;
    unsigned char quality;
    unsigned char trend;
    unsigned int temperature_x10;
    unsigned int previous_temperature_x10 = 0U;
    bit have_previous_measurement = 0;

    lcd_init();
    adc_init();

    lcd_clear_line(0x80);
    lcd_print("DIGITAL THERMO");
    lcd_clear_line(0xC0);
    lcd_print("SMART ACQUIRE");
    delay(80U);

    while (1)
    {
        filtered_adc = adc_read_trimmed_mean(&sample_spread);
        temperature_x10 = adc_to_temperature_x10(filtered_adc);
        quality = classify_quality(sample_spread);

        if (temperature_x10 > MAX_VALID_TEMP_X10)
        {
            lcd_show_fault();
            have_previous_measurement = 0;
        }
        else
        {
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

            lcd_show_measurement(temperature_x10, quality, trend);
            previous_temperature_x10 = temperature_x10;
        }

        delay(40U);
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
    TH0 = 0xC2;
    TL0 = 0xC2;
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

    /* Wait for the ADC0808 end-of-conversion pulse. */
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
 * remaining 14. The rejected extremes reduce the effect of single-sample
 * spikes without the RAM cost of sorting a sample array on an 8051.
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

void lcd_init(void)
{
    delay(20U);
    lcd_command(0x38); /* 8-bit, two-line mode. */
    lcd_command(0x0C); /* Display on, cursor off. */
    lcd_command(0x06); /* Increment cursor. */
    lcd_command(0x01); /* Clear display. */
    delay(5U);
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

void lcd_show_measurement(unsigned int temperature_x10,
                          unsigned char quality,
                          unsigned char trend)
{
    lcd_clear_line(0x80);
    lcd_print("TEMP: ");
    lcd_print_unsigned(temperature_x10 / 10U);
    lcd_data('.');
    lcd_data((unsigned char)('0' + (temperature_x10 % 10U)));
    lcd_data(0xDF); /* Degree symbol in common HD44780 character ROMs. */
    lcd_data('C');

    lcd_clear_line(0xC0);
    lcd_print("Q:");

    if (quality == QUALITY_HIGH)
    {
        lcd_print("HIGH ");
    }
    else if (quality == QUALITY_MEDIUM)
    {
        lcd_print("MED  ");
    }
    else
    {
        lcd_print("LOW  ");
    }

    if (trend == TREND_RISING)
    {
        lcd_print("RISING");
    }
    else if (trend == TREND_FALLING)
    {
        lcd_print("FALLING");
    }
    else
    {
        lcd_print("STABLE");
    }
}

void lcd_show_fault(void)
{
    lcd_clear_line(0x80);
    lcd_print(" SENSOR FAULT");
    lcd_clear_line(0xC0);
    lcd_print(" CHECK LM35/ADC");
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

