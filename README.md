# Projeto Final da Embarcatech

## Afinador de Guitarra com Raspberry Pi Pico

Este projeto é um afinador de guitarra baseado no Raspberry Pi Pico. Ele utiliza o microfone integrado na placa da BitDogLab para capturar o som das cordas, processa o sinal de áudio usando a Transformada Rápida de Fourier (FFT) para detectar a frequência fundamental e compara essa frequência com as frequências alvo de cada corda. O sistema fornece feedback visual através de LEDs e um display OLED, além de feedback sonoro com um buzzer.

## Funcionalidades

- **Detecção de Frequência:** Usa FFT para detectar a frequência fundamental do som capturado.
- **Feedback Visual:** LEDs indicam se a corda está afinada, muito apertada ou muito solta.
- **Feedback Sonoro:** Buzzer toca a frequência alvo da corda selecionada.
- **Interface do Usuário:** Display OLED exibe informações sobre a afinação e a frequência detectada.
- **Seleção de Afinação:** Permite escolher entre diferentes afinações (E-Standard, Drop D, Drop C).

## Componentes Utilizados

- **Raspberry Pi Pico**
- **Microfone (conectado ao ADC)**
- **Display OLED (comunicação I2C)**
- **LEDs (verde, vermelho, azul)**
- **Buzzer (PWM)**
- **Botões para seleção de afinação e corda**

## Esquema de Conexões

| Componente       | Pino no RP2040 |
|------------------|----------------|
| Microfone        | ADC2 (GP28)    |
| Display OLED SDA | GP14           |
| Display OLED SCL | GP15           |
| LED Verde        | GP11           |
| LED Vermelho     | GP13           |
| LED Azul         | GP12           |
| Buzzer           | GP10           |
| Botão Esquerdo   | GP5            |
| Botão Direito    | GP6            |

## Como Funciona

1. **Tela de Menu** Escolha a afinação desejada (E-Standard, Drop D, Drop C) usando o Botão B para navegar e o botão A para selecionar.
2. **Buzzer de acessibilidade será ligado com a freuqência da nota da corda**: Para cada corda haverá um buzzer inicial indicando a nota a ser afinada e um buzzer final indicando que a corda foi afinada
3. **Afinar a Corda:** Toque a corda do instrumento e observe os LEDs e o display OLED para ajustar a afinação (vermelho indica muita tensão, azul indica pouca tensão e verde indica ideal)
4. **Percorre as cordas da sexta até a primeira**: ao afinar basta clicar e segurar o botão B para repetir o processo com outra corda.

## Estrutura do Código

- **`main.c`:** Contém a lógica principal do afinador.
- **`ssd1306.c` e `ssd1306.h`:** Biblioteca para controlar o display OLED.
- **`kiss_fft.c` e `kiss_fft.h`:** Biblioteca para realizar a FFT.
- **`pico_sdk_import.cmake` e `CMakeLists.txt`:** Configurações para compilar o projeto com o SDK do Raspberry Pi Pico.

## Dependências

- **Pico SDK:** Necessário para compilar o projeto.
- **Kiss FFT:** Biblioteca para realizar a Transformada Rápida de Fourier.
- **SSD1306:** Biblioteca para controlar o display OLED.

## Como Compilar

1. Instale o [Pico SDK](https://github.com/raspberrypi/pico-sdk).
2. Clone este repositório
3. Entre na pasta build
4. Adicione o arquivo '.uf2' na placa em modo bootsel.
