#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "pico/binary_info.h"
#include "inc/ssd1306.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "kissfft/kiss_fft.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "kiss_fftr.h"


// Definições de hardware
#define AUDIO_PIN 28  // Microfone no ADC2
#define BTN_LEFT 5    // Botão para mudar afinação
#define BTN_RIGHT 6   // Botão para mudar corda
#define LED_OK 11     // LED verde (indica afinado)
#define LED_TIGHT 13  // LED vermelho (indica apertado demais)
#define LED_LOOSE 12  // LED AZUL (indica solto demais)
#define FFT_SIZE 1024 // Tamanho da FFT
#define BUZZER_PIN 10 // Pino do buzzer

// I2C configuration
#define OLED_SDA_PIN 14 // Pino SDA
#define OLED_SCL_PIN 15 // Pino SCL

// Frequências
float tunings[3][6] = {
    {82.41, 110.00, 146.83, 196.00, 246.94, 329.63},  // E-Standard
    {73.42, 110.00, 146.83, 196.00, 246.94, 329.63},  // Drop D
    {65.41, 98.00, 130.81, 174.61, 220.00, 293.66}    // Drop C
};

// Notas
char notes[3][6] = {
    {'E', 'A', 'D', 'G', 'B', 'E'},  // E-Standard
    {'D', 'A', 'D', 'G', 'B', 'E'},  // Drop D
    {'C', 'G', 'C', 'F', 'A', 'D'}   // Drop C
};

int tuning_index = 0; // Afinação selecionada
int string_index = 0; // Corda atual
float tolerance = 8.0; // Tolerância de frequência em Hz medida experimentalmente

uint8_t capture_buf[FFT_SIZE];  // Buffer para captura de áudio
float freqs[FFT_SIZE / 2];      // Frequências correspondentes a cada bin da FFT

// Configuração do DMA
dma_channel_config cfg;
uint dma_chan;

uint8_t ssd[ssd1306_buffer_length];  // Buffer para o display OLED

