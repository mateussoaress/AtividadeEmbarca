#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/pio.h" 
#include "pico/multicore.h"
#include "hardware/timer.h"
#include "hardware/clocks.h"
#include "pio_matrix.pio.h"  

// Definição dos GPIOs
#define LED_AZUL 11
#define LED_VERDE 12
#define LED_VERMELHO 13
#define BOTAO_A 5
#define BOTAO_B 6
#define MATRIZ_LED 7

// Variáveis globais
volatile int numero_atual = 0;  // Número exibido na matriz
PIO pio = pio0;
int maquina_estado = 0;

// Protótipos das funções
void configurar();
void tratar_interrupcao_botao(uint gpio, uint32_t eventos);
void debounce(uint gpio);
void exibir_numero(int numero);
void piscar_led_vermelho();
uint32_t ajustar_brilho(uint32_t cor);

// Matriz com os padrões dos números (corrigida para exibição correta)
const uint32_t numeros[10][25] = {
    { // 0
      1, 1, 1, 1, 1,
      1, 0, 0, 0, 1,
      1, 0, 0, 0, 1,
      1, 0, 0, 0, 1,
      1, 1, 1, 1, 1 },
    { // 1
      0, 0, 1, 0, 0,
      0, 0, 1, 1, 0,
      0, 0, 1, 0, 0,
      0, 0, 1, 0, 0,
      0, 0, 1, 0, 0 },
    { // 2
      1, 1, 1, 1, 1,
      1, 0, 0, 0, 0,
      1, 1, 1, 1, 1,
      0, 0, 0, 0, 1,
      1, 1, 1, 1, 1 },
    { // 3
      1, 1, 1, 1, 1,
      1, 0, 0, 0, 0,
      1, 1, 1, 1, 1,
      1, 0, 0, 0, 0,
      1, 1, 1, 1, 1 },
    { // 4 (espelhado)
      1, 0, 0, 0, 1,
      1, 0, 0, 0, 1,
      1, 1, 1, 1, 1,
      1, 0, 0, 0, 0,
      0, 0, 0, 0, 1 },
    { // 5
      1, 1, 1, 1, 1,
      0, 0, 0, 0, 1,
      1, 1, 1, 1, 1,
      1, 0, 0, 0, 0,
      1, 1, 1, 1, 1 },
    { // 6
      1, 1, 1, 1, 1,
      0, 0, 0, 0, 1,
      1, 1, 1, 1, 1,
      1, 0, 0, 0, 1,
      1, 1, 1, 1, 1 },
    { // 7
      1, 1, 1, 1, 1,
      1, 0, 0, 0, 0,
      0, 0, 0, 1, 0,
      0, 0, 1, 0, 0,
      0, 0, 1, 0, 0 },
    { // 8
      1, 1, 1, 1, 1,
      1, 0, 0, 0, 1,
      1, 1, 1, 1, 1,
      1, 0, 0, 0, 1,
      1, 1, 1, 1, 1 },
    { // 9
      1, 1, 1, 1, 1,
      1, 0, 0, 0, 1,
      1, 1, 1, 1, 1,
      1, 0, 0, 0, 0,
      1, 1, 1, 1, 1 }
};

// Configuração inicial
void configurar() {
    stdio_init_all();  

    gpio_init(LED_VERMELHO);
    gpio_set_dir(LED_VERMELHO, GPIO_OUT);

    gpio_init(BOTAO_A);
    gpio_set_dir(BOTAO_A, GPIO_IN);
    gpio_pull_up(BOTAO_A);
    
    gpio_init(BOTAO_B);
    gpio_set_dir(BOTAO_B, GPIO_IN);
    gpio_pull_up(BOTAO_B);
    
    gpio_set_irq_enabled_with_callback(BOTAO_A, GPIO_IRQ_EDGE_FALL, true, &tratar_interrupcao_botao);
    gpio_set_irq_enabled_with_callback(BOTAO_B, GPIO_IRQ_EDGE_FALL, true, &tratar_interrupcao_botao);

    uint deslocamento = pio_add_program(pio, &pio_matrix_program);
    pio_matrix_program_init(pio, maquina_estado, deslocamento, MATRIZ_LED);
}

void tratar_interrupcao_botao(uint gpio, uint32_t eventos) {
    debounce(gpio);

    if (gpio == BOTAO_A) {
        numero_atual = (numero_atual + 1 + 10) % 10;
    } else if (gpio == BOTAO_B) {
        numero_atual = (numero_atual - 1 + 10) % 10;
    }

    exibir_numero(numero_atual);
}

// Função de debounce
void debounce(uint gpio) {
    static uint32_t ultimo_tempo_A = 0;
    static uint32_t ultimo_tempo_B = 0;
    uint32_t tempo_atual = to_ms_since_boot(get_absolute_time());

    if (gpio == BOTAO_A) {
        if (tempo_atual - ultimo_tempo_A < 100) return;
        ultimo_tempo_A = tempo_atual;
    } else if (gpio == BOTAO_B) {
        if (tempo_atual - ultimo_tempo_B < 100) return;
        ultimo_tempo_B = tempo_atual;
    }
}

void exibir_numero(int numero) {
    float fator_brilho = 0.1; 

    uint32_t cores[10] = {
        0xFF0000, 0x00FF00, 0x00FFFF, 0xFFFF00, 
        0xFF00FF, 0x00FFFF, 0xFFA500, 0x8A2BE2, 
        0xFFFFFF, 0x808080 
    };

    uint8_t r = ((cores[numero] >> 16) & 0xFF) * fator_brilho;
    uint8_t g = ((cores[numero] >> 8) & 0xFF) * fator_brilho;
    uint8_t b = (cores[numero] & 0xFF) * fator_brilho;

    uint32_t cor = (r << 16) | (g << 8) | b; 

    for (int i = 0; i < 25; i++) {
        uint32_t cor_pixel = numeros[numero][24 - i] ? cor : 0x000000;
        pio_sm_put_blocking(pio, maquina_estado, cor_pixel);
    }
}

// Função para piscar o LED vermelho em outro núcleo
void piscar_led_vermelho() {
    while (true) {
        gpio_put(LED_VERMELHO, 1);
        sleep_ms(100);
        gpio_put(LED_VERMELHO, 0);
        sleep_ms(100);
    }
}

// Função principal
int main() {
    configurar();
    exibir_numero(0); 

    multicore_launch_core1(piscar_led_vermelho);

    while (true) {
        sleep_ms(100);
    }
}
