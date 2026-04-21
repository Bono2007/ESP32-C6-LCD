#include "Display_ST7789.h"

static SPIClass *lcd_spi = nullptr;

static inline void dc_cmd(void)  { digitalWrite(LCD_DC_PIN, LOW); }
static inline void dc_data(void) { digitalWrite(LCD_DC_PIN, HIGH); }
static inline void cs_on(void)   { digitalWrite(LCD_CS_PIN, LOW); }
static inline void cs_off(void)  { digitalWrite(LCD_CS_PIN, HIGH); }

static void LCD_WriteCommand(uint8_t cmd)
{
    dc_cmd();
    cs_on();
    lcd_spi->transfer(cmd);
    cs_off();
}

static void LCD_WriteData(uint8_t data)
{
    dc_data();
    cs_on();
    lcd_spi->transfer(data);
    cs_off();
}

static void LCD_WriteData_Word(uint16_t data)
{
    dc_data();
    cs_on();
    lcd_spi->transfer16(data);
    cs_off();
}

/* Bulk transfer — avoids stack overflow on large pixel blobs */
static void LCD_WriteData_nbyte(const uint8_t *buf, size_t len)
{
    dc_data();
    cs_on();
    lcd_spi->writeBytes(buf, len);
    cs_off();
}

static void LCD_Reset(void)
{
    digitalWrite(LCD_RST_PIN, HIGH);
    delay(10);
    digitalWrite(LCD_RST_PIN, LOW);
    delay(10);
    digitalWrite(LCD_RST_PIN, HIGH);
    delay(120);
}

void LCD_SetCursor(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    x1 += LCD_X_OFFSET;
    x2 += LCD_X_OFFSET;
    y1 += LCD_Y_OFFSET;
    y2 += LCD_Y_OFFSET;

    LCD_WriteCommand(0x2A);
    LCD_WriteData(x1 >> 8); LCD_WriteData(x1 & 0xFF);
    LCD_WriteData(x2 >> 8); LCD_WriteData(x2 & 0xFF);

    LCD_WriteCommand(0x2B);
    LCD_WriteData(y1 >> 8); LCD_WriteData(y1 & 0xFF);
    LCD_WriteData(y2 >> 8); LCD_WriteData(y2 & 0xFF);

    LCD_WriteCommand(0x2C);
}

void LCD_addWindow(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t *color)
{
    LCD_SetCursor(x1, y1, x2, y2);
    uint32_t pixels = (uint32_t)(x2 - x1 + 1) * (y2 - y1 + 1);
    /* Swap bytes for RGB565 endianness expected by ST7789 */
    uint8_t *p = (uint8_t *)color;
    dc_data();
    cs_on();
    for (uint32_t i = 0; i < pixels; i++) {
        lcd_spi->transfer(p[i * 2 + 1]);
        lcd_spi->transfer(p[i * 2]);
    }
    cs_off();
}

void Backlight_Init(void)
{
    ledcAttach(LCD_BL_PIN, BL_PWM_FREQ, BL_PWM_RES);
}

void Set_Backlight(uint8_t percent)
{
    if (percent > 100) percent = 100;
    uint32_t duty = ((uint32_t)percent * ((1 << BL_PWM_RES) - 1)) / 100;
    ledcWrite(LCD_BL_PIN, duty);
}

void LCD_Init(void)
{
    pinMode(LCD_DC_PIN,  OUTPUT);
    pinMode(LCD_CS_PIN,  OUTPUT);
    pinMode(LCD_RST_PIN, OUTPUT);
    cs_off();

    lcd_spi = new SPIClass(FSPI);
    lcd_spi->begin(LCD_SCLK_PIN, LCD_MISO_PIN, LCD_MOSI_PIN, LCD_CS_PIN);
    lcd_spi->setFrequency(LCD_SPI_FREQ);

    Backlight_Init();
    Set_Backlight(0);
    LCD_Reset();

    /* ST7789 init sequence */
    LCD_WriteCommand(0x11); /* Sleep out */
    delay(120);

    LCD_WriteCommand(0x36); /* MADCTL: row/col order */
    LCD_WriteData(0x00);

    LCD_WriteCommand(0x3A); /* COLMOD: 16-bit RGB565 */
    LCD_WriteData(0x05);

    /* Porch settings */
    LCD_WriteCommand(0xB2);
    LCD_WriteData(0x0C); LCD_WriteData(0x0C);
    LCD_WriteData(0x00); LCD_WriteData(0x33); LCD_WriteData(0x33);

    LCD_WriteCommand(0xB7); /* Gate control */
    LCD_WriteData(0x35);

    LCD_WriteCommand(0xBB); /* VCOM */
    LCD_WriteData(0x19);

    LCD_WriteCommand(0xC0); /* LCM control */
    LCD_WriteData(0x2C);

    LCD_WriteCommand(0xC2);
    LCD_WriteData(0x01);

    LCD_WriteCommand(0xC3); /* VRH */
    LCD_WriteData(0x12);

    LCD_WriteCommand(0xC4); /* VDV */
    LCD_WriteData(0x20);

    LCD_WriteCommand(0xC6); /* Frame rate: 60 Hz */
    LCD_WriteData(0x0F);

    LCD_WriteCommand(0xD0); /* Power control */
    LCD_WriteData(0xA4); LCD_WriteData(0xA1);

    /* Positive gamma */
    LCD_WriteCommand(0xE0);
    LCD_WriteData(0xD0); LCD_WriteData(0x04); LCD_WriteData(0x0D);
    LCD_WriteData(0x11); LCD_WriteData(0x13); LCD_WriteData(0x2B);
    LCD_WriteData(0x3F); LCD_WriteData(0x54); LCD_WriteData(0x4C);
    LCD_WriteData(0x18); LCD_WriteData(0x0D); LCD_WriteData(0x0B);
    LCD_WriteData(0x1F); LCD_WriteData(0x23);

    /* Negative gamma */
    LCD_WriteCommand(0xE1);
    LCD_WriteData(0xD0); LCD_WriteData(0x04); LCD_WriteData(0x0C);
    LCD_WriteData(0x11); LCD_WriteData(0x13); LCD_WriteData(0x2C);
    LCD_WriteData(0x3F); LCD_WriteData(0x44); LCD_WriteData(0x51);
    LCD_WriteData(0x2F); LCD_WriteData(0x1F); LCD_WriteData(0x1F);
    LCD_WriteData(0x20); LCD_WriteData(0x23);

    LCD_WriteCommand(0x21); /* Inversion on (required for ST7789 172-px variant) */
    LCD_WriteCommand(0x29); /* Display on */
    delay(10);

    Set_Backlight(80);
}