// Função para configurar o hardware
void setup() {
    stdio_init_all();

    // Configura o LED
    gpio_init(LED_OK);
    gpio_init(LED_TIGHT);
    gpio_init(LED_LOOSE);
    gpio_set_dir(LED_OK, GPIO_OUT);
    gpio_set_dir(LED_TIGHT, GPIO_OUT);
    gpio_set_dir(LED_LOOSE, GPIO_OUT);

    // Configura o ADC
    adc_init();
    adc_gpio_init(AUDIO_PIN); 
    adc_select_input(2);  // Canal ADC2

    // Configura o FIFO do ADC
    adc_fifo_setup(
        true,    // Escreve cada conversão no FIFO
        true,    // Habilita requisição de DMA (DREQ)
        1,       // DREQ (e IRQ) é ativado quando há pelo menos 1 amostra
        false,   // Não usamos o bit ERR porque lemos 8 bits
        true     // Desloca cada amostra para 8 bits ao enviar para o FIFO
    );

    // Configura a taxa de amostragem do ADC como sendo os 48MHz do processador
    //dividido pela taxa de amostragem da FFT que é 8khz
    adc_set_clkdiv(6000.f ); // (48000000 / 8000 - 1)

    // Configura o DMA
    dma_chan = dma_claim_unused_channel(true);
    cfg = dma_channel_get_default_config(dma_chan);

    // Lê de um endereço constante (FIFO do ADC) e escreve em um buffer
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_8);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, true);

    // Sincroniza as transferências com a disponibilidade de amostras do ADC
    channel_config_set_dreq(&cfg, DREQ_ADC);

    // Calcula as frequências correspondentes a cada bin da FFT
    float f_res = (float)8000 / FFT_SIZE;
    for (int i = 0; i < FFT_SIZE / 2; i++) {
        freqs[i] = f_res * i;
    }

    // Configura botões
    gpio_init(BTN_LEFT);
    gpio_init(BTN_RIGHT);
    gpio_set_dir(BTN_LEFT, GPIO_IN);
    gpio_set_dir(BTN_RIGHT, GPIO_IN);
    gpio_pull_up(BTN_LEFT);
    gpio_pull_up(BTN_RIGHT);

    // Inicializa o display OLED
    i2c_init(i2c1, ssd1306_i2c_clock * 1000);
    gpio_set_function(OLED_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(OLED_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(OLED_SDA_PIN);
    gpio_pull_up(OLED_SCL_PIN);
    ssd1306_init();
}

// Função para capturar amostras de áudio
void sample(uint8_t *capture_buf) {
    adc_fifo_drain();
    adc_run(false);

    // Configura o DMA para transferir as amostras para o buffer
    dma_channel_configure(dma_chan, &cfg,
                          capture_buf,    // Destino (buffer de captura)
                          &adc_hw->fifo,  // Fonte (FIFO do ADC)
                          FFT_SIZE,       // Número de amostras
                          true            // Inicia imediatamente
    );

    adc_run(true);
    dma_channel_wait_for_finish_blocking(dma_chan);
}

// Processa FFT e retorna a frequência detectada
float process_fft() {
    kiss_fftr_cfg fft_cfg = kiss_fftr_alloc(FFT_SIZE, false, 0, 0);
    if (fft_cfg == NULL) {
        printf("Failed to allocate memory for FFT configuration\n");
        return -1.0f;
    }

    kiss_fft_scalar fft_in[FFT_SIZE];  // Entrada da FFT
    kiss_fft_cpx fft_out[FFT_SIZE];    // Saída da FFT

    // Preenche a entrada da FFT, removendo o componente DC
    uint64_t sum = 0;
    for (int i = 0; i < FFT_SIZE; i++) {
        sum += capture_buf[i];
    }
    float avg = (float)sum / FFT_SIZE;
    for (int i = 0; i < FFT_SIZE; i++) {
        fft_in[i] = (float)capture_buf[i] - avg;
    }

    // Executa a FFT
    kiss_fftr(fft_cfg, fft_in, fft_out);

    // Encontra a frequência com a maior magnitude
    float max_power = 0;
    int max_idx = 0;
    for (int i = 0; i < FFT_SIZE / 2; i++) {
        float power = fft_out[i].r * fft_out[i].r + fft_out[i].i * fft_out[i].i;
        if (power > max_power) {
            max_power = power;
            max_idx = i;
        }
    }

    // Libera a memória alocada para a FFT
    kiss_fftr_free(fft_cfg);

    // Retorna a frequência detectada
    printf( "Detected frequency: %f Hz\n", freqs[max_idx]);
    return freqs[max_idx];
}

// Verifica se a frequência detectada está afinada
void check_tuning(float freq_detected) {
    float target_freq = tunings[tuning_index][string_index];
    printf("Check tuning: %f, %f\n", freq_detected, target_freq);
    // Se a corda está muito apertada, liga a luz vermelha
    if (freq_detected > target_freq + tolerance) {
        gpio_put(LED_OK, 0);
        gpio_put(LED_TIGHT, 1);
        gpio_put(LED_LOOSE, 0);
    // Se a corda está muito solta, liga a luz azul
    } else if (freq_detected < target_freq - tolerance) {
        gpio_put(LED_OK, 0);
        gpio_put(LED_TIGHT, 0);
        gpio_put(LED_LOOSE, 1);
    // Se a corda está dentro da tolerância, liga a luz verde
    } else {
        gpio_put(LED_OK, 1);
        gpio_put(LED_TIGHT, 0);
        gpio_put(LED_LOOSE, 0);
        play_frequency(BUZZER_PIN, tunings[tuning_index][string_index], 2000);
    }
}

// Exibe a mensagem de boas-vindas no OLED
void oled_display_welcome(struct render_area frame_area) {
    char *text[] = {
        "  Bem vindos    ", 
        "", 
        "", 
        "   Afinador ",
        "",
        "", 
        "by Pedro Viana", 
        "for Embarcatech"
    };

    int y = 0;
    for (uint i = 0; i < sizeof(text) / sizeof(text[0]); i++) {
        ssd1306_draw_string(ssd, 5, y, text[i]);
        y += 8;
    }
    render_on_display(ssd, &frame_area);
}

// Exibe o menu de seleção de afinação no OLED
int oled_display_menu(struct render_area frame_area) {
    static int bt_pressed = 0;  
    

    // Detecta se o botão foi pressionado
    if (gpio_get(BTN_RIGHT) == 0) {
        bt_pressed++; // Contagem de cliques
        if (bt_pressed > 3) {
            bt_pressed = 1; // Reinicia ao passar de 3
        }
        sleep_ms(150);  // Debounce para evitar múltiplas contagens para um único clique)
    }

    // Define opções de texto
    char *text[] = {
        "    Tunning      ",
        "               ", 
        " E Standard     ", 
        " Drop D         ",
        " Drop C         ",
        "               ",
        "B to nav       ",
        "A to select    "
    };

    // Ajusta ponteiro de seleção com base no botão pressionado
    switch (bt_pressed) {
        case 1:
            text[2] = "0 E-Standard ";
            tuning_index = 0; 
            break;
        case 2:
            text[3] = "0 Drop D     ";
            tuning_index = 1;  
            break;
        case 3:
            text[4] = "0 Drop C     ";
            tuning_index = 2;  
            break;
        default:
            break;
    }          

    int y = 0;
    for (uint i = 0; i < sizeof(text) / sizeof(text[0]); i++) {
        ssd1306_draw_string(ssd, 5, y, text[i]);
        y += 8;
    }

    render_on_display(ssd, &frame_area);

    return tuning_index;
}

//Buzzer para Acessibilidade Visual
// Função para tocar uma frequência no buzzer
void play_frequency(uint buzzer_pin, float frequency, uint duration_ms) {
    // Configura o PWM no pino do buzzer
    gpio_set_function(buzzer_pin, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(buzzer_pin);
    uint channel_num = pwm_gpio_to_channel(buzzer_pin);

    // Calcula o período do PWM com base na frequência
    float clock_divider = 125.0f;  // Divisor de clock padrão do PWM
    float wrap_value = (125000000.0f / (clock_divider * frequency)) - 1;

    // Configura o PWM
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, clock_divider);
    pwm_config_set_wrap(&config, (uint16_t)wrap_value);
    pwm_init(slice_num, &config, true);

    // Define o ciclo de trabalho (50% para um som claro)
    pwm_set_chan_level(slice_num, channel_num, (uint16_t)(wrap_value / 2));

    // Toca o som pelo tempo especificado
    sleep_ms(duration_ms);

    // Para o PWM (desliga o buzzer)
    pwm_set_enabled(slice_num, false);
}
// Função principal do afinador
void tuner(struct render_area frame_area) {
    static int bt_pressed = 0; // Contagem de cliques
    static int last_string_index = -1;  // Armazena o último índice de corda

    printf("tuning_index: %d, string_index: %d\n", tuning_index, string_index);
    
    // Define opções de texto
    char text[8][32] = {
        "      hearing....      ",          
        "                       ",
        "                       ", 
        "                       ",
        "                       ",
        "                       ",
        "                       ",
        "                       "
    };

    // Loop principal
    while (1) {
        // Verifica se o botão direito foi pressionado
        if (gpio_get(BTN_RIGHT) == 0) {  // Botão em pull-up, pressionado = 0
            if (!bt_pressed) {  // Evita múltiplas detecções
                bt_pressed = 1;
                
                // Incrementa o índice da corda
                string_index++;
                if (string_index > 5) {  // Verifica se excedeu o número de cordas
                    string_index = 0;
                }
                
                printf("String changed to index: %d\n", string_index);
                sleep_ms(200);  // Debounce
            }
        } else {
            bt_pressed = 0;  // Reseta o estado do botão
        }

        // Limpa o buffer do display
        memset(ssd, 0, ssd1306_buffer_length);

        // Atualiza o texto com a frequência alvo
        sprintf(text[4], "Target %.2f Hz", tunings[tuning_index][string_index]);

        // Exibe a nota da corda atual
        sprintf(text[2], "Tune %c", notes[tuning_index][string_index]);

        // Captura áudio e processa FFT
        printf("Capturing audio...\n");
        sample(capture_buf);
        float freq = process_fft();
        printf("Detected frequency: %.2f Hz\n", freq);

        // Verifica se a corda está afinada
        check_tuning(freq);

        // Atualiza o texto com a frequência detectada
        sprintf(text[7], "heard %.2f Hz", freq); 
        printf("Printando no display\n");

        // Desenha cada linha do texto na tela
        int y = 0;
        for (uint i = 0; i < sizeof(text) / sizeof(text[0]); i++) {
            ssd1306_draw_string(ssd, 5, y, text[i]);
            y += 8;  // Ajusta o espaçamento entre as linhas
        }

        // Atualiza o display com o conteúdo desenhado
        render_on_display(ssd, &frame_area);

        // Verifica se a corda mudou
        if (string_index != last_string_index) {
            // Toca a frequência da corda atual no buzzer por 2 segundos
            play_frequency(BUZZER_PIN, tunings[tuning_index][string_index], 2000);

            // Atualiza o último índice de corda
            last_string_index = string_index;
        }

        sleep_ms(200);  // Aguarda um pouco antes da próxima leitura
    }
}
// Loop principal do afinador
void tuner_loop() {
    struct render_area frame_area = {
        start_column: 0,
        end_column: ssd1306_width - 1,
        start_page: 0,
        end_page: ssd1306_n_pages - 1
    };
    calculate_render_area_buffer_length(&frame_area);

    // Exibe a mensagem de boas-vindas
    oled_display_welcome(frame_area);
    sleep_ms(5000);

    // Exibe o menu de seleção de afinação
    while (gpio_get(BTN_LEFT) == 1) {
        tuning_index = oled_display_menu(frame_area);
        sleep_ms(100);  // Debounce delay
    }
    printf("Selected tuning: %d\n", tuning_index);

    // Limpa o display
    memset(ssd, 0, ssd1306_buffer_length);
    render_on_display(ssd, &frame_area);

    // Inicia o afinador
    printf("Starting tuner...\n");

    printf("Looping...\n");
    tuner(frame_area);
}

int main() {
    // Configura o hardware
    setup();

    // Inicia o afinador
    tuner_loop();
    
    return 0;
}