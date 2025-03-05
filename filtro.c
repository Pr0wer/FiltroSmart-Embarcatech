#include <stdio.h>
#include "hardware/adc.h"
#include "hardware/timer.h"
#include "hardware/pwm.h"
#include "WS2812B/matrizFrames.h"
#include "Display/ssd1306.h"

// Macros
#define CENTRAL_X 2048
#define CENTRAL_Y 2048
#define JS_VARIACAO 180
#define MIN_WIDTH 10
#define MIN_HEIGHT 10

// Pinos
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

// Valor para conversão da variação do joystick para pixels no display
const uint jsx_por_pixel = CENTRAL_X / (DISPLAY_WIDTH - MIN_WIDTH - 2);
const uint jsy_por_pixel = CENTRAL_Y / (DISPLAY_HEIGHT - MIN_HEIGHT - 2);

// Armazenar dados do display
ssd1306_t ssd;

// Armazenam a variação anterior e atual do joystick (em relação à posição central)
static uint16_t joystick_dx, joystick_dy = 0;
static uint16_t dx_anterior, dy_anterior = 0;

// Dados do recipiente atual
static volatile uint8_t width;
static volatile uint8_t height;
static volatile uint8_t max_fill_height;

// Variáveis de controle
static volatile char temperaturaAtual = 'N';
static volatile bool ligado = false;
static volatile bool copoDetected = false;

// Atualização e armazenamento do nível da água atual
static volatile uint8_t nivelAgua = 0;
struct repeating_timer timer;

// Buffers que indicam se esses periféricos devem ou não ser atualizados
static volatile bool novaMatriz = true;
static volatile bool novoDisplay = true;

// Buffers para tratamento do bounce de botões
static volatile uint atual;
static volatile uint last_time = 0;

// Headers de função
void inicializarBtn(uint pino);
uint inicializarPWM(uint pino);
void onBtnPress(uint gpio, uint32_t events);
bool encherRecipiente(struct repeating_timer *t);

int main()
{   
    // Inicializar LED RGB verde
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
        // Entrar em modo de preparação caso não houver um copo
        if (!copoDetected) // Modo de preparo
        {
            // Ler variação das posições X e Y do joystick dado a posição central
            adc_select_input(1);
            joystick_dx = abs(adc_read() - CENTRAL_X);
            adc_select_input(0);
            joystick_dy = abs(adc_read() - CENTRAL_Y);

            // Tratar possíveis variações da posição central do joystick
            tratarVariacao(&joystick_dx, JS_VARIACAO);
            tratarVariacao(&joystick_dy, JS_VARIACAO);

            // Se joystick se moveu
            if ((joystick_dx != dx_anterior || joystick_dy != dy_anterior));
            {   
                // Indicar nova informação no display
                novoDisplay = true;

                // Atualiza buffers
                dx_anterior = joystick_dx;
                dy_anterior = joystick_dy;
            }

            // Atualizar display caso exista uma nova informação
            if (novoDisplay)
            {   
                // Obter largura e altura do copo baseado na posição do joystick
                width = MIN_WIDTH + joystick_dx / jsx_por_pixel;
                height = MIN_HEIGHT + joystick_dy / jsy_por_pixel;

                // Desenhar recipiente no display
                ssd1306_fill(&ssd, false);
                ssd1306_draw_recipient(&ssd, width, height, 0);
                ssd1306_send_data(&ssd);
                novoDisplay = false;
            }

            // Atualizar matriz caso houver novas informações
            if (novaMatriz)
            {   
                // Desenhar icone da temperatura na matriz
                desenharTemperatura(temperaturaAtual);
                novaMatriz = false;
            }
        }
        else // Modo de preenchimento
        {       
            // Atualizar display caso existam novas informações
            if (novoDisplay)
            {   
                // Desenhar recipiente com o nível da água atual
                ssd1306_fill(&ssd, false);
                ssd1306_draw_recipient(&ssd, width, height, nivelAgua);

                // Se o recipiente ainda não está preenchido
                if (nivelAgua < max_fill_height)
                {    
                    // Desenhar "água caindo do filtro"
                    ssd1306_vline(&ssd, (DISPLAY_WIDTH / 2) - 1, 0, DISPLAY_HEIGHT - 1, true);
                    ssd1306_vline(&ssd, DISPLAY_WIDTH / 2, 0, DISPLAY_HEIGHT - 1 , true);
                }
                else
                {   
                    // Indicar pelo LED RGB que o filtro está desligado
                    gpio_put(led_green_pin, 0);

                    // Emitir sinal sonoro de finalização
                    pwm_set_gpio_level(buzzer_pin, WRAP / 2);
                    sleep_ms(300);
                    pwm_set_gpio_level(buzzer_pin, 0);
                }

                // Enviar informações ao display e atualizar buffer
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

// Rotina de interrupção dos botões
void onBtnPress(uint gpio, uint32_t events)
{   
    // Debounce dos botões
    atual = to_us_since_boot(get_absolute_time());
    if (atual - last_time > 200000)
    {   
        last_time = atual;

        // Botão A funciona apenas durante o modo de preparação
        if (gpio == btn_a_pin && !copoDetected)
        {   
            // Atualizar detecção do copo
            copoDetected = true;

            // Obter nível da água máximo para esse recipiente
            max_fill_height = height - FILL_HEIGHT_DIF;

            // "Velocidade" de emissão da água
            add_repeating_timer_ms(500, encherRecipiente, NULL, &timer);

            // Indicar filtro ligado no LED RGB
            gpio_put(led_green_pin, 1);
            novoDisplay = true;
        }
        else if (gpio == btn_b_pin)
        {
            if (!copoDetected) // Modo de preparação
            {    
                // Alterar temperatura da água
                temperaturaAtual = temperaturaAtual == 'N' ? 'G' : 'N';
                novaMatriz = true;
            }
            else // Modo de preenchimento
            {   
                // Resetar variáveis de controle para retorno ao modo de preparação
                copoDetected = false;
                nivelAgua = 0;
                max_fill_height = 0;
                gpio_put(led_green_pin, 0);
            }
        }
    }
}

// Função para o temporizador periódico que aumenta gradativamente o nível da água
bool encherRecipiente(struct repeating_timer *t)
{   
    // Aumenta o nível atual de água e indicar atualização para o display
    nivelAgua++;
    novoDisplay = true;

    // Desligar temporizador caso recipiente estiver cheio
    if (nivelAgua >= max_fill_height)
    {   
        return false;
    }
    else
    {
        return true;
    }
}
