#include "ssd1306.h"

void ssd1306_i2c_init(ssd1306_t *ssd)
{
    // Inicializar porta I2C
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C); 
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C); 

    // Pull up da data e clock line
    gpio_pull_up(I2C_SDA); 
    gpio_pull_up(I2C_SCL); 

    // Inicializar e configurar display
    ssd1306_init(ssd, DISPLAY_WIDTH, DISPLAY_HEIGHT, false, endereco, I2C_PORT); 
    ssd1306_config(ssd); 
    ssd1306_send_data(ssd);
  
    // Limpa o display. O display inicia com todos os pixels apagados.
    ssd1306_fill(ssd, false);
    ssd1306_send_data(ssd);
}

void ssd1306_init(ssd1306_t *ssd, uint8_t width, uint8_t height, bool external_vcc, uint8_t address, i2c_inst_t *i2c) {
  ssd->width = width;
  ssd->height = height;
  ssd->pages = height / 8U;
  ssd->address = address;
  ssd->i2c_port = i2c;
  ssd->bufsize = ssd->pages * ssd->width + 1;
  ssd->ram_buffer = calloc(ssd->bufsize, sizeof(uint8_t));
  ssd->ram_buffer[0] = 0x40;
  ssd->port_buffer[0] = 0x80;
}

void ssd1306_config(ssd1306_t *ssd) {
  ssd1306_command(ssd, SET_DISP | 0x00);
  ssd1306_command(ssd, SET_MEM_ADDR);
  ssd1306_command(ssd, 0x01);
  ssd1306_command(ssd, SET_DISP_START_LINE | 0x00);
  ssd1306_command(ssd, SET_SEG_REMAP | 0x01);
  ssd1306_command(ssd, SET_MUX_RATIO);
  ssd1306_command(ssd, DISPLAY_HEIGHT - 1);
  ssd1306_command(ssd, SET_COM_OUT_DIR | 0x08);
  ssd1306_command(ssd, SET_DISP_OFFSET);
  ssd1306_command(ssd, 0x00);
  ssd1306_command(ssd, SET_COM_PIN_CFG);
  ssd1306_command(ssd, 0x12);
  ssd1306_command(ssd, SET_DISP_CLK_DIV);
  ssd1306_command(ssd, 0x80);
  ssd1306_command(ssd, SET_PRECHARGE);
  ssd1306_command(ssd, 0xF1);
  ssd1306_command(ssd, SET_VCOM_DESEL);
  ssd1306_command(ssd, 0x30);
  ssd1306_command(ssd, SET_CONTRAST);
  ssd1306_command(ssd, 0xFF);
  ssd1306_command(ssd, SET_ENTIRE_ON);
  ssd1306_command(ssd, SET_NORM_INV);
  ssd1306_command(ssd, SET_CHARGE_PUMP);
  ssd1306_command(ssd, 0x14);
  ssd1306_command(ssd, SET_DISP | 0x01);
}

void ssd1306_command(ssd1306_t *ssd, uint8_t command) {
  ssd->port_buffer[1] = command;
  i2c_write_blocking(
    ssd->i2c_port,
    ssd->address,
    ssd->port_buffer,
    2,
    false
  );
}

void ssd1306_send_data(ssd1306_t *ssd) {
  ssd1306_command(ssd, SET_COL_ADDR);
  ssd1306_command(ssd, 0);
  ssd1306_command(ssd, ssd->width - 1);
  ssd1306_command(ssd, SET_PAGE_ADDR);
  ssd1306_command(ssd, 0);
  ssd1306_command(ssd, ssd->pages - 1);
  i2c_write_blocking(
    ssd->i2c_port,
    ssd->address,
    ssd->ram_buffer,
    ssd->bufsize,
    false
  );
}

void ssd1306_pixel(ssd1306_t *ssd, uint8_t x, uint8_t y, bool value) {
  uint16_t index = (y >> 3) + (x << 3) + 1;
  uint8_t pixel = (y & 0b111);
  if (value)
    ssd->ram_buffer[index] |= (1 << pixel);
  else
    ssd->ram_buffer[index] &= ~(1 << pixel);
}


void ssd1306_fill(ssd1306_t *ssd, bool value) {
    // Itera por todas as posições do display
    for (uint8_t y = 0; y < ssd->height; ++y) {
        for (uint8_t x = 0; x < ssd->width; ++x) {
            ssd1306_pixel(ssd, x, y, value);
        }
    }
}

void ssd1306_hline(ssd1306_t *ssd, uint8_t x0, uint8_t x1, uint8_t y, bool value) {
  for (uint8_t x = x0; x <= x1; ++x)
    ssd1306_pixel(ssd, x, y, value);
}

void ssd1306_vline(ssd1306_t *ssd, uint8_t x, uint8_t y0, uint8_t y1, bool value) {
  for (uint8_t y = y0; y <= y1; ++y)
    ssd1306_pixel(ssd, x, y, value);
}

void ssd1306_draw_recipient(ssd1306_t *ssd, uint8_t width, uint8_t height, uint8_t fill_height)
{ 
    // Obter pontos de referência das colunas (Inferior esquerdo e direito)
    uint8_t bleft_x = (DISPLAY_WIDTH / 2) - round(width / 2.0) - 1;
    uint8_t bright_x = bleft_x + width + 1;
    limitar(&bleft_x, 1, DISPLAY_WIDTH - 1);
    limitar(&bright_x, 1, DISPLAY_WIDTH - 1);

    // Obter pontos de referência das linhas
    uint8_t line_y = DISPLAY_HEIGHT - 1;
    uint8_t height_y = line_y - height;
    limitar(&height_y, 1, DISPLAY_HEIGHT - 1);

    // Desenhar recipiente com largura um pouco maior para manter largura desejada do copo
    ssd1306_hline(ssd, bleft_x + 1, bright_x - 1, line_y, true);
    ssd1306_vline(ssd, bleft_x, height_y, line_y, true);
    ssd1306_vline(ssd, bright_x, height_y, line_y, true);

    // Se o recipiente deve ser preenchido
    if (fill_height > 0)
    {    
        // Limitar altura do preenchimento com a máxima
        limitar(&fill_height, 0, height - FILL_HEIGHT_DIF);

        // Adicionar linha interior no recipiente até atinjir a altura desejada
        for (uint8_t i = line_y - 1; i > line_y - fill_height ; i--)
        {
          ssd1306_hline(ssd, bleft_x + 1, bright_x - 1, i, true);
        }
    }
}
