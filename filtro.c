#include <stdio.h>
#include "hardware/adc.h"
#include "hardware/timer.h"
#include "hardware/pwm.h"
#include "WS2812B/matrizFrames.h"
#include "Display/ssd1306.h"

#define CENTRAL_X 2048
#define CENTRAL_Y 2048
#define JS_VARIACAO 180
#define MIN_WIDTH 10
#define MIN_HEIGHT 10

const uint joystick_x_pin = 27;
const uint joystick_y_pin = 26;
const uint btn_a_pin = 5;
const uint btn_b_pin = 6;
const uint led_green_pin = 11;
const uint buzzer_pin = 21;

// Variáveis para PWM
const uint16_t WRAP = 5000;   
const float DIV = 0.0;
uint sliceBuzzer;

const uint jsx_por_pixel = CENTRAL_X / (DISPLAY_WIDTH - MIN_WIDTH - 2);
const uint jsy_por_pixel = CENTRAL_Y / (DISPLAY_HEIGHT - MIN_HEIGHT - 2);

ssd1306_t ssd;

static uint16_t joystick_dx, joystick_dy = 0;
static uint16_t dx_anterior, dy_anterior = 0;
static volatile uint8_t width;
static volatile uint8_t height;
static volatile uint8_t max_fill_height;

static volatile char temperaturaAtual = 'N';
static volatile bool ligado = false;
static volatile bool copoDetected = false;
static volatile uint8_t nivelAgua = 0;
struct repeating_timer timer;

static volatile bool novaMatriz = true;
static volatile bool novoDisplay = true;

// Buffers para tratamento do bounce de botões
static volatile uint atual;
static volatile uint last_time = 0;

void inicializarBtn(uint pino);
uint inicializarPWM(uint pino);
void onBtnPress(uint gpio, uint32_t events);
bool encherRecipiente(struct repeating_timer *t);

int main()
{   
    gpio_init(led_green_pin);
    gpio_set_dir(led_green_pin, GPIO_OUT);
    gpio_put(led_green_pin, 0);

    // Inicializar ADC para o joystick
    adc_init();
    adc_gpio_init(joystick_x_pin);
    adc_gpio_init(joystick_y_pin);

    // Inicializar buzzer
    sliceBuzzer = inicializarPWM(buzzer_pin);
    pwm_set_enabled(sliceBuzzer, true);

    // Inicializar matriz de LEDs
    inicializarMatriz();

    // Inicializar display
    ssd1306_i2c_init(&ssd);

    // Inicializar botões e configurar rotinas de interrupção
    inicializarBtn(btn_a_pin);
    inicializarBtn(btn_b_pin);
    gpio_set_irq_enabled_with_callback(btn_a_pin, GPIO_IRQ_EDGE_FALL, true, &onBtnPress);
    gpio_set_irq_enabled_with_callback(btn_b_pin, GPIO_IRQ_EDGE_FALL, true, &onBtnPress);

    while (true)
    {   
        if (!copoDetected)
        {
            // Ler variação das posições X e Y do joystick dado a posição central
            adc_select_input(1);
            joystick_dx = abs(adc_read() - CENTRAL_X);
            adc_select_input(0);
            joystick_dy = abs(adc_read() - CENTRAL_Y);

            // Tratar possíveis variações da posição central do joystick
            tratarVariacao(&joystick_dx, JS_VARIACAO);
            tratarVariacao(&joystick_dy, JS_VARIACAO);

            if ((joystick_dx != dx_anterior || joystick_dy != dy_anterior));
            {
                novoDisplay = true;
                dx_anterior = joystick_dx;
                dy_anterior = joystick_dy;
            }

            if (novoDisplay)
            {   
                width = MIN_WIDTH + joystick_dx / jsx_por_pixel;
                height = MIN_HEIGHT + joystick_dy / jsy_por_pixel;

                ssd1306_fill(&ssd, false);
                ssd1306_draw_recipient(&ssd, width, height, 0);
                ssd1306_send_data(&ssd);
                novoDisplay = false;
            }
            if (novaMatriz)
            {
                desenharTemperatura(temperaturaAtual);
                novaMatriz = false;
            }
        }
        else
        {
            if (novoDisplay)
            {
                ssd1306_fill(&ssd, false);
                ssd1306_draw_recipient(&ssd, width, height, nivelAgua);
                if (nivelAgua < max_fill_height)
                {
                    ssd1306_vline(&ssd, (DISPLAY_WIDTH / 2) - 1, 0, DISPLAY_HEIGHT - 1, true);
                    ssd1306_vline(&ssd, DISPLAY_WIDTH / 2, 0, DISPLAY_HEIGHT - 1 , true);
                }
                else
                {   
                    gpio_put(led_green_pin, 0);

                    pwm_set_gpio_level(buzzer_pin, WRAP / 2);
                    sleep_ms(300);
                    pwm_set_gpio_level(buzzer_pin, 0);
                }
                ssd1306_send_data(&ssd);
                novoDisplay = false;
            }

        }
    }
}

// Inicializa o botão em um dado pino
void inicializarBtn(uint pino)
{
    gpio_init(pino);
    gpio_set_dir(pino, GPIO_IN);
    gpio_pull_up(pino);
}

// Inicializa e configura o PWM em um pino
uint inicializarPWM(uint pino)
{   
    // Obter slice e definir pino como PWM
    gpio_set_function(pino, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(pino);

    // Configurar frequência
    pwm_set_wrap(slice, WRAP);
    pwm_set_clkdiv(slice, DIV); 

    return slice;
}

void onBtnPress(uint gpio, uint32_t events)
{   
    atual = to_us_since_boot(get_absolute_time());
    if (atual - last_time > 200000)
    {   
        last_time = atual;

        if (gpio == btn_a_pin)
        {
            if (!copoDetected)
            {
                copoDetected = true;
                
                max_fill_height = height - FILL_HEIGHT_DIF;
                add_repeating_timer_ms(500, encherRecipiente, NULL, &timer);
                gpio_put(led_green_pin, 1);
                novoDisplay = true;
            }
        }
        else if (gpio == btn_b_pin)
        {
            if (!copoDetected)
            {
                temperaturaAtual = temperaturaAtual == 'N' ? 'G' : 'N';
                novaMatriz = true;
            }
            else
            {   
                copoDetected = false;
                nivelAgua = 0;
                max_fill_height = 0;
                gpio_put(led_green_pin, 0);
            }
        }
    }
}

bool encherRecipiente(struct repeating_timer *t)
{   
    nivelAgua++;
    novoDisplay = true;
    if (nivelAgua >= max_fill_height)
    {   
        return false;
    }
    else
    {
        return true;
    }
}